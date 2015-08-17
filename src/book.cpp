﻿#include "book.hpp"
#include "position.hpp"
#include "move.hpp"
#include "usi.hpp"
#include "thread.hpp"

// 定跡生成時に探索を行い、score を得る為に必要。
void go(const Position& pos, std::istringstream& ssCmd);

MT64bit Book::mt64bit_; // 定跡のhash生成用なので、seedは固定でデフォルト値を使う。
Key Book::ZobPiece[PieceNone][SquareNum];
Key Book::ZobHand[HandPieceNum][19]; // 持ち駒の同一種類の駒の数ごと
Key Book::ZobTurn;

void Book::init() {
	for (Piece p = Empty; p < PieceNone; ++p) {
		for (Square sq = I9; sq < SquareNum; ++sq) {
			ZobPiece[p][sq] = mt64bit_.random();
		}
	}
	for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp) {
		for (int num = 0; num < 19; ++num) {
			ZobHand[hp][num] = mt64bit_.random();
		}
	}
	ZobTurn = mt64bit_.random();
}

bool Book::open(const char* fName) {
	fileName_ = "";

	if (is_open()) {
		close();
	}

	std::ifstream::open(fName, std::ifstream::in | std::ifstream::binary | std::ios::ate);

	if (!is_open()) {
		return false;
	}

	size_ = tellg() / sizeof(BookEntry);

	if (!good()) {
		std::cerr << "Failed to open book file " << fName  << std::endl;
		exit(EXIT_FAILURE);
	}

	fileName_ = fName;
	return true;
}

void Book::binary_search(const Key key) {
	size_t low = 0;
	size_t high = size_ - 1;
	size_t mid;
	BookEntry entry;

	while (low < high && good()) {
		mid = (low + high) / 2;

		assert(mid >= low && mid < high);

		// std::ios_base::beg はストリームの開始位置を指す。
		// よって、ファイルの開始位置から mid * sizeof(BookEntry) バイト進んだ位置を指す。
		seekg(mid * sizeof(BookEntry), std::ios_base::beg);
		read(reinterpret_cast<char*>(&entry), sizeof(entry));

		if (key <= entry.key) {
			high = mid;
		}
		else {
			low = mid + 1;
		}
	}

	assert(low == high);

	seekg(low * sizeof(BookEntry), std::ios_base::beg);
}

Key Book::bookKey(const Position& pos, const bool inaniwaBook) {
	Key key = 0;
	Bitboard bb = pos.occupiedBB();

	if (inaniwaBook) {
		// 自陣から5段目までと、角の位置のみから key を生成する。
		const Color us = pos.turn();
		const Rank TRank6 = (us == Black ? Rank6 : Rank4);
		const Bitboard TRank1_5BB = inFrontMask(oppositeColor(us), TRank6);
		bb &= TRank1_5BB;
		bb |= pos.bbOf(Bishop);
	}

	while (bb.isNot0()) {
		const Square sq = bb.firstOneFromI9();
		key ^= ZobPiece[pos.piece(sq)][sq];
	}
	const Hand hand = pos.hand(pos.turn());
	for (HandPiece hp = HPawn; hp < HandPieceNum; ++hp) {
		key ^= ZobHand[hp][hand.numOf(hp)];
	}
	if (pos.turn() == White) {
		key ^= ZobTurn;
	}
	return key;
}

