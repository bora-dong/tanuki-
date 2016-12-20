#include "kifu_generator.h"

#include <atomic>
#include <ctime>
#include <direct.h>
#include <fstream>
#include <memory>
#include <omp.h>
#include <random>
#include <sstream>

#include "kifu_writer.h"
#include "search.h"
#include "thread.h"

using Search::RootMove;
using USI::Option;
using USI::OptionsMap;

namespace Learner
{
  std::pair<Value, std::vector<Move> > search(Position& pos, Value alpha, Value beta, int depth);
  std::pair<Value, std::vector<Move> > qsearch(Position& pos, Value alpha, Value beta);
}

namespace
{
  constexpr int kMaxGamePlay = 256;
  constexpr int kMaxSwapTrials = 10;
  constexpr int kMaxTrialsToSelectSquares = 100;

  constexpr char* OPTION_GENERATOR_NUM_POSITIONS = "GeneratorNumPositions";
  constexpr char* OPTION_GENERATOR_MIN_SEARCH_DEPTH = "GeneratorMinSearchDepth";
  constexpr char* OPTION_GENERATOR_MAX_SEARCH_DEPTH = "GeneratorMaxSearchDepth";
  constexpr char* OPTION_GENERATOR_KIFU_TAG = "GeneratorKifuTag";
  constexpr char* OPTION_GENERATOR_BOOK_FILE_NAME = "GeneratorStartposFileName";
  constexpr char* OPTION_GENERATOR_MIN_BOOK_MOVE = "GeneratorMinBookMove";
  constexpr char* OPTION_GENERATOR_MAX_BOOK_MOVE = "GeneratorMaxBookMove";
  constexpr char* OPTION_GENERATOR_VALUE_THRESHOLD = "GeneratorValueThreshold";
  constexpr char* OPTION_GENERATOR_DO_RANDOM_KING_MOVE_PROBABILITY = "GeneratorDoRandomKingMoveProbability";
  constexpr char* OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY = "GeneratorSwapTwoPiecesProbability";
  constexpr char* OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY = "GeneratorDoRandomMoveProbability";
  constexpr char* OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK = "GeneratorDoRandomMoveAfterBook";

  std::vector<std::string> book;
  std::uniform_real_distribution<> probability_distribution;

  bool ReadBook()
  {
    // ��Ճt�@�C��(�Ƃ������P�Ȃ�����t�@�C��)�̓ǂݍ���
    std::string book_file_name = Options[OPTION_GENERATOR_BOOK_FILE_NAME];
    std::ifstream fs_book;
    fs_book.open(book_file_name);

    if (!fs_book.is_open())
    {
      sync_cout << "Error! : can't read " << book_file_name << sync_endl;
      return false;
    }

    sync_cout << "read book.sfen " << sync_endl;
    std::string line;
    while (!fs_book.eof())
    {
      std::getline(fs_book, line);
      if (!line.empty())
        book.push_back(line);
      if ((book.size() % 1000) == 0)
        std::cout << ".";
    }
    std::cout << std::endl;
    return true;
  }

  template<typename T>
  T ParseOptionOrDie(const char* name) {
    std::string value_string = (std::string)Options[name];
    std::istringstream iss(value_string);
    T value;
    if (!(iss >> value)) {
      sync_cout << "Failed to parse an option. Exitting...: name=" << name << " value=" << value << sync_endl;
      std::exit(1);
    }
    return value;
  }

  // �����ړ�����w���肩�烉���_����1��I��Ŏw��
  bool DoRandomKingMove(Position& pos, std::mt19937_64& mt, StateInfo* state) {
    MoveList<LEGAL> list(pos);
    ExtMove* it2 = (ExtMove*)list.begin();
    for (ExtMove* it = (ExtMove*)list.begin(); it != list.end(); ++it)
      if (type_of(pos.moved_piece_after(it->move)) == KING)
        *it2++ = *it;

    auto size = it2 - list.begin();
    if (size == 0) {
      return false;
    }

    // �����_���ɂЂƂI��
    pos.do_move(list.at(std::uniform_int_distribution<>(0, static_cast<int>(size) - 1)(mt)),
      state[pos.game_ply()]);
    Eval::evaluate(pos);
    return true;
  }

