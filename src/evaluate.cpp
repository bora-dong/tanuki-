﻿#include "evaluate.hpp"
#include "position.hpp"
#include "search.hpp"
#include "thread.hpp"

KPPBoardIndexStartToPiece g_kppBoardIndexStartToPiece;

s16 Evaluater::KPP[SquareNum][fe_end + KPP_PADDING1][fe_end + KPP_PADDING0];
s32 Evaluater::KKP[SquareNum][SquareNum][fe_end];
s32 Evaluater::KK[SquareNum][SquareNum];

#if defined USE_K_FIX_OFFSET
const s32 Evaluater::K_Fix_Offset[SquareNum] = {
  2000 * FVScale, 1700 * FVScale, 1350 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale,
  1800 * FVScale, 1500 * FVScale, 1250 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale,
  1800 * FVScale, 1500 * FVScale, 1250 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale,
  1700 * FVScale, 1400 * FVScale, 1150 * FVScale,  900 * FVScale,  550 * FVScale,  250 * FVScale,    0 * FVScale,    0 * FVScale,    0 * FVScale,
  1600 * FVScale, 1300 * FVScale, 1050 * FVScale,  800 * FVScale,  450 * FVScale,  150 * FVScale,    0 * FVScale,    0 * FVScale,    0 * FVScale,
  1700 * FVScale, 1400 * FVScale, 1150 * FVScale,  900 * FVScale,  550 * FVScale,  250 * FVScale,    0 * FVScale,    0 * FVScale,    0 * FVScale,
  1800 * FVScale, 1500 * FVScale, 1250 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale,
  1900 * FVScale, 1600 * FVScale, 1350 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale,
  2000 * FVScale, 1700 * FVScale, 1350 * FVScale, 1000 * FVScale,  650 * FVScale,  350 * FVScale,  100 * FVScale,    0 * FVScale,    0 * FVScale
};
#endif

EvaluateHashTable g_evalTable;

const int kppArray[31] = {
  0,        f_pawn,   f_lance,  f_knight,
  f_silver, f_bishop, f_rook,   f_gold,
  0,        f_gold,   f_gold,   f_gold,
  f_gold,   f_horse,  f_dragon,
  0,
  0,        e_pawn,   e_lance,  e_knight,
  e_silver, e_bishop, e_rook,   e_gold,
  0,        e_gold,   e_gold,   e_gold,
  e_gold,   e_horse,  e_dragon
};

const int kppHandArray[ColorNum][HandPieceNum] = {
  { f_hand_pawn, f_hand_lance, f_hand_knight, f_hand_silver,
  f_hand_gold, f_hand_bishop, f_hand_rook },
  { e_hand_pawn, e_hand_lance, e_hand_knight, e_hand_silver,
  e_hand_gold, e_hand_bishop, e_hand_rook }
};

namespace {
#ifdef HAVE_AVX2
  static const __m256i MASK[9] = {
    _mm256_setzero_si256(),
    _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, -1),
    _mm256_set_epi32(0, 0, 0, 0, 0, 0, -1, -1),
    _mm256_set_epi32(0, 0, 0, 0, 0, -1, -1, -1),
    _mm256_set_epi32(0, 0, 0, 0, -1, -1, -1, -1),
    _mm256_set_epi32(0, 0, 0, -1, -1, -1, -1, -1),
    _mm256_set_epi32(0, 0, -1, -1, -1, -1, -1, -1),
    _mm256_set_epi32(0, -1, -1, -1, -1, -1, -1, -1),
    _mm256_set_epi32(-1, -1, -1, -1, -1, -1, -1, -1),
  };
#endif

  s32 doapc(const Position& pos, const int index[2]) {
    const Square sq_bk = pos.kingSquare(Black);
    const Square sq_wk = pos.kingSquare(White);
    const int* list0 = pos.cplist0();
    const int* list1 = pos.cplist1();

    s32 sum = Evaluater::KKP[sq_bk][sq_wk][index[0]];
    const auto* pkppb = Evaluater::KPP[sq_bk][index[0]];
    const auto* pkppw = Evaluater::KPP[inverse(sq_wk)][index[1]];

    for (int i = 0; i < pos.nlist(); ++i) {
      sum += pkppb[list0[i]];
      sum -= pkppw[list1[i]];
    }

    return sum;
  }