std::tuple<Move, Score> Book::probe(const Position& pos, const std::string& fName, const bool pickBest, const bool inaniwaBook) {
	BookEntry entry;
	u16 best = 0;
	u32 sum = 0;
	Move move = Move::moveNone();
	const Key key = bookKey(pos, inaniwaBook);
	const Score min_book_score = static_cast<Score>(static_cast<int>(g_options["Min_Book_Score"]));
	Score score;

	if (fileName_ != fName && !open(fName.c_str())) {
		return std::make_tuple(Move::moveNone(), ScoreNone);
	}

	binary_search(key);

	// 現在の局面における定跡手の数だけループする。
	while (read(reinterpret_cast<char*>(&entry), sizeof(entry)), entry.key == key && good()) {
		best = std::max(best, entry.count);
		sum += entry.count;

		// 指された確率に従って手が選択される。
		// count が大きい順に並んでいる必要はない。
		if (min_book_score <= entry.score
			&& ((random_.random() % sum < entry.count)
				|| (pickBest && entry.count == best)))
		{
			const Move tmp = Move(entry.fromToPro);
			const Square to = tmp.to();
			if (tmp.isDrop()) {
				const PieceType ptDropped = tmp.pieceTypeDropped();
				move = makeDropMove(ptDropped, to);
			}
			else {
				const Square from = tmp.from();
				const PieceType ptFrom = pieceToPieceType(pos.piece(from));
				const bool promo = tmp.isPromotion();
				if (promo) {
					move = makeCapturePromoteMove(ptFrom, from, to, pos);
				}
				else {
					move = makeCaptureMove(ptFrom, from, to, pos);
				}
			}
			score = entry.score;
		}
	}

	return std::make_tuple(move, score);
}

inline bool countCompare(const BookEntry& b1, const BookEntry& b2) {
	return b1.count < b2.count;
}