  // 2��������_���ɓ���ւ���
  // https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  bool SwapTwoPieces(Position& pos, std::mt19937_64& mt) {
    for (int retry = 0; retry < 10; ++retry) {
      // ��ԑ��̋��2�����ւ���B

      // �^����ꂽBitboard���烉���_����1���I�сA����Square��Ԃ��B
      auto get_one = [&mt](Bitboard pieces) {
        // ��̐�
        int num = pieces.pop_count();

        // ���Ԗڂ��̋�
        int n = std::uniform_int_distribution<>(1, num)(mt);
        Square sq = SQ_NB;
        for (int i = 0; i < n; ++i) {
          sq = pieces.pop();
        }
        return sq;
      };

      // ���̏���2������ւ���B
      auto pieces = pos.pieces(pos.side_to_move());

      auto sq1 = get_one(pieces);
      // sq1������bitboard
      auto sq2 = get_one(pieces ^ sq1);

      // sq2�͉��������Ȃ��ꍇ�ASQ_NB�ɂȂ邩��A����𒲂ׂĂ���
      // ���̎w����ɐ���������A�����do_move�̑���ł��邩�獡��Ado_move()�͍s��Ȃ��B

      if (sq2 != SQ_NB && pos.do_move_by_swapping_pieces(sq1, sq2)) {
        Eval::evaluate(pos);
        // ���ؗp��assert
        if (!is_ok(pos)) {
          sync_cout << pos << sq1 << sq2 << sync_endl;
        }

        return true;
      }
    }

    return false;
  }

  // ���@��̒����烉���_����1��w��
  //https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
  bool DoRandomMove(Position& pos, std::mt19937_64& mt, StateInfo* state) {
    // ���@��̂Ȃ����烉���_����1��I�ԃt�F�[�Y
    MoveList<LEGAL> list(pos);
    Move m = list.at(std::uniform_int_distribution<>(0, static_cast<int>(list.size()) - 1)(mt));
    pos.do_move(m, state[pos.game_ply()]);
    Eval::evaluate(pos);
    return true;
  }
}

void Learner::InitializeGenerator(USI::OptionsMap& o) {
  o[OPTION_GENERATOR_NUM_POSITIONS] << Option("10000000000");
  o[OPTION_GENERATOR_MIN_SEARCH_DEPTH] << Option(3, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_SEARCH_DEPTH] << Option(4, 1, MAX_PLY);
  o[OPTION_GENERATOR_KIFU_TAG] << Option("default_tag");
  o[OPTION_GENERATOR_BOOK_FILE_NAME] << Option("startpos.sfen");
  o[OPTION_GENERATOR_MIN_BOOK_MOVE] << Option(0, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_BOOK_MOVE] << Option(32, 1, MAX_PLY);
  o[OPTION_GENERATOR_VALUE_THRESHOLD] << Option(VALUE_MATE, 0, VALUE_MATE);
  o[OPTION_GENERATOR_DO_RANDOM_KING_MOVE_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY] << Option("0.1");
  o[OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK] << Option(true);
}

void Learner::GenerateKifu()
{
#ifdef USE_FALSE_PROBE_IN_TT
  sync_cout << "Please undefine USE_FALSE_PROBE_IN_TT." << sync_endl;
  ASSERT_LV3(false);
#endif

  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  // ��Ղ̓ǂݍ���
  if (!ReadBook()) {
    sync_cout << "Failed to read the book." << sync_endl;
    return;
  }

  Eval::load_eval();

  omp_set_num_threads((int)Options["Threads"]);

  Search::LimitsType limits;
  limits.max_game_ply = kMaxGamePlay;
  limits.depth = MAX_PLY;
  limits.silent = true;
  Search::Limits = limits;

  std::string kifu_directory = (std::string)Options["KifuDir"];
  _mkdir(kifu_directory.c_str());

  int min_search_depth = Options[OPTION_GENERATOR_MIN_SEARCH_DEPTH];
  int max_search_depth = Options[OPTION_GENERATOR_MAX_SEARCH_DEPTH];
  std::uniform_int_distribution<> search_depth_distribution(min_search_depth, max_search_depth);
  int min_book_move = Options[OPTION_GENERATOR_MIN_BOOK_MOVE];
  int max_book_move = Options[OPTION_GENERATOR_MAX_BOOK_MOVE];
  std::uniform_int_distribution<> num_book_move_distribution(min_book_move, max_book_move);
  int64_t num_positions = ParseOptionOrDie<int64_t>(OPTION_GENERATOR_NUM_POSITIONS);
  double do_random_king_move_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_DO_RANDOM_KING_MOVE_PROBABILITY);
  double swap_two_pieces_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_SWAP_TWO_PIECES_PROBABILITY);
  double do_random_move_probability =
    ParseOptionOrDie<double>(OPTION_GENERATOR_DO_RANDOM_MOVE_PROBABILITY);
  bool do_random_move_after_book = (bool)Options[OPTION_GENERATOR_DO_RANDOM_MOVE_AFTER_BOOK];
  int value_threshold = Options[OPTION_GENERATOR_VALUE_THRESHOLD];

  time_t start;
  std::time(&start);
  ASSERT_LV3(book.size());
  std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
  // �X���b�h�Ԃŋ��L����
  std::atomic_int64_t global_position_index = 0;