#if defined INANIWA_SHIFT
  Score inaniwaScoreBody(const Position& pos) {
    Score score = ScoreZero;
    if (pos.csearcher()->inaniwaFlag == InaniwaIsBlack) {
      if (pos.piece(B9) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(H9) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(A7) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(I7) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(C7) == WKnight) { score += 400 * FVScale; }
      if (pos.piece(G7) == WKnight) { score += 400 * FVScale; }
      if (pos.piece(B5) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(H5) == WKnight) { score += 700 * FVScale; }
      if (pos.piece(D5) == WKnight) { score += 100 * FVScale; }
      if (pos.piece(F5) == WKnight) { score += 100 * FVScale; }
      if (pos.piece(E3) == BPawn) { score += 200 * FVScale; }
      if (pos.piece(E4) == BPawn) { score += 200 * FVScale; }
      if (pos.piece(E5) == BPawn) { score += 200 * FVScale; }
    }
    else {
      assert(pos.csearcher()->inaniwaFlag == InaniwaIsWhite);
      if (pos.piece(B1) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(H1) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(A3) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(I3) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(C3) == BKnight) { score -= 400 * FVScale; }
      if (pos.piece(G3) == BKnight) { score -= 400 * FVScale; }
      if (pos.piece(B5) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(H5) == BKnight) { score -= 700 * FVScale; }
      if (pos.piece(D5) == BKnight) { score -= 100 * FVScale; }
      if (pos.piece(F5) == BKnight) { score -= 100 * FVScale; }
      if (pos.piece(E7) == WPawn) { score -= 200 * FVScale; }
      if (pos.piece(E6) == WPawn) { score -= 200 * FVScale; }
      if (pos.piece(E5) == WPawn) { score -= 200 * FVScale; }
    }
    return score;
  }
  inline Score inaniwaScore(const Position& pos) {
    if (pos.csearcher()->inaniwaFlag == NotInaniwa) return ScoreZero;
    return inaniwaScoreBody(pos);
  }
