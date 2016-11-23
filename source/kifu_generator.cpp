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
  constexpr char* OPTION_GENERATOR_ENABLE_SWAP = "GeneratorEnableSwap";
  constexpr char* OPTION_GENERATOR_VALUE_THRESHOLD = "GeneratorValueThreshold";

  std::vector<std::string> book;
  std::uniform_int_distribution<> swap_distribution(0, 9);

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
}

void Learner::InitializeGenerator(USI::OptionsMap& o) {
  o[OPTION_GENERATOR_NUM_POSITIONS] << Option("10000000000");
  o[OPTION_GENERATOR_MIN_SEARCH_DEPTH] << Option(3, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_SEARCH_DEPTH] << Option(4, 1, MAX_PLY);
  o[OPTION_GENERATOR_KIFU_TAG] << Option("default_tag");
  o[OPTION_GENERATOR_BOOK_FILE_NAME] << Option("startpos.sfen");
  o[OPTION_GENERATOR_MIN_BOOK_MOVE] << Option(0, 1, MAX_PLY);
  o[OPTION_GENERATOR_MAX_BOOK_MOVE] << Option(32, 1, MAX_PLY);
  o[OPTION_GENERATOR_ENABLE_SWAP] << Option(true);
  o[OPTION_GENERATOR_VALUE_THRESHOLD] << Option(VALUE_MATE, 0, VALUE_MATE);
}

void Learner::GenerateKifu()
{
#ifdef USE_FALSE_PROBE_IN_TT
  sync_cout << "Please undefine USE_FALSE_PROBE_IN_TT." << sync_endl;
  ASSERT_LV3(false);
#endif

  std::srand(std::time(nullptr));

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
  int64_t num_positions;
  std::istringstream((std::string)Options[OPTION_GENERATOR_NUM_POSITIONS]) >> num_positions;

  time_t start;
  std::time(&start);
  ASSERT_LV3(book.size());
  std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
  // �X���b�h�Ԃŋ��L����
  std::atomic_int64_t global_position_index = 0;
  bool enable_swap = Options[OPTION_GENERATOR_ENABLE_SWAP];
  int value_threshold = Options[OPTION_GENERATOR_VALUE_THRESHOLD];
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
      auto SetupStates = Search::StateStackPtr(new aligned_stack<StateInfo>);

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

        SetupStates->push(StateInfo());
        pos.do_move(m, SetupStates->top());
      }

      while (pos.game_ply() < kMaxGamePlay) {
        // ���̊m���Ŏ���2������ւ���
        // TODO(tanuki-): �O��������i�荞�߂Ă��Ȃ��̂ōi�荞��
        if (enable_swap && swap_distribution(mt19937_64) == 0 &&
          pos.pos_is_ok() &&
          !pos.is_mated() &&
          !pos.in_check() &&
          !pos.attackers_to(pos.side_to_move(), pos.king_square(~pos.side_to_move()))) {
          std::string originalSfen = pos.sfen();
          int counter = 0;
          for (; counter < kMaxSwapTrials; ++counter) {
            pos.set(originalSfen);

            // ����2������ւ���
            std::vector<Square> myPieceSquares;
            for (Square square = SQ_11; square < SQ_NB; ++square) {
              Piece piece = pos.piece_on(square);
              if (piece == NO_PIECE) {
                continue;
              }
              else if (color_of(piece) == pos.side_to_move()) {
                myPieceSquares.push_back(square);
              }
            }

            // �ꎞ�̈�Ƃ��Ďg�p����}�X��I��
            Square square0;
            Square square1;
            int num_trials_to_select_squares;
            // �������[�v�Ɋׂ�ꍇ������̂Ő�����������
            for (num_trials_to_select_squares = 0;
              num_trials_to_select_squares < kMaxTrialsToSelectSquares;
              ++num_trials_to_select_squares) {
              std::uniform_int_distribution<> square_index_distribution(0, myPieceSquares.size() - 1);
              square0 = myPieceSquares[square_index_distribution(mt19937_64)];
              square1 = myPieceSquares[square_index_distribution(mt19937_64)];
              // ������ނ̋��I�΂Ȃ��悤�ɂ���
              if (pos.piece_on(square0) != pos.piece_on(square1)) {
                break;
              }
            }

            if (num_trials_to_select_squares == kMaxTrialsToSelectSquares) {
              break;
            }

            Piece piece0 = pos.piece_on(square0);
            PieceNo pieceNo0 = pos.piece_no_of(piece0, square0);
            Piece piece1 = pos.piece_on(square1);
            PieceNo pieceNo1 = pos.piece_no_of(piece1, square1);

            // �ʂ�����̋�̗����̂��鏡�Ɉړ�����ꍇ�͂�蒼��
            // TODO(tanuki-): �K�v���ǂ����킩��Ȃ��̂Œ��ׂ�
            if (type_of(piece0) == KING && pos.attackers_to(~pos.side_to_move(), square1)) {
              continue;
            }
            if (type_of(piece1) == KING && pos.attackers_to(~pos.side_to_move(), square0)) {
              continue;
            }

            // 2�̋�����ւ���
            pos.remove_piece(square0);
            pos.remove_piece(square1);
            pos.put_piece(square0, piece1, pieceNo1);
            pos.put_piece(square1, piece0, pieceNo0);

            // �s���ȋǖʂɂȂ����ꍇ�͂�蒼��
            // �����_���ɓ���ւ����2���E9�i�ڂ̕����E89�i�ڂ̌j�Ȃ�
            // �s���ȏ�ԂɂȂ�ꍇ�����邽��
            if (!pos.pos_is_ok()) {
              continue;
            }

            // 1��l�߂ɂȂ����ꍇ����蒼��
            // TODO(tanuki-): ��蒼���̏������i�荞��
            if (pos.mate1ply()) {
              continue;
            }

            // �l��ł��܂��Ă���ꍇ����蒼��
            // TODO(tanuki-): ��蒼���̏������i�荞��
            if (pos.is_mated()) {
              continue;
            }

            // ���肪�������Ă���ꍇ���L�����Z��
            if (pos.in_check()) {
              continue;
            }

            // ����ւ�����ő���̉�������ꍇ����蒼��
            if (pos.attackers_to(pos.side_to_move(), pos.king_square(~pos.side_to_move()))) {
              continue;
            }

            // �n�b�V���E�������̓�����Ԃ��X�V���邽��sfen������set����
            pos.set(pos.sfen());

            break;
          };

          // ���x��蒼���Ă����܂������Ȃ��ꍇ�͒��߂�
          if (counter >= kMaxSwapTrials) {
            pos.set(originalSfen);
          }
        }

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

        if (std::abs(value) > value_threshold) {
          break;
        }

        // �ǖʂ��s���ȏꍇ������̂ōēx�`�F�b�N����
        if (pos.pos_is_ok()) {
          Learner::Record record = { 0 };
          pos.sfen_pack(record.packed);

          record.value = value;

          int num_pv = search_depth;
          num_pv = std::min<int>(num_pv, sizeof(record.pv) / sizeof(record.pv[0]));
          num_pv = std::min<int>(num_pv, static_cast<int>(pv.size()));
          copy(pv.begin(), pv.begin() + num_pv, record.pv);

          kifu_writer->Write(record);
          int64_t position_index = global_position_index++;
          ShowProgress(start, position_index, num_positions, 1000'0000);
        }

        SetupStates->push(StateInfo());
        pos.do_move(pv[0], SetupStates->top());
      }
    }
  }
}