#if !defined MINIMUL
void makeBook(Position& pos, const bool inaniwaBook) {
	std::ifstream ifs("../book.sfen", std::ios::binary);
	std::string token;
	std::string line;
	std::map<Key, std::vector<BookEntry> > bookMap;

	while (std::getline(ifs, line)) {
		std::string sfen;
		std::stringstream ss(line);
		ss >> token;

		if (token == "startpos") {
			sfen = DefaultStartPositionSFEN;
			ss >> token; // "moves" が入力されるはず。
		}
		else if (token == "sfen") {
			while (ss >> token && token != "moves") {
				sfen += token + " ";
			}
		}
		pos.set(sfen, g_threads.mainThread());
		StateStackPtr SetUpStates = StateStackPtr(new std::stack<StateInfo>());
		Ply currentPly = pos.gamePly();
		while (ss >> token) {
			const Move move = usiToMove(pos, token);
			if (move.isNone()) {
				pos.print();
				std::cout << "!!! Illegal move = " << token << " !!!" << std::endl;
				break;
			}
			const Key key = Book::bookKey(pos, inaniwaBook);
			bool isFind = false;
			if (bookMap.find(key) != bookMap.end()) {
				for (auto& elem : bookMap[key]) {
					if (elem.fromToPro == move.proFromAndTo()) {
						++elem.count;
						if (elem.count < 1) {
							// 数えられる数の上限を超えたので元に戻す。
							--elem.count;
						}
						isFind = true;
					}
				}
			}
			if (isFind == false) {
				// 未登録の手
				BookEntry be;
				be.score = ScoreZero;
				be.key = key;
				be.fromToPro = static_cast<u16>(move.proFromAndTo());
				be.count = 1;
				bookMap[key].push_back(be);
			}
			SetUpStates->push(StateInfo());
			pos.doMove(usiToMove(pos, token), SetUpStates->top());
			++currentPly;
			pos.setStartPosPly(currentPly);
		}
	}

	// BookEntry::count の値で降順にソート
	for (auto& elem : bookMap) {
		std::sort(elem.second.rbegin(), elem.second.rend(), countCompare);
	}

#if 0
	// 2 回以上棋譜に出現していない手は削除する。
	for (auto& elem : bookMap) {
		auto& second = elem.second;
		auto erase_it = std::find_if(second.begin(), second.end(), [](decltype(*second.begin())& second_elem) { return second_elem.count < 2; });
		second.erase(erase_it, second.end());
	}
#endif

	std::ofstream ofs((inaniwaBook ? "inaniwabook.bin" : "book.bin"), std::ios::binary);
	for (auto& elem : bookMap) {
		for (auto& elel : elem.second) {
			ofs.write(reinterpret_cast<char*>(&(elel)), sizeof(BookEntry));
		}
	}

	std::cout << "book making was done" << std::endl;
}
void makeBookCSA1Line(Position& pos, const bool inaniwaBook) {
	std::ifstream ifs((inaniwaBook ? "../utf8inaniwakifu.csa" : "../2chkifu/2013/utf82chkifu.csa"), std::ios::binary);
	std::string line;
	std::map<Key, std::vector<BookEntry> > bookMap;

	while (std::getline(ifs, line)) {
		std::string elem;
		std::stringstream ss(line);
		ss >> elem; // 棋譜番号を飛ばす。
		ss >> elem; // 対局日を飛ばす。
		ss >> elem; // 先手
		const std::string sente = elem;
		ss >> elem; // 後手
		const std::string gote = elem;
		ss >> elem; // (0:引き分け,1:先手の勝ち,2:後手の勝ち)
		const Color winner = (elem == "1" ? Black : elem == "2" ? White : ColorNum);
		const Color inaniwaColor = (sente == "Inaniwa" ? Black : gote == "Inaniwa" ? White : ColorNum);
		// 勝った方の指し手を記録していく。
		// 又は稲庭戦法側を記録していく。
		const Color saveColor = (inaniwaBook ? inaniwaColor : winner);

		if (!std::getline(ifs, line)) {
			std::cout << "!!! header only !!!" << std::endl;
			return;
		}
		pos.set(DefaultStartPositionSFEN, g_threads.mainThread());
		StateStackPtr SetUpStates = StateStackPtr(new std::stack<StateInfo>());
		while (!line.empty()) {
			const std::string moveStrCSA = line.substr(0, 6);
			const Move move = csaToMove(pos, moveStrCSA);
			if (move.isNone()) {
				pos.print();
				std::cout << "!!! Illegal move = " << moveStrCSA << " !!!" << std::endl;
				break;
			}
			line.erase(0, 6); // 先頭から6文字削除
			if (pos.turn() == saveColor) {
				// 先手、後手の内、片方だけを記録する。
				const Key key = Book::bookKey(pos, inaniwaBook);
				bool isFind = false;
				if (bookMap.find(key) != bookMap.end()) {
					for (std::vector<BookEntry>::iterator it = bookMap[key].begin();
						 it != bookMap[key].end();
						 ++it)
					{
						if (it->fromToPro == move.proFromAndTo()) {
							++it->count;
							if (it->count < 1) {
								// 数えられる数の上限を超えたので元に戻す。
								--it->count;
							}
							isFind = true;
						}
					}
				}
				if (isFind == false) {
#if defined MAKE_SEARCHED_BOOK
					SetUpStates->push(StateInfo());
					pos.doMove(move, SetUpStates->top());

					std::istringstream ssCmd("byoyomi 1000");
					go(pos, ssCmd);
					g_threads.waitForThinkFinished();

					pos.undoMove(move);
					SetUpStates->pop();

					// doMove してから search してるので点数が反転しているので直す。
					const Score score = -Searcher::rootMoves[0].score_;
#else
					const Score score = ScoreZero;
#endif
					// 未登録の手
					BookEntry be;
					be.score = score;
					be.key = key;
					be.fromToPro = static_cast<u16>(move.proFromAndTo());
					be.count = 1;
					bookMap[key].push_back(be);
				}
			}
			SetUpStates->push(StateInfo());
			pos.doMove(move, SetUpStates->top());
		}
	}

	// BookEntry::count の値で降順にソート
	for (auto& elem : bookMap) {
		std::sort(elem.second.rbegin(), elem.second.rend(), countCompare);
	}

#if 0
	// 2 回以上棋譜に出現していない手は削除する。
	for (auto& elem : bookMap) {
		auto& second = elem.second;
		auto erase_it = std::find_if(second.begin(), second.end(), [](decltype(*second.begin())& second_elem) { return second_elem.count < 2; });
		second.erase(erase_it, second.end());
	}
#endif

#if 0
	// narrow book
	for (auto& elem : bookMap) {
		auto& second = elem.second;
		auto erase_it = std::find_if(second.begin(), second.end(), [&](decltype(*second.begin())& second_elem) { return second_elem.count < second[0].count / 2; });
		second.erase(erase_it, second.end());
	}
#endif

	std::ofstream ofs((inaniwaBook ? "inaniwabook.bin" : "book.bin"), std::ios::binary);
	for (auto& elem : bookMap) {
		for (auto& elel : elem.second) {
			ofs.write(reinterpret_cast<char*>(&(elel)), sizeof(BookEntry));
		}
	}

	std::cout << "book making was done" << std::endl;
}
#endif