#endif

  bool calcDifference(Position& pos, SearchStack* ss) {
#if defined INANIWA_SHIFT
    if (pos.csearcher()->inaniwaFlag != NotInaniwa) return false;
#endif
    Move lastMove;
    if ((ss - 1)->staticEvalRaw == ScoreNotEvaluated || (lastMove = (ss - 1)->currentMove).pieceTypeFrom() == King) {
      return false;
    }

    assert(lastMove != Move::moveNull());

    const int listIndex = pos.cl().listindex[0];
    auto diff = doapc(pos, pos.cl().clistpair[0].newlist);
    if (pos.cl().size == 1) {
      pos.plist0()[listIndex] = pos.cl().clistpair[0].oldlist[0];
      pos.plist1()[listIndex] = pos.cl().clistpair[0].oldlist[1];
      diff -= doapc(pos, pos.cl().clistpair[0].oldlist);
    }
    else {
      assert(pos.cl().size == 2);
      diff += doapc(pos, pos.cl().clistpair[1].newlist);
      diff -= Evaluater::KPP[pos.kingSquare(Black)][pos.cl().clistpair[0].newlist[0]][pos.cl().clistpair[1].newlist[0]];
      diff += Evaluater::KPP[inverse(pos.kingSquare(White))][pos.cl().clistpair[0].newlist[1]][pos.cl().clistpair[1].newlist[1]];
      const int listIndex_cap = pos.cl().listindex[1];
      pos.plist0()[listIndex_cap] = pos.cl().clistpair[1].oldlist[0];
      pos.plist1()[listIndex_cap] = pos.cl().clistpair[1].oldlist[1];

      pos.plist0()[listIndex] = pos.cl().clistpair[0].oldlist[0];
      pos.plist1()[listIndex] = pos.cl().clistpair[0].oldlist[1];
      diff -= doapc(pos, pos.cl().clistpair[0].oldlist);

      diff -= doapc(pos, pos.cl().clistpair[1].oldlist);
      diff += Evaluater::KPP[pos.kingSquare(Black)][pos.cl().clistpair[0].oldlist[0]][pos.cl().clistpair[1].oldlist[0]];
      diff -= Evaluater::KPP[inverse(pos.kingSquare(White))][pos.cl().clistpair[0].oldlist[1]][pos.cl().clistpair[1].oldlist[1]];
      pos.plist0()[listIndex_cap] = pos.cl().clistpair[1].newlist[0];
      pos.plist1()[listIndex_cap] = pos.cl().clistpair[1].newlist[1];
    }
    pos.plist0()[listIndex] = pos.cl().clistpair[0].newlist[0];
    pos.plist1()[listIndex] = pos.cl().clistpair[0].newlist[1];

    diff += pos.materialDiff() * FVScale;

    ss->staticEvalRaw = static_cast<Score>(diff) + (ss - 1)->staticEvalRaw;

    return true;
  }

  int make_list_unUseDiff(const Position& pos, int list0[EvalList::ListSize], int list1[EvalList::ListSize], int nlist) {
    Square sq;
    Bitboard bb;

#define FOO(posBB, f_pt, e_pt)							\
		bb = (posBB) & pos.bbOf(Black);					\
		FOREACH_BB(bb, sq, {							\
				list0[nlist] = (f_pt) + sq;				\
				list1[nlist] = (e_pt) + inverse(sq);	\
				nlist    += 1;							\
			});											\
														\
		bb = (posBB) & pos.bbOf(White);					\
		FOREACH_BB(bb, sq, {							\
				list0[nlist] = (e_pt) + sq;				\
				list1[nlist] = (f_pt) + inverse(sq);	\
				nlist    += 1;							\
			});

    FOO(pos.bbOf(Pawn), f_pawn, e_pawn);
    FOO(pos.bbOf(Lance), f_lance, e_lance);
    FOO(pos.bbOf(Knight), f_knight, e_knight);
    FOO(pos.bbOf(Silver), f_silver, e_silver);
    const Bitboard goldsBB = pos.goldsBB();
    FOO(goldsBB, f_gold, e_gold);
    FOO(pos.bbOf(Bishop), f_bishop, e_bishop);
    FOO(pos.bbOf(Horse), f_horse, e_horse);
    FOO(pos.bbOf(Rook), f_rook, e_rook);
    FOO(pos.bbOf(Dragon), f_dragon, e_dragon);

#undef FOO

    return nlist;
  }

  s32 evaluateBody(Position& pos, SearchStack* ss) {
    if (calcDifference(pos, ss)) {
      const auto score = ss->staticEvalRaw;
      assert(evaluateUnUseDiff(pos) == (pos.turn() == Black ? score : -score));
      return score;
    }

    const Square sq_bk = pos.kingSquare(Black);
    const Square sq_wk = pos.kingSquare(White);
    const int* list0 = pos.plist0();
    const int* list1 = pos.plist1();

    const auto* ppkppb = Evaluater::KPP[sq_bk];
    const auto* ppkppw = Evaluater::KPP[inverse(sq_wk)];

    s32 score = Evaluater::KK[sq_bk][sq_wk];

#ifdef HAVE_AVX2
    __m256i ymmZero = _mm256_setzero_si256();
    __m256i ymmMask6 = _mm256_set_epi32(0, 0, -1, -1, -1, -1, -1, -1);

    const s32* kkpbw = Evaluater::KKP[sq_bk][sq_wk];
    __m256i ymmScore = ymmZero;
    for (int i = 0; i < pos.nlist(); i += 8) {
      __m256i ymmList0 = _mm256_load_si256((const __m256i*)&list0[i]);
      __m256i ymmKkp0 = _mm256_mask_i32gather_epi32(
        ymmZero,
        kkpbw,
        ymmList0,
        MASK[std::min(8, (int)pos.nlist() - i)],
        sizeof(s32));
      ymmScore = _mm256_add_epi32(ymmScore, ymmKkp0);
    }

    // http://www.slideshare.net/KenjiImasaki/ss-46408963
    ymmScore = _mm256_hadd_epi32(ymmScore, ymmScore);
    ymmScore = _mm256_hadd_epi32(ymmScore, ymmScore);
    __m128i xmmScoreLow = _mm256_castsi256_si128(ymmScore);
    __m128i xmmScoreHigh = _mm256_extracti128_si256(ymmScore, 1);
    score += _mm_cvtsi128_si32(xmmScoreLow);
    score += _mm_cvtsi128_si32(xmmScoreHigh);

    for (int i = 1; i < pos.nlist(); ++i) {
      const int k0 = list0[i];
      const int k1 = list1[i];
      const auto* pkppb = ppkppb[k0];
      const auto* pkppw = ppkppw[k1];
      for (int j = 0; j < i; ++j) {
        const int l0 = list0[j];
        const int l1 = list1[j];
        score += pkppb[l0];
        score -= pkppw[l1];
        //fprintf(stderr, "%5d %5d %5d %5d\n", l0, pkppb[l0], l1, pkppw[l1]);
      }
      //fprintf(stderr, "\n");
    }
#else

    // loop 開始を i = 1 からにして、i = 0 の分のKKPを先に足す。
    score += Evaluater::KKP[sq_bk][sq_wk][list0[0]];
    for (int i = 1; i < pos.nlist(); ++i) {
      const int k0 = list0[i];
      const int k1 = list1[i];
      const auto* pkppb = ppkppb[k0];
      const auto* pkppw = ppkppw[k1];
      for (int j = 0; j < i; ++j) {
        const int l0 = list0[j];
        const int l1 = list1[j];
        score += pkppb[l0];
        score -= pkppw[l1];
        //fprintf(stderr, "%5d %5d %5d %5d\n", l0, pkppb[l0], l1, pkppw[l1]);
      }
      //fprintf(stderr, "\n");
      score += Evaluater::KKP[sq_bk][sq_wk][k0];
    }
#endif

    score += pos.material() * FVScale;
#if defined INANIWA_SHIFT
    score += inaniwaScore(pos);
#endif
    ss->staticEvalRaw = static_cast<Score>(score);

    assert(evaluateUnUseDiff(pos) == (pos.turn() == Black ? score : -score));
    return score;
  }
}

