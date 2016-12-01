#include "experimental_learner.h"

#include <array>
#include <ctime>
#include <direct.h>
#include <experimental/generator>
#include <fstream>
#include <numeric>
#include <omp.h>

#include "kifu_reader.h"
#include "position.h"
#include "search.h"
#include "thread.h"

namespace Learner
{
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
  std::pair<Value, std::vector<Move> > qsearch(Position& pos, Value alpha, Value beta);
}

namespace Eval
{
  typedef std::array<int16_t, 2> ValueKpp;
  typedef std::array<int32_t, 2> ValueKkp;
  typedef std::array<int32_t, 2> ValueKk;
  // FV_SCALEで割る前の値が入る
  extern ALIGNED(32) ValueKk kk[SQ_NB][SQ_NB];
  extern ALIGNED(32) ValueKpp kpp[SQ_NB][fe_end][fe_end];
  extern ALIGNED(32) ValueKkp kkp[SQ_NB][SQ_NB][fe_end];
  extern const int FV_SCALE = 32;

  void save_eval();
  void eval_learn_init();
  extern BonaPiece inv_piece[fe_end];
  extern BonaPiece mir_piece[fe_end];
}

using Eval::BonaPiece;
using Eval::fe_end;
using Eval::inv_piece;
using Eval::mir_piece;
using USI::Option;
using USI::OptionsMap;

namespace
{
  using WeightType = double;

  enum WeightKind {
    WEIGHT_KIND_COLOR,
    WEIGHT_KIND_TURN,
    WEIGHT_KIND_ZERO = 0,
    WEIGHT_KIND_NB = 2,
  };
  ENABLE_FULL_OPERATORS_ON(WeightKind);

  struct Weight
  {
    // 勾配の和
    WeightType sum_gradient;
    // 重み
    WeightType w;
    // Adam用変数
    WeightType m;
    WeightType v;

    void AddGradient(double gradient);
    template<typename T>
    void UpdateWeight(
      double adam_beta1_t, double adam_beta2_t, double learning_rate, T& eval_weight);
    template<typename T>
    void Finalize(int num_mini_batches, T& eval_weight);
  };

  constexpr int kFvScale = 32;
  constexpr WeightType kEps = 1e-8;
  constexpr WeightType kAdamBeta1 = 0.9;
  constexpr WeightType kAdamBeta2 = 0.999;
  constexpr int kMaxGamePlay = 256;
  constexpr int64_t kMaxPositionsForErrorMeasurement = 1000'0000LL;
  constexpr int64_t kMaxPositionsForBenchmark = 1'0000'0000LL;

  constexpr char* kOptionValueLearnerNumPositions = "LearnerNumPositions";
  constexpr char* kOptionValueLearnerPvStrapMaxDepth = "LearnerPvStrapMaxDepth";
  constexpr char* kOptionValueLearningRate = "LearningRate";
  constexpr char* kOptionValueLearningRateDecayRate = "LearningRateDecayRate";
  constexpr char* kOptionValueNumPositionsToDecayLearningRate = "NumPositionsToDecayLearningRate";
  constexpr char* kOptionValueKifuForTestDir = "KifuForTestDir";
  constexpr char* kOptionValueLearnerNumPositionsForTest = "LearnerNumPositionsForTest";
  constexpr char* kOptionValueMiniBatchSize = "MiniBatchSize";

  class Kpp {
  public:
    Kpp(Square king, BonaPiece piece0, BonaPiece piece1, WeightKind weight_kind)
      : king_(king), piece0_(piece0), piece1_(piece1), weight_kind_(weight_kind) {
    }

    // Inclusive
    static int MinIndex() {
      return 0;
    }

    // Exclusive
    static int MaxIndex() {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      return k * p * p * w;
    }

    static bool IsValid(int dimension) {
      return MinIndex() <= dimension && dimension < MaxIndex();
    }

    int ToIndex() const {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      int index = ((king_ * p + piece0_) * p + piece1_) * w + weight_kind_;
      ASSERT_LV3(IsValid(index));
      return index;
    }

    auto ToLowerDimensions() const {
      yield Kpp(king_, piece0_, piece1_, weight_kind_);
      yield Kpp(king_, piece1_, piece0_, weight_kind_);
      yield Kpp(Mir(king_), mir_piece[piece0_], mir_piece[piece1_], weight_kind_);
      yield Kpp(Mir(king_), mir_piece[piece1_], mir_piece[piece0_], weight_kind_);
    }

