#ifdef USE_KIFU_GENERATOR

#include "kifu_generator.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <direct.h>
#include <fstream>
#include <memory>
#include <omp.h>
#include <random>
#include <sstream>

#include "kifu_writer.h"
#include "misc.h"
#include "progress_report.h"
#include "search.h"
#include "thread.h"

#ifdef abs
#undef abs
#endif

using Search::RootMove;
using USI::Option;
using USI::OptionsMap;

namespace Learner
{
	std::pair<Value, std::vector<Move> > search(Position& pos, int depth);
	std::pair<Value, std::vector<Move> > qsearch(Position& pos);
}

namespace
{
	constexpr int kMaxGamePlay = 256;
	constexpr int kMaxSwapTrials = 10;
	constexpr int kMaxTrialsToSelectSquares = 100;

	constexpr char* kOptionGeneratorNumPositions = "GeneratorNumPositions";
	constexpr char* kOptionGeneratorMinSearchDepth = "GeneratorMinSearchDepth";
	constexpr char* kOptionGeneratorMaxSearchDepth = "GeneratorMaxSearchDepth";
	constexpr char* kOptionGeneratorKifuTag = "GeneratorKifuTag";
	constexpr char* kOptionGeneratorStartposFileName = "GeneratorStartposFileName";
	constexpr char* kOptionGeneratorMinBookMove = "GeneratorMinBookMove";
	constexpr char* kOptionGeneratorMaxBookMove = "GeneratorMaxBookMove";
	constexpr char* kOptionGeneratorValueThreshold = "GeneratorValueThreshold";

	std::vector<std::string> book;
	std::uniform_real_distribution<> probability_distribution;