Score evaluateUnUseDiff(const Position& pos) {
  int list0[EvalList::ListSize];
  int list1[EvalList::ListSize];

  const Hand handB = pos.hand(Black);
  const Hand handW = pos.hand(White);

  const Square sq_bk = pos.kingSquare(Black);
  const Square sq_wk = pos.kingSquare(White);
  int nlist = 0;

#define FOO(hand, HP, list0_index, list1_index)		\
	for (u32 i = 1; i <= hand.numOf<HP>(); ++i) {	\
		list0[nlist] = list0_index + i;				\
		list1[nlist] = list1_index + i;				\
		++nlist;									\
	}

  FOO(handB, HPawn, f_hand_pawn, e_hand_pawn);
  FOO(handW, HPawn, e_hand_pawn, f_hand_pawn);
  FOO(handB, HLance, f_hand_lance, e_hand_lance);
  FOO(handW, HLance, e_hand_lance, f_hand_lance);
  FOO(handB, HKnight, f_hand_knight, e_hand_knight);
  FOO(handW, HKnight, e_hand_knight, f_hand_knight);
  FOO(handB, HSilver, f_hand_silver, e_hand_silver);
  FOO(handW, HSilver, e_hand_silver, f_hand_silver);
  FOO(handB, HGold, f_hand_gold, e_hand_gold);
  FOO(handW, HGold, e_hand_gold, f_hand_gold);
  FOO(handB, HBishop, f_hand_bishop, e_hand_bishop);
  FOO(handW, HBishop, e_hand_bishop, f_hand_bishop);
  FOO(handB, HRook, f_hand_rook, e_hand_rook);
  FOO(handW, HRook, e_hand_rook, f_hand_rook);
#undef FOO

  nlist = make_list_unUseDiff(pos, list0, list1, nlist);

  const auto* ppkppb = Evaluater::KPP[sq_bk];
  const auto* ppkppw = Evaluater::KPP[inverse(sq_wk)];

  s32 score = Evaluater::KK[sq_bk][sq_wk];
  for (int i = 0; i < nlist; ++i) {
    const int k0 = list0[i];
    const int k1 = list1[i];
    const auto* pkppb = ppkppb[k0];
    const auto* pkppw = ppkppw[k1];
    for (int j = 0; j < i; ++j) {
      const int l0 = list0[j];
      const int l1 = list1[j];
      score += pkppb[l0];
      score -= pkppw[l1];
    }
    score += Evaluater::KKP[sq_bk][sq_wk][k0];
  }

  score += pos.material() * FVScale;

#if defined INANIWA_SHIFT
  score += inaniwaScore(pos);
#endif

  if (pos.turn() == White) {
    score = -score;
  }

  return static_cast<Score>(score);
}