    int ToNormalizedIndex() const {
      int best_index = std::numeric_limits<int>::max();
      for (const auto& kpp : ToLowerDimensions()) {
        best_index = std::min(best_index, kpp.ToIndex());
      }
      return best_index;
    }

    static Kpp ForIndex(int index) {
      int original_index = index;
      ASSERT_LV3(IsValid(index));
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      index -= MinIndex();
      int weight_kind = index % w;
      index /= w;
      int piece1 = index % p;
      index /= p;
      int piece0 = index % p;
      index /= p;
      int king = index;
      ASSERT_LV3(king < k);
      Kpp kpp(static_cast<Square>(king), static_cast<BonaPiece>(piece0),
        static_cast<BonaPiece>(piece1), static_cast<WeightKind>(weight_kind));
      ASSERT_LV3(kpp.ToIndex() == original_index);
      return kpp;
    }

    Square king() const { return king_; }
    BonaPiece piece0() const { return piece0_; }
    BonaPiece piece1() const { return piece1_; }
    WeightKind weight_kind() const { return weight_kind_; }

  private:
    Square king_;
    BonaPiece piece0_;
    BonaPiece piece1_;
    WeightKind weight_kind_;
  };

  class Kkp {
  public:
    Kkp(Square king0, Square king1, BonaPiece piece, WeightKind weight_kind)
      : king0_(king0), king1_(king1), piece_(piece), weight_kind_(weight_kind) {
    }

    // Inclusive
    static int MinIndex() {
      return Kpp::MaxIndex();
    }

    // Exclusive
    static int MaxIndex() {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      return MinIndex() + k * k * p * w;
    }

    static bool IsValid(int dimension) {
      return MinIndex() <= dimension && dimension < MaxIndex();
    }

    int ToIndex() const {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      int index = MinIndex() + ((king0_ * k + king1_) * p + piece_) * w + weight_kind_;
      ASSERT_LV3(IsValid(index));
      return index;
    }

    auto ToLowerDimensions() const {
      yield Kkp(king0_, king1_, piece_, weight_kind_);
      yield Kkp(Mir(king0_), Mir(king1_), mir_piece[piece_], weight_kind_);
    }

    int ToNormalizedIndex() const {
      int best_index = std::numeric_limits<int>::max();
      for (const auto& kpp : ToLowerDimensions()) {
        best_index = std::min(best_index, kpp.ToIndex());
      }
      return best_index;
    }

    static Kkp ForIndex(int index) {
      int original_index = index;
      ASSERT_LV3(IsValid(index));
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      index -= MinIndex();
      int weight_kind = index % w;
      index /= w;
      int piece = index % p;
      index /= p;
      int king1 = index % k;
      index /= k;
      int king0 = index;
      ASSERT_LV3(king0 < k);
      Kkp kkp(static_cast<Square>(king0), static_cast<Square>(king1),
        static_cast<BonaPiece>(piece), static_cast<WeightKind>(weight_kind));
      ASSERT_LV3(kkp.ToIndex() == original_index);
      return kkp;
    }

    Square king0() const { return king0_; }
    Square king1() const { return king1_; }
    BonaPiece piece() const { return piece_; }
    WeightKind weight_kind() const { return weight_kind_; }

  private:
    Square king0_;
    Square king1_;
    BonaPiece piece_;
    WeightKind weight_kind_;
  };

  class Kk {
  public:
    Kk(Square king0, Square king1, WeightKind weight_kind)
      : king0_(king0), king1_(king1), weight_kind_(weight_kind) {
    }

    // Inclusive
    static int MinIndex() {
      return Kkp::MaxIndex();
    }

    // Exclusive
    static int MaxIndex() {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      return MinIndex() + k * k * w;
    }

    static bool IsValid(int dimension) {
      return MinIndex() <= dimension && dimension < MaxIndex();
    }

    int ToIndex() const {
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      int index = MinIndex() + (king0_ * k + king1_) * w + weight_kind_;
      ASSERT_LV3(IsValid(index));
      return index;
    }

    auto ToLowerDimensions() const {
      yield Kk(king0_, king1_, weight_kind_);
    }

    int ToNormalizedIndex() const {
      int best_index = std::numeric_limits<int>::max();
      for (const auto& kpp : ToLowerDimensions()) {
        best_index = std::min(best_index, kpp.ToIndex());
      }
      return best_index;
    }