#pragma omp parallel
  {
    int thread_index = ::omp_get_thread_num();
    char output_file_path[1024];
    std::string output_file_name_tag = Options[OPTION_GENERATOR_KIFU_TAG];
    std::sprintf(output_file_path, "%s/kifu.%s.%d-%d.%I64d.%03d.bin", kifu_directory.c_str(),
      output_file_name_tag.c_str(), min_search_depth, max_search_depth, num_positions,
      thread_index);
    // �e�X���b�h�Ɏ�������
    std::unique_ptr<Learner::KifuWriter> kifu_writer =
      std::make_unique<Learner::KifuWriter>(output_file_path);
    std::mt19937_64 mt19937_64(start + thread_index);

    while (global_position_index < num_positions) {
      Thread& thread = *Threads[thread_index];
      Position& pos = thread.rootPos;
      pos.set_hirate();
      StateInfo state_infos[512] = { 0 };
      StateInfo* state = state_infos + 8;

      const std::string& opening = book[opening_index(mt19937_64)];
      std::istringstream is(opening);
      std::string token;
      int num_book_move = num_book_move_distribution(mt19937_64);
      while (global_position_index < num_positions && pos.game_ply() < num_book_move)
      {
        if (!(is >> token)) {
          break;
        }
        if (token == "startpos" || token == "moves")
          continue;

        Move m = move_from_usi(pos, token);
        if (!is_ok(m) || !pos.legal(m))
        {
          //  sync_cout << "Error book.sfen , line = " << book_number << " , moves = " << token << endl << rootPos << sync_endl;
          // ���@�G���[�����͂��Ȃ��B
          break;
        }

        pos.do_move(m, state[pos.game_ply()]);
        // �����v�Z�̂���evaluate()���Ăяo��
        Eval::evaluate(pos);
      }

      if (do_random_move_after_book) {
        // �J�n�ǖʂ��烉���_���Ɏ���w���āA�ǖʂ��o����������
        DoRandomMove(pos, mt19937_64, state);
      }

      while (pos.game_ply() < kMaxGamePlay) {
        pos.set_this_thread(&thread);
        if (pos.is_mated()) {
          break;
        }
        int search_depth = search_depth_distribution(mt19937_64);
        auto valueAndPv = Learner::search(pos, -VALUE_INFINITE, VALUE_INFINITE, search_depth);

        // Apery�ł͌��Ԃł��X�R�A�̒l�𔽓]�������Ɋw�K�ɗp���Ă���
        Value value = valueAndPv.first;
        const std::vector<Move>& pv = valueAndPv.second;
        if (pv.empty()) {
          break;
        }

        // �]���l�̐�Βl�̏���𒴂��Ă���ꍇ�͏����o���Ȃ��悤�ɂ���
        if (std::abs(value) > value_threshold) {
          break;
        }

        // �ǖʂ��s���ȏꍇ������̂ōēx�`�F�b�N����
        if (pos.pos_is_ok()) {
          Learner::Record record = { 0 };
          pos.sfen_pack(record.packed);

          record.value = value;

          kifu_writer->Write(record);
          int64_t position_index = global_position_index++;
          ShowProgress(start, position_index, num_positions, 1000'0000);
        }

        // �w�肵���m���ɏ]���ē��ʂȎw������w��
        double r = probability_distribution(mt19937_64);
        bool special_move_is_done = false;
        if (r < do_random_king_move_probability) {
          if (!pos.in_check()) {
            special_move_is_done = DoRandomKingMove(pos, mt19937_64, state);
          }
        }
        else if (r < do_random_king_move_probability + swap_two_pieces_probability) {
          if (!pos.in_check() && pos.pieces(pos.side_to_move()).pop_count() >= 6) {
            special_move_is_done = SwapTwoPieces(pos, mt19937_64);
          }
        }
        else if (r < do_random_king_move_probability + swap_two_pieces_probability +
          do_random_move_probability) {
          if (!pos.in_check()) {
            special_move_is_done = DoRandomMove(pos, mt19937_64, state);
          }
        }

        // ���ʂȎw���肪�w����Ȃ������ꍇ�͒T���ɂ��w������w��
        if (!special_move_is_done) {
          pos.do_move(pv[0], state[pos.game_ply()]);
          // �����v�Z�̂���evaluate()���Ăяo��
          Eval::evaluate(pos);
        }
      }
    }
  }
}