Score evaluate(Position& pos, SearchStack* ss) {
  if (ss->staticEvalRaw != ScoreNotEvaluated) {
    // null move の次の手の時のみ、ここに入る。
    assert((pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) == evaluateUnUseDiff(pos));
    return (pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) / FVScale;
  }

  const u32 keyHigh32 = static_cast<u32>(pos.getKey() >> 32);
  const Key keyExcludeTurn = pos.getKeyExcludeTurn();
  // ポインタで取得してはいけない。lockless hash なので key と score を同時に取得する。
  EvaluateHashEntry entry = *g_evalTable[keyExcludeTurn];
  entry.decode();
  if (entry.key() == keyHigh32) {
    ss->staticEvalRaw = entry.score();
    assert((pos.turn() == Black ? ss->staticEvalRaw : -ss->staticEvalRaw) == evaluateUnUseDiff(pos));
    return (pos.turn() == Black ? entry.score() : -entry.score()) / FVScale;
  }

  const Score score = static_cast<Score>(evaluateBody(pos, ss));
  entry.save(pos.getKey(), score);
  entry.encode();
  *g_evalTable[keyExcludeTurn] = entry;
  return (pos.turn() == Black ? score : -score) / FVScale;
}

bool Evaluater::readSynthesized(const std::string& dirName) {
  {
    std::ifstream ifs((addSlashIfNone(dirName) + "KPP16_synthesized.bin").c_str(), std::ios::binary);
    if (!ifs) {
      return false;
    }
    // デバッグ実行時は速度アップのためパディングを無効にして直接読み込む
#ifdef _DEBUG
    ifs.read(reinterpret_cast<char*>(KPP), sizeof(KPP));
#else
    std::vector<s16> temp(SquareNum * fe_end * fe_end);
    ifs.read(reinterpret_cast<char*>(&temp[0]), SquareNum * fe_end * fe_end * (int)sizeof(KPP[0][0][0]));

    int index = 0;
    for (int i = 0; i < SquareNum; ++i) {
      for (int j = 0; j < fe_end; ++j) {
        for (int k = 0; k < fe_end; ++k) {
          KPP[i][j][k] = temp[index++];
        }
      }
    }
#endif
  }
  {
    std::ifstream ifs((addSlashIfNone(dirName) + "KKP32_synthesized.bin").c_str(), std::ios::binary);
    if (!ifs) {
      return false;
    }
    ifs.read(reinterpret_cast<char*>(KKP), sizeof(KKP));
  }
  {
    std::ifstream ifs((addSlashIfNone(dirName) + "KK32_synthesized.bin").c_str(), std::ios::binary);
    if (!ifs) {
      return false;
    }
    ifs.read(reinterpret_cast<char*>(KK), sizeof(KK));
  }
  return true;
}

bool Evaluater::writeSynthesized(const std::string& dirName) {
  {
    std::ofstream ofs((addSlashIfNone(dirName) + "KPP16_synthesized.bin").c_str(), std::ios::binary);
    if (!ofs) {
      return false;
    }
    std::vector<s16> temp(SquareNum * fe_end * fe_end);

    int index = 0;
    for (int i = 0; i < SquareNum; ++i) {
      for (int j = 0; j < fe_end; ++j) {
        for (int k = 0; k < fe_end; ++k) {
          temp[index++] = KPP[i][j][k];
        }
      }
    }

    ofs.write(reinterpret_cast<char*>(&temp[0]), SquareNum * fe_end * fe_end * (int)sizeof(KPP[0][0][0]));
  }
  {
    std::ofstream ofs((addSlashIfNone(dirName) + "KKP32_synthesized.bin").c_str(), std::ios::binary);
    if (!ofs) {
      return false;
    }
    ofs.write(reinterpret_cast<char*>(KKP), sizeof(KKP));
  }
  {
    std::ofstream ofs((addSlashIfNone(dirName) + "KK32_synthesized.bin").c_str(), std::ios::binary);
    if (!ofs) {
      return false;
    }
    ofs.write(reinterpret_cast<char*>(KK), sizeof(KK));
  }
  return true;
}