    static Kk ForIndex(int index) {
      int original_index = index;
      ASSERT_LV3(IsValid(index));
      constexpr int k = SQ_NB;
      constexpr int p = fe_end;
      constexpr int w = WEIGHT_KIND_NB;
      index -= MinIndex();
      int weight_kind = index % w;
      index /= w;
      int king1 = index % k;
      index /= k;
      int king0 = index;
      ASSERT_LV3(king0 < k);
      Kk kk(static_cast<Square>(king0), static_cast<Square>(king1),
        static_cast<WeightKind>(weight_kind));
      ASSERT_LV3(kk.ToIndex() == original_index);
      return kk;
    }

    Square king0() const { return king0_; }
    Square king1() const { return king1_; }
    WeightKind weight_kind() const { return weight_kind_; }

  private:
    Square king0_;
    Square king1_;
    WeightKind weight_kind_;
  };

  void Weight::AddGradient(double gradient) {
    sum_gradient += gradient;
  }

  template<typename T>
  void Weight::UpdateWeight(double adam_beta1_t, double adam_beta2_t, double learning_rate, T& eval_weight) {
    // Adam
    m = kAdamBeta1 * m + (1.0 - kAdamBeta1) * sum_gradient;
    v = kAdamBeta2 * v + (1.0 - kAdamBeta2) * sum_gradient * sum_gradient;
    // 高速化のためpow(ADAM_BETA1, t)の値を保持しておく
    WeightType mm = m / (1.0 - adam_beta1_t);
    WeightType vv = v / (1.0 - adam_beta2_t);
    WeightType delta = learning_rate * mm / (std::sqrt(vv) + kEps);
    w -= delta;

    // 重みテーブルに書き戻す
    eval_weight = static_cast<T>(std::round(w));

    sum_gradient = 0.0;
  }

  template<typename T>
  void Weight::Finalize(int num_mini_batches, T& eval_weight)
  {
    int64_t value = static_cast<int64_t>(std::round(w));
    value = std::max<int64_t>(std::numeric_limits<T>::min(), value);
    value = std::min<int64_t>(std::numeric_limits<T>::max(), value);
    eval_weight = static_cast<T>(value);
  }

  double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
  }

  double winning_percentage(Value value) {
    return sigmoid(static_cast<int>(value) / 600.0);
  }

  double dsigmoid(double x) {
    return sigmoid(x) * (1.0 - sigmoid(x));
  }

  std::string GetDateTimeString() {
    time_t time = std::time(nullptr);
    struct tm *tm = std::localtime(&time);
    char buffer[1024];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d-%H-%M-%S", tm);
    return buffer;
  }

  void save_eval(const std::string& output_folder_path_base, int64_t position_index) {
    sync_cout << "Creating a folder: output_folder_path_base=" << output_folder_path_base << sync_endl;
    _mkdir(output_folder_path_base.c_str());

    char buffer[1024];
    std::sprintf(buffer, "%s/%I64d", output_folder_path_base.c_str(), position_index);
    std::printf("Writing eval files: %s\n", buffer);
    std::fflush(stdout);
    Options["EvalDir"] = buffer;
    sync_cout << "Creating a folder: buffer=" << buffer << sync_endl;
    _mkdir(buffer);
    Eval::save_eval();
  }

  // 浅い探索付きの*Strapを行う
  void Strap(const Learner::Record& record, int pv_strap_max_depth,
    std::function<void(Value record_value, Value value, Color root_color, Position& pos)> f) {
    int thread_index = omp_get_thread_num();
    Thread& thread = *Threads[thread_index];
    Position& pos = thread.rootPos;
    pos.set_from_packed_sfen(record.packed);
    pos.set_this_thread(&thread);
    if (!pos.pos_is_ok()) {
      sync_cout << "Position is not ok! Exiting..." << sync_endl;
      sync_cout << pos << sync_endl;
      std::exit(1);
    }

    Color root_color = pos.side_to_move();

    // 0手読み+静止探索を行う
    auto value_and_pv = Learner::qsearch(pos, -VALUE_INFINITE, VALUE_INFINITE);

    // Eval::evaluate()を使うと差分計算のおかげで少し速くなるはず
    // 全計算はPosition::set()の中で行われているので差分計算ができる
    Value value = Eval::evaluate(pos);
    StateInfo stateInfo[MAX_PLY];
    std::vector<Move>& pv = value_and_pv.second;
    for (int shallow_search_pv_play_index = 0;
      shallow_search_pv_play_index < static_cast<int>(pv.size());
      ++shallow_search_pv_play_index) {
      pos.do_move(pv[shallow_search_pv_play_index], stateInfo[shallow_search_pv_play_index]);
      value = Eval::evaluate(pos);
    }

    // Eval::evaluate()は常に手番から見た評価値を返すので
    // 探索開始局面と手番が違う場合は符号を反転する
    if (root_color != pos.side_to_move()) {
      value = -value;
    }

    f(static_cast<Value>(record.value), value, root_color, pos);

    // 局面を浅い探索のroot局面にもどす
    for (auto rit = pv.rbegin(); rit != pv.rend(); ++rit) {
      pos.undo_move(*rit);
    }
  }
}

