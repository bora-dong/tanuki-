﻿#ifndef APERY_SEARCH_HPP
#define APERY_SEARCH_HPP

#include <atomic>
#include "move.hpp"
#include "pieceScore.hpp"
#include "scanner.hpp"
#include "thread.hpp"
#include "timeManager.hpp"
#include "tt.hpp"

class Position;
struct SplitPoint;

struct SearchStack {
  SplitPoint* splitPoint;
  Ply ply;
  Move currentMove;
  Move excludedMove; // todo: これは必要？
  Move killers[2];
  Depth reduction;
  Score staticEval;
  bool skipNullMove;
  Score staticEvalRaw; // 評価関数の差分計算用、値が入っていないときは ScoreNotEvaluated にしておく。
                       // 常に Black の評価値を入れておく。
};

struct SignalsType {
  std::atomic<bool> stopOnPonderHit;
  std::atomic<bool> firstRootMove;
  std::atomic<bool> stop;
  std::atomic<bool> failedLowAtRoot;
};

enum InaniwaFlag {
  NotInaniwa,
  InaniwaIsBlack,
  InaniwaIsWhite,
  InaniwaFlagNum
};

enum BishopInDangerFlag {
  NotBishopInDanger,
  BlackBishopInDangerIn28,
  WhiteBishopInDangerIn28,
  BlackBishopInDangerIn78,
  WhiteBishopInDangerIn78,
  BishopInDangerFlagNum
};

class RootMove {
public:
  RootMove() {}
  explicit RootMove(const Move m) : score_(-ScoreInfinite), prevScore_(-ScoreInfinite) {
    pv_.push_back(m);
    pv_.push_back(Move::moveNone());
  }
  explicit RootMove(const std::tuple<Move, Score> m) : score_(std::get<1>(m)), prevScore_(-ScoreInfinite) {
    pv_.push_back(std::get<0>(m));
    pv_.push_back(Move::moveNone());
  }

  bool operator < (const RootMove& m) const {
    return score_ < m.score_;
  }
  bool operator == (const Move& m) const {
    return pv_[0] == m;
  }

  void extractPvFromTT(Position& pos);
  void insertPvInTT(Position& pos);

public:
  Score score_;
  Score prevScore_;
  std::vector<Move> pv_;
};

template <bool Gain>
class Stats {
public:
  static constexpr Score MaxScore = static_cast<Score>(2000);

  void clear() { memset(table_, 0, sizeof(table_)); }
  Score value(const bool isDrop, const Piece pc, const Square to) const {
    assert(0 < pc && pc < PieceNone);
    assert(isInSquare(to));
    return table_[isDrop][pc][to];
  }
  void update(const bool isDrop, const Piece pc, const Square to, const Score s) {
    if (Gain) {
      table_[isDrop][pc][to] = std::max(s, value(isDrop, pc, to) - 1);
    }
    else if (abs(value(isDrop, pc, to) + s) < MaxScore) {
      table_[isDrop][pc][to] += s;
    }
  }

private:
  // [isDrop][piece][square] とする。
  Score table_[2][PieceNone][SquareNum];
};

using History = Stats<false>;
using Gains = Stats<true>;

class TranspositionTable;

struct Searcher {
  // static メンバ関数からだとthis呼べないので代わりに thisptr を使う。
  // static じゃないときは this を入れることにする。
  STATIC Searcher* thisptr;
  STATIC SignalsType signals;
  STATIC LimitsType limits;
  STATIC std::vector<Move> searchMoves;
  STATIC Time searchTimer;
  STATIC u64 lastSearchedNodes;
  STATIC StateStackPtr setUpStates;
  STATIC std::vector<RootMove> rootMoves;

  STATIC size_t pvSize;
  STATIC size_t pvIdx;
  STATIC std::unique_ptr<TimeManager> timeManager;
  STATIC Ply bestMoveChanges;
  STATIC History history;
  STATIC Gains gains;
  STATIC TranspositionTable tt;
  STATIC bool recordIterativeDeepningScores;
  // 思考スレッドの監視スレッドの実行周期の最小値
  STATIC constexpr int MIN_TIMER_PERIOD_MS = 5;
  // 思考スレッドの監視スレッドの実行周期の最大値
  STATIC constexpr int MAX_TIMER_PERIOD_MS = 32;

#if defined INANIWA_SHIFT
  STATIC InaniwaFlag inaniwaFlag;
#endif
#if defined BISHOP_IN_DANGER
  STATIC BishopInDangerFlag bishopInDangerFlag;
#endif
  STATIC Position rootPosition;
  STATIC ThreadPool threads;
  STATIC OptionsMap options;
  STATIC bool outputInfo;

  STATIC void init();
  STATIC void idLoop(Position& pos);
  STATIC std::string pvInfoToUSI(Position& pos, const Ply depth, const Score alpha, const Score beta);
  template <NodeType NT, bool INCHECK>
  STATIC Score qsearch(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth);
#if defined INANIWA_SHIFT
  STATIC void detectInaniwa(const Position& pos);
#endif
#if defined BISHOP_IN_DANGER
  STATIC void detectBishopInDanger(const Position& pos);
#endif
  template <NodeType NT>
  STATIC Score search(Position& pos, SearchStack* ss, Score alpha, Score beta, const Depth depth, const bool cutNode);
  STATIC void think();
  STATIC void checkTime();

  STATIC void doUSICommandLoop(int argc, char* argv[]);
  STATIC void setOption(Scanner commands);
};

void initSearchTable();

#endif // #ifndef APERY_SEARCH_HPP