	bool ReadBook()
	{
		// ��Ճt�@�C��(�Ƃ������P�Ȃ�����t�@�C��)�̓ǂݍ���
		std::string book_file_name = Options[kOptionGeneratorStartposFileName];
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

	// ���@��̒����烉���_����1��w��
	//https://github.com/yaneurao/YaneuraOu/blob/master/source/learn/learner.cpp
	bool DoRandomMove(Position& pos, std::mt19937_64& mt, StateInfo* state, Move& move) {
		// ���@��̂Ȃ����烉���_����1��I�ԃt�F�[�Y
		MoveList<LEGAL> list(pos);
		move = list.at(std::uniform_int_distribution<>(0, static_cast<int>(list.size()) - 1)(mt));
		pos.do_move(move, state[pos.game_ply()]);
		// �����v�Z�̂���evaluate()���Ăяo��
		Eval::evaluate(pos);
		return true;
	}
}

void Learner::InitializeGenerator(USI::OptionsMap& o) {
	o[kOptionGeneratorNumPositions] << Option("10000000000");
	o[kOptionGeneratorMinSearchDepth] << Option(3, 1, MAX_PLY);
	o[kOptionGeneratorMaxSearchDepth] << Option(4, 1, MAX_PLY);
	o[kOptionGeneratorKifuTag] << Option("default_tag");
	o[kOptionGeneratorStartposFileName] << Option("startpos.sfen");
	o[kOptionGeneratorMinBookMove] << Option(0, 1, MAX_PLY);
	o[kOptionGeneratorMaxBookMove] << Option(32, 1, MAX_PLY);
	o[kOptionGeneratorValueThreshold] << Option(3000, 0, VALUE_MATE);
}

void Learner::GenerateKifu()
{
#ifdef USE_FALSE_PROBE_IN_TT
	static_assert(false, "Please undefine USE_FALSE_PROBE_IN_TT.");
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

	int min_search_depth = Options[kOptionGeneratorMaxSearchDepth];
	int max_search_depth = Options[kOptionGeneratorMaxSearchDepth];
	std::uniform_int_distribution<> search_depth_distribution(min_search_depth, max_search_depth);
	int min_book_move = Options[kOptionGeneratorMinBookMove];
	int max_book_move = Options[kOptionGeneratorMaxBookMove];
	std::uniform_int_distribution<> num_book_move_distribution(min_book_move, max_book_move);
	int64_t num_positions = ParseOptionOrDie<int64_t>(kOptionGeneratorNumPositions);
	int value_threshold = Options[kOptionGeneratorValueThreshold];
	std::string output_file_name_tag = Options[kOptionGeneratorKifuTag];

	time_t start_time;
	std::time(&start_time);
	ASSERT_LV3(book.size());
	std::uniform_int<> opening_index(0, static_cast<int>(book.size() - 1));
	// �X���b�h�Ԃŋ��L����
	std::atomic_int64_t global_position_index = 0;
	ProgressReport progress_report(num_positions, 10 * 60);
#pragma omp parallel
	{
		int thread_index = ::omp_get_thread_num();
		WinProcGroup::bindThisThread(thread_index);
		char output_file_path[1024];
		std::sprintf(output_file_path, "%s/kifu.%s.%d-%d.%I64d.%03d.bin", kifu_directory.c_str(),
			output_file_name_tag.c_str(), min_search_depth, max_search_depth, num_positions,
			thread_index);
		// �e�X���b�h�Ɏ�������
		std::unique_ptr<Learner::KifuWriter> kifu_writer =
			std::make_unique<Learner::KifuWriter>(output_file_path);
		std::mt19937_64 mt19937_64(start_time + thread_index);

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

			if (!pos.in_check())
			{
				// �J�n�ǖʂ��烉���_���Ɏ���w���āA�ǖʂ��o����������
				Move move = Move::MOVE_NONE;
				DoRandomMove(pos, mt19937_64, state, move);
				// �����v�Z�̂���evaluate()���Ăяo��
				Eval::evaluate(pos);
			}

			std::vector<Learner::Record> records;
			Value last_value;
			while (pos.game_ply() < kMaxGamePlay && !pos.is_mated() && pos.is_repetition() == REPETITION_NONE) {
				pos.set_this_thread(&thread);

				Move pv_move = Move::MOVE_NONE;
				int search_depth = search_depth_distribution(mt19937_64);
				auto valueAndPv = Learner::search(pos, search_depth);

				// Apery�ł͌��Ԃł��X�R�A�̒l�𔽓]�������Ɋw�K�ɗp���Ă���
				last_value = valueAndPv.first;

				// �]���l�̐�Βl��臒l�𒴂�����I������
				if (std::abs(last_value) > value_threshold) {
					break;
				}

				const std::vector<Move>& pv = valueAndPv.second;

				// �l�݂̏ꍇ��pv����ɂȂ�
				// ��L�̏���������̂ł���͂���Ȃ���������Ȃ�
				if (pv.empty()) {
					break;
				}

				// �ǖʂ��s���ȏꍇ������̂ōēx�`�F�b�N����
				if (!pos.pos_is_ok()) {
					break;
				}

				Learner::Record record = { 0 };
				pos.sfen_pack(record.packed);
				record.value = last_value;
				records.push_back(record);

				pv_move = pv[0];
				pos.do_move(pv_move, state[pos.game_ply()]);
				// �����v�Z�̂���evaluate()���Ăяo��
				Eval::evaluate(pos);
			}

			Color win;
			RepetitionState repetition_state = pos.is_repetition();
			if (last_value > value_threshold) {
				// ����
				// ���ʏ����������ɓ���͂�
				win = pos.side_to_move();
			}
			else if (last_value < -value_threshold) {
				// ����
				win = ~pos.side_to_move();
			}
			else if (repetition_state == REPETITION_NONE) {
				// �����ł͂Ȃ�
				// �w�K�f�[�^�ɂ͊܂߂Ȃ�
				continue;
			}
			else if (repetition_state == REPETITION_WIN) {
				// �A������̐����ɂ�鏟��
				// value = VALUE_MATE�ƂȂ邽�߂����ɂ͗��Ȃ��͂�
				win = pos.side_to_move();
			}
			else if (repetition_state == REPETITION_LOSE) {
				// �A������̐����ɂ�镉��
				// value = -VALUE_MATE�ƂȂ邽�߂����ɂ͗��Ȃ��͂�
				win = ~pos.side_to_move();
			}
			else if (repetition_state == REPETITION_DRAW) {
				// �A������ł͂Ȃ����ʂ̐����
				// �w�K�f�[�^�ɂ͊܂߂Ȃ�
				continue;
			}
			else if (repetition_state == REPETITION_SUPERIOR) {
				// �D���ǖ�(�Տ�̋�����Ŏ�������D��Ă���)
				// value = VALUE_SUPERIOR�ƂȂ邽�߂����ɂ͗��Ȃ��͂�
				win = pos.side_to_move();
			}
			else if (repetition_state == REPETITION_INFERIOR) {
				// �򓙋ǖ�(�Տ�̋�����Ŏ�������D��Ă���)
				// value = -VALUE_SUPERIOR�ƂȂ邽�߂����ɂ͗��Ȃ��͂�
				win = ~pos.side_to_move();
			}

			for (auto& record : records) {
				record.win_color = win;
			}

			for (const auto& record : records) {
				if (!kifu_writer->Write(record)) {
					sync_cout << "info string Failed to write a record." << sync_endl;
					std::exit(1);
				}
			}

			progress_report.Show(global_position_index += records.size());
		}

		// �K�v�ǖʐ�����������S�X���b�h�̒T�����~����
		// �������Ȃ��Ƒ����ʓ����@��̑����ǖʂŎ~�܂�܂łɎ��Ԃ�������
		Search::Signals.stop = true;
	}
}

#endif // USE_KIFU_GENERATOR