void Learner::InitializeLearner(USI::OptionsMap& o) {
  o[kOptionValueLearnerNumPositions] << Option("10000000000");
  o[kOptionValueLearnerPvStrapMaxDepth] << Option(0, 0, MAX_PLY);
  o[kOptionValueLearningRate] << Option("1.0");
  o[kOptionValueNumPositionsToDecayLearningRate] << Option("1000000000");
  o[kOptionValueLearningRateDecayRate] << Option("1.0");
  o[kOptionValueKifuForTestDir] << Option("kifu_for_test");
  o[kOptionValueLearnerNumPositionsForTest] << Option("1000000");
  o[kOptionValueMiniBatchSize] << Option("1000000");
}

void Learner::ShowProgress(const time_t& start_time, int64_t current_data, int64_t total_data,
  int64_t show_per) {
  if (!current_data || current_data % show_per) {
    return;
  }
  time_t current_time;
  std::time(&current_time);
  if (start_time == current_time) {
    return;
  }

  time_t elapsed_time = current_time - start_time;
  int hour = static_cast<int>(elapsed_time / 3600);
  int minute = elapsed_time / 60 % 60;
  int second = elapsed_time % 60;
  double data_per_time = current_data / static_cast<double>(current_time - start_time);
  time_t expected_time =
    static_cast<time_t>(current_time + (total_data - current_data) / data_per_time);
  struct tm current_tm = *std::localtime(&current_time);
  struct tm expected_tm = *std::localtime(&expected_time);
  std::printf(
    "%lld / %lld\n"
    "        elapsed time = %02d:%02d:%02d\n"
    "   current date time = %04d-%02d-%02d %02d:%02d:%02d\n"
    "    finish date time = %04d-%02d-%02d %02d:%02d:%02d\n",
    current_data, total_data,
    hour, minute, second,
    current_tm.tm_year + 1900, current_tm.tm_mon + 1, current_tm.tm_mday, current_tm.tm_hour, current_tm.tm_min, current_tm.tm_sec,
    expected_tm.tm_year + 1900, expected_tm.tm_mon + 1, expected_tm.tm_mday, expected_tm.tm_hour, expected_tm.tm_min, expected_tm.tm_sec);
  std::fflush(stdout);
}

void Learner::Learn(std::istringstream& iss) {
  sync_cout << "Learner::Learn()" << sync_endl;

  Eval::eval_learn_init();

  ASSERT_LV3(
    Kk::MaxIndex() == 
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);
#ifndef USE_FALSE_PROBE_IN_TT
  sync_cout << "Please define USE_FALSE_PROBE_IN_TT." << sync_endl;
  ASSERT_LV3(false)
#endif

    Eval::eval_learn_init();

  omp_set_num_threads((int)Options["Threads"]);

  std::string output_folder_path_base = "learner_output/" + GetDateTimeString();
  std::string token;
  while (iss >> token) {
    if (token == "output_folder_path_base") {
      iss >> output_folder_path_base;
    }
  }

  std::vector<int64_t> write_eval_per_positions;
  for (int64_t base = 1; base <= 9; ++base) {
    write_eval_per_positions.push_back(base * 10'0000'0000LL);
    write_eval_per_positions.push_back(base * 100'0000'0000LL);
  }
  write_eval_per_positions.push_back(std::numeric_limits<int64_t>::max());
  std::sort(write_eval_per_positions.begin(), write_eval_per_positions.end());
  int write_eval_per_positions_index = 0;

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  auto kifu_reader = std::make_unique<Learner::KifuReader>((std::string)Options["KifuDir"], true);

  Eval::load_eval();

  int vector_length = Kk::MaxIndex();

  std::vector<Weight> weights(vector_length);
  memset(&weights[0], 0, sizeof(weights[0]) * weights.size());

  // 評価関数テーブルから重みベクトルへ重みをコピーする
  // 並列化を効かせたいのでdimension_indexで回す
#pragma omp parallel for schedule(guided)
  for (int dimension_index = 0; dimension_index < vector_length; ++dimension_index) {
    if (Kpp::IsValid(dimension_index)) {
      Kpp kpp = Kpp::ForIndex(dimension_index);
      weights[dimension_index].w = static_cast<WeightType>(
        Eval::kpp[kpp.king()][kpp.piece0()][kpp.piece1()][kpp.weight_kind()]);

    }
    else if (Kkp::IsValid(dimension_index)) {
      Kkp kkp = Kkp::ForIndex(dimension_index);
      weights[dimension_index].w = static_cast<WeightType>(
        Eval::kkp[kkp.king0()][kkp.king1()][kkp.piece()][kkp.weight_kind()]);

    }
    else if (Kk::IsValid(dimension_index)) {
      Kk kk = Kk::ForIndex(dimension_index);
      weights[dimension_index].w = static_cast<WeightType>(
        Eval::kk[kk.king0()][kk.king1()][kk.weight_kind()]);

    }
    else {
      ASSERT_LV3(false);
    }
  }

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  // 作成・破棄のコストが高いためループの外に宣言する
  std::vector<Record> records;

  // 全学習データに対してループを回す
  time_t start;
  std::time(&start);
  int num_mini_batches = 0;
  double learning_rate = Options[kOptionValueLearningRate].cast<double>();
  double learning_rate_decay_rate = Options[kOptionValueLearningRateDecayRate].cast<double>();
  int64_t num_positions_to_decay_learning_rate =
    Options[kOptionValueNumPositionsToDecayLearningRate].cast<int64_t>();
  int64_t next_positionsto_decay_learning_rate = num_positions_to_decay_learning_rate;
  int64_t max_positions_for_learning = Options[kOptionValueLearnerNumPositions].cast<int64_t>();
  int pv_strap_max_depth = Options[kOptionValueLearnerPvStrapMaxDepth];
  std::string kifu_for_test_dir = Options[kOptionValueKifuForTestDir];
  int64_t num_positions_for_test = Options[kOptionValueLearnerNumPositionsForTest].cast<int64_t>();
  int64_t mini_batch_size = Options[kOptionValueMiniBatchSize].cast<int64_t>();

  sync_cout << "learning_rate=" << learning_rate << sync_endl;
  sync_cout << "learning_rate_decay_rate=" << learning_rate_decay_rate << sync_endl;
  sync_cout << "num_positions_to_decay_learning_rate=" << num_positions_to_decay_learning_rate
    << sync_endl;
  sync_cout << "max_positions_for_learning=" << max_positions_for_learning << sync_endl;
  sync_cout << "pv_strap_max_depth=" << pv_strap_max_depth << sync_endl;
  sync_cout << "kifu_for_test_dir=" << kifu_for_test_dir << sync_endl;
  sync_cout << "num_positions_for_test=" << num_positions_for_test << sync_endl;
  sync_cout << "mini_batch_size=" << mini_batch_size << sync_endl;

  auto kifu_reader_for_test = std::make_unique<Learner::KifuReader>(kifu_for_test_dir, false);
  std::vector<Record> records_for_test;
  if (!kifu_reader_for_test->Read(num_positions_for_test, records_for_test)) {
    sync_cout << "Failed to read kifu for test." << sync_endl;
    std::exit(1);
  }

  // 未学習の評価関数ファイルを出力しておく
  save_eval(output_folder_path_base, 0);

  // 損失関数ログの作成
  char train_loss_file_name[_MAX_PATH];
  std::sprintf(train_loss_file_name, "%s/loss_%I64d.csv", output_folder_path_base.c_str(),
    start);
  FILE* file_loss = std::fopen(train_loss_file_name, "w");
  std::fprintf(file_loss, ",train_rmse_value,train_rmse_winning_percentage,train_mean_cross_entropy,test_rmse_value,test_rmse_winning_percentage,test_mean_cross_entropy,norm\n");
  std::fflush(file_loss);

  for (int64_t num_processed_positions = 0; num_processed_positions < max_positions_for_learning;) {
    ShowProgress(start, num_processed_positions, max_positions_for_learning, mini_batch_size * 10);

    int num_records = static_cast<int>(std::min(
      max_positions_for_learning - num_processed_positions, mini_batch_size));
    if (!kifu_reader->Read(num_records, records)) {
      sync_cout << "Failed to read kifu for train." << sync_endl;
      std::exit(1);
    }
    ++num_mini_batches;

    double sum_train_squared_error_of_value = 0.0;
    double sum_norm = 0.0;
    double sum_train_squared_error_of_winning_percentage = 0.0;
    double sum_train_cross_entropy = 0.0;
    double sum_test_squared_error_of_value = 0.0;
    double sum_test_squared_error_of_winning_percentage = 0.0;
    double sum_test_cross_entropy = 0.0;

#pragma omp parallel
    {
      int thread_index = omp_get_thread_num();
      // ミニバッチ
      // num_records個の学習データの勾配の和を求めて重みを更新する
#pragma omp for schedule(guided) reduction(+:sum_train_squared_error_of_value) reduction(+:sum_norm) reduction(+:sum_train_squared_error_of_winning_percentage) reduction(+:sum_train_cross_entropy)
      for (int record_index = 0; record_index < num_records; ++record_index) {
        auto f = [&weights, &sum_train_squared_error_of_value, &sum_norm,
          &sum_train_squared_error_of_winning_percentage, &sum_train_cross_entropy](
            Value record_value, Value value, Color root_color, Position& pos) {
          // 評価値から推定した勝率の分布の交差エントロピー
          double p = winning_percentage(record_value);
          double q = winning_percentage(value);
          WeightType delta = q - p;

          double diff_value = record_value - value;
          sum_train_squared_error_of_value += diff_value * diff_value;
          double diff_winning_percentage = p - q;
          sum_train_squared_error_of_winning_percentage +=
            diff_winning_percentage * diff_winning_percentage;
          sum_train_cross_entropy +=
            (-p * std::log(q + kEps) - (1.0 - p) * std::log(1.0 - q + kEps));
          sum_norm += abs(value);

          // 先手から見た評価値の差分。sum.p[?][0]に足したり引いたりする。
          WeightType delta_color = (root_color == BLACK ? delta : -delta);
          // 手番から見た評価値の差分。sum.p[?][1]に足したり引いたりする。
          WeightType delta_turn = (root_color == pos.side_to_move() ? delta : -delta);

          // 値を更新する
          Square sq_bk = pos.king_square(BLACK);
          Square sq_wk = pos.king_square(WHITE);
          const auto& list0 = pos.eval_list()->piece_list_fb();
          const auto& list1 = pos.eval_list()->piece_list_fw();

          // 勾配の値を加算する

          // KK
          weights[Kk(sq_bk, sq_wk, WEIGHT_KIND_COLOR).ToNormalizedIndex()].AddGradient(delta_color);
          weights[Kk(sq_bk, sq_wk, WEIGHT_KIND_TURN).ToNormalizedIndex()].AddGradient(delta_turn);

          for (int i = 0; i < PIECE_NO_KING; ++i) {
            Eval::BonaPiece k0 = list0[i];
            Eval::BonaPiece k1 = list1[i];
            for (int j = 0; j < i; ++j) {
              Eval::BonaPiece l0 = list0[j];
              Eval::BonaPiece l1 = list1[j];

              // 常にp0 < p1となるようにアクセスする

              // KPP
              Eval::BonaPiece p0b = std::min(k0, l0);
              Eval::BonaPiece p1b = std::max(k0, l0);
              weights[Kpp(sq_bk, p0b, p1b, WEIGHT_KIND_COLOR).ToNormalizedIndex()]
                .AddGradient(delta_color);
              weights[Kpp(sq_bk, p0b, p1b, WEIGHT_KIND_TURN).ToNormalizedIndex()]
                .AddGradient(delta_turn);

              // KPP
              Eval::BonaPiece p0w = std::min(k1, l1);
              Eval::BonaPiece p1w = std::max(k1, l1);
              weights[Kpp(Inv(sq_wk), p0w, p1w, WEIGHT_KIND_COLOR).ToNormalizedIndex()]
                .AddGradient(-delta_color);
              weights[Kpp(Inv(sq_wk), p0w, p1w, WEIGHT_KIND_TURN).ToNormalizedIndex()]
                .AddGradient(delta_turn);
            }

            // KKP
            weights[Kkp(sq_bk, sq_wk, k0, WEIGHT_KIND_COLOR).ToNormalizedIndex()]
              .AddGradient(delta_color);
            weights[Kkp(sq_bk, sq_wk, k0, WEIGHT_KIND_TURN).ToNormalizedIndex()]
              .AddGradient(delta_turn);
          }
        };
        Strap(records[record_index], pv_strap_max_depth, f);
      }

      // 損失関数を計算する
#pragma omp for schedule(guided) reduction(+:sum_test_squared_error_of_value) reduction(+:sum_test_squared_error_of_winning_percentage) reduction(+:sum_test_cross_entropy)
      for (int record_index = 0; record_index < num_records; ++record_index) {
        auto f = [&weights, &sum_test_squared_error_of_value,
          &sum_test_squared_error_of_winning_percentage, &sum_test_cross_entropy](
            Value record_value, Value value, Color root_color, Position& pos) {
          // 評価値から推定した勝率の分布の交差エントロピー
          double p = winning_percentage(record_value);
          double q = winning_percentage(value);
          WeightType delta = q - p;

          double diff_value = record_value - value;
          sum_test_squared_error_of_value += diff_value * diff_value;
          double diff_winning_percentage = p - q;
          sum_test_squared_error_of_winning_percentage +=
            diff_winning_percentage * diff_winning_percentage;
          sum_test_cross_entropy +=
            (-p * std::log(q + kEps) - (1.0 - p) * std::log(1.0 - q + kEps));
        };
        Strap(records_for_test[record_index], pv_strap_max_depth, f);
      }

      // 重みを更新する
      double adam_beta1_t = std::pow(kAdamBeta1, num_mini_batches);
      double adam_beta2_t = std::pow(kAdamBeta2, num_mini_batches);

      // 並列化を効かせたいのでdimension_indexで回す
#pragma omp for schedule(guided)
      for (int dimension_index = 0; dimension_index < vector_length; ++dimension_index) {
        if (Kpp::IsValid(dimension_index)) {
          Kpp kpp = Kpp::ForIndex(dimension_index);
          int normalized_index = kpp.ToNormalizedIndex();
          if (normalized_index != dimension_index) {
            continue;
          }
          auto& weight = weights[normalized_index];
          int16_t value = 0;
          weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate, value);
          for (const auto& lower : kpp.ToLowerDimensions()) {
            Eval::kpp[lower.king()][lower.piece0()][lower.piece1()][lower.weight_kind()] = value;
          }
        }
        else if (Kkp::IsValid(dimension_index)) {
          Kkp kkp = Kkp::ForIndex(dimension_index);
          int normalized_index = kkp.ToNormalizedIndex();
          if (normalized_index != dimension_index) {
            continue;
          }
          auto& weight = weights[normalized_index];
          int16_t value = 0;
          weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate, value);
          for (const auto& lower : kkp.ToLowerDimensions()) {
            Eval::kkp[lower.king0()][lower.king1()][lower.piece()][lower.weight_kind()] = value;
          }
        }
        else if (Kk::IsValid(dimension_index)) {
          Kk kk = Kk::ForIndex(dimension_index);
          int normalized_index = kk.ToNormalizedIndex();
          if (normalized_index != dimension_index) {
            continue;
          }
          auto& weight = weights[normalized_index];
          int16_t value = 0;
          weight.UpdateWeight(adam_beta1_t, adam_beta2_t, learning_rate, value);
          for (const auto& lower : kk.ToLowerDimensions()) {
            Eval::kk[lower.king0()][lower.king1()][lower.weight_kind()] = value;
          }
        }
        else {
          ASSERT_LV3(false);
        }
      }
    }

    // 損失関数の値を出力する
    fprintf(file_loss, "%I64d,%f,%f,%f,%f,%f,%f,%f\n", num_processed_positions,
      std::sqrt(sum_train_squared_error_of_value / num_records),
      std::sqrt(sum_train_squared_error_of_winning_percentage / num_records),
      sum_train_cross_entropy / num_records,
      std::sqrt(sum_test_squared_error_of_value / num_records),
      std::sqrt(sum_test_squared_error_of_winning_percentage / num_records),
      sum_test_cross_entropy / num_records,
      sum_norm / num_records);
    std::fflush(file_loss);

    num_processed_positions += num_records;

    // 評価関数ファイルの書き出し
    if (write_eval_per_positions[write_eval_per_positions_index] <= num_processed_positions) {
      save_eval(output_folder_path_base, num_processed_positions);
      while (write_eval_per_positions[write_eval_per_positions_index] <= num_processed_positions) {
        ++write_eval_per_positions_index;
        ASSERT_LV3(write_eval_per_positions_index < static_cast<int>(write_eval_per_positions.size()));
      }
    }

    // 学習率の減衰
    if (num_processed_positions >= next_positionsto_decay_learning_rate) {
      learning_rate *= learning_rate_decay_rate;
      next_positionsto_decay_learning_rate += num_positions_to_decay_learning_rate;
      std::printf("Decayed the learning rate: learning_rate=%f\n", learning_rate);
      std::fflush(stdout);
    }
  }

  sync_cout << "Finished..." << sync_endl;
}

void Learner::MeasureError() {
  sync_cout << "Learner::MeasureError()" << sync_endl;

  Eval::eval_learn_init();

  omp_set_num_threads((int)Options["Threads"]);

  ASSERT_LV3(
    Kk::MaxIndex() ==
    static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * static_cast<int>(Eval::fe_end) * WEIGHT_KIND_NB +
    static_cast<int>(SQ_NB) * static_cast<int>(SQ_NB) * WEIGHT_KIND_NB);

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  auto kifu_reader = std::make_unique<KifuReader>((std::string)Options["KifuDir"], false);
  int pv_strap_max_depth = Options[kOptionValueLearnerPvStrapMaxDepth];
  int64_t mini_batch_size = Options[kOptionValueMiniBatchSize].cast<int64_t>();;

  Eval::load_eval();

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = 1;
  limits.silent = true;
  Search::Limits = limits;

  // 作成・破棄のコストが高いためループの外に宣言する
  std::vector<Record> records;

  time_t start;
  std::time(&start);
  double sum_squared_error_of_value = 0.0;
  double sum_norm = 0.0;
  double sum_squared_error_of_winning_percentage = 0.0;
  double sum_cross_entropy = 0.0;
  for (int64_t num_processed_positions = 0; num_processed_positions < kMaxPositionsForErrorMeasurement;) {
    ShowProgress(start, num_processed_positions, kMaxPositionsForErrorMeasurement, mini_batch_size);

    int num_records = static_cast<int>(std::min(
      kMaxPositionsForErrorMeasurement - num_processed_positions, mini_batch_size));
    if (!kifu_reader->Read(num_records, records)) {
      break;
    }

    // ミニバッチ
#pragma omp parallel for reduction(+:sum_squared_error_of_value) reduction(+:sum_norm) reduction(+:sum_squared_error_of_winning_percentage) reduction(+:sum_cross_entropy) schedule(guided)
    for (int record_index = 0; record_index < num_records; ++record_index) {
      auto f = [&sum_squared_error_of_value, &sum_squared_error_of_winning_percentage,
        &sum_cross_entropy, &sum_norm](Value record_value, Value value, Color root_color,
          Position& pos) {
        double diff_value = record_value - value;
        sum_squared_error_of_value += diff_value * diff_value;
        double p = winning_percentage(record_value);
        double q = winning_percentage(value);
        double diff_winning_percentage = p - q;
        sum_squared_error_of_winning_percentage += diff_winning_percentage * diff_winning_percentage;
        sum_cross_entropy += (-p * std::log(q + kEps) - (1.0 - p) * std::log(1.0 - q + kEps));
        sum_norm += abs(value);
      };
      Strap(records[record_index], pv_strap_max_depth, f);
    }

    num_processed_positions += num_records;
  }

  std::printf(
    "info string rmse_value=%f rmse_winning_percentage=%f mean_cross_entropy=%f norm=%f\n",
    std::sqrt(sum_squared_error_of_value / kMaxPositionsForErrorMeasurement),
    std::sqrt(sum_squared_error_of_winning_percentage / kMaxPositionsForErrorMeasurement),
    sum_cross_entropy / kMaxPositionsForErrorMeasurement,
    sum_norm / kMaxPositionsForErrorMeasurement);
  std::fflush(stdout);
}

void Learner::BenchmarkKifuReader() {
  sync_cout << "Learner::BenchmarkKifuReader()" << sync_endl;

  Eval::eval_learn_init();

  omp_set_num_threads((int)Options["Threads"]);
  int64_t mini_batch_size = Options[kOptionValueMiniBatchSize].cast<int64_t>();

  time_t start;
  std::time(&start);

  sync_cout << "Initializing kifu reader..." << sync_endl;
  std::unique_ptr<Learner::KifuReader> kifu_reader =
    std::make_unique<Learner::KifuReader>((std::string)Options["KifuDir"], true);

  sync_cout << "Reading kifu..." << sync_endl;
  std::vector<Record> records;
  for (int64_t num_processed_positions = 0; num_processed_positions < kMaxPositionsForBenchmark;) {
    ShowProgress(start, num_processed_positions, kMaxPositionsForBenchmark, 100'0000LL);

    int num_records = static_cast<int>(std::min(
      kMaxPositionsForBenchmark - num_processed_positions, mini_batch_size));
    if (!kifu_reader->Read(num_records, records)) {
      break;
    }

    num_processed_positions += num_records;
  }
}
