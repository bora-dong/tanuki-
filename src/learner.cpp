#include "learner.hpp"

#if defined LEARN

#if 0
#define PRINT_PV
#endif

///////////////////////////////////////////////////////////////////////////////
// LearnEvaluater
///////////////////////////////////////////////////////////////////////////////
void LearnEvaluater::incParam(const Position& pos, const double dinc) {
  const Square sq_bk = pos.kingSquare(Black);
  const Square sq_wk = pos.kingSquare(White);
  const int* list0 = pos.cplist0();
  const int* list1 = pos.cplist1();
  const float f = dinc / FVScale; // same as Bonanza

  kk_raw[sq_bk][sq_wk] += f;
  for (int i = 0; i < pos.nlist(); ++i) {
    const int k0 = list0[i];
    const int k1 = list1[i];
    for (int j = 0; j < i; ++j) {
      const int l0 = list0[j];
      const int l1 = list1[j];
      kpp_raw[sq_bk][k0][l0] += f;
      kpp_raw[inverse(sq_wk)][k1][l1] -= f;
    }
    kkp_raw[sq_bk][sq_wk][k0] += f;
  }
}

void LearnEvaluater::lowerDimension() {
#define FOO(indices, oneArray, sum)										\
		for (auto indexAndWeight : indices) {							\
			if (indexAndWeight.first == std::numeric_limits<ptrdiff_t>::max()) break; \
			if (0 <= indexAndWeight.first) oneArray[ indexAndWeight.first] += sum; \
			else                           oneArray[-indexAndWeight.first] -= sum; \
		}

  // KPP
{
  std::pair<ptrdiff_t, int> indices[KPPIndicesMax];
  for (Square ksq = I9; ksq < SquareNum; ++ksq) {
    for (int i = 0; i < fe_end; ++i) {
      for (int j = 0; j < fe_end; ++j) {
        kppIndices(indices, ksq, i, j);
        FOO(indices, oneArrayKPP, kpp_raw[ksq][i][j]);
      }
    }
  }
}
// KKP
{
  std::pair<ptrdiff_t, int> indices[KKPIndicesMax];
  for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
    for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
      for (int i = 0; i < fe_end; ++i) {
        kkpIndices(indices, ksq0, ksq1, i);
        FOO(indices, oneArrayKKP, kkp_raw[ksq0][ksq1][i]);
      }
    }
  }
}
// KK
{
  std::pair<ptrdiff_t, int> indices[KKIndicesMax];
  for (Square ksq0 = I9; ksq0 < SquareNum; ++ksq0) {
    for (Square ksq1 = I9; ksq1 < SquareNum; ++ksq1) {
      kkIndices(indices, ksq0, ksq1);
      FOO(indices, oneArrayKK, kk_raw[ksq0][ksq1]);
    }
  }
}
#undef FOO
}

// float �^�Ƃ����ƋK�i�I�� 0 �͕ۏ؂���Ȃ������C�����邪���p����Ȃ����낤�B
void LearnEvaluater::clear() {
  memset(kpp_raw, 0, sizeof(kpp_raw));
  memset(kkp_raw, 0, sizeof(kkp_raw));
  memset(kk_raw, 0, sizeof(kk_raw));
}

LearnEvaluater& operator += (LearnEvaluater& lhs, LearnEvaluater& rhs) {
  for (auto lit = &(***std::begin(lhs.kpp_raw)), rit = &(***std::begin(rhs.kpp_raw)); lit != &(***std::end(lhs.kpp_raw)); ++lit, ++rit)
    *lit += *rit;
  for (auto lit = &(***std::begin(lhs.kkp_raw)), rit = &(***std::begin(rhs.kkp_raw)); lit != &(***std::end(lhs.kkp_raw)); ++lit, ++rit)
    *lit += *rit;
  for (auto lit = &(** std::begin(lhs.kk_raw)), rit = &(** std::begin(rhs.kk_raw)); lit != &(** std::end(lhs.kk_raw)); ++lit, ++rit)
    *lit += *rit;

  return lhs;
}

///////////////////////////////////////////////////////////////////////////////
// Parse2Data
///////////////////////////////////////////////////////////////////////////////
void Parse2Data::clear() {
  params.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Learner
///////////////////////////////////////////////////////////////////////////////
void Learner::learn(Position& pos, std::istringstream& ssCmd) {
  eval_.init(pos.searcher()->options["Eval_Dir"], true);
  copyFromKppkkpkkToOneArray();
  readBook(pos, ssCmd);
  size_t threadNum;
  ssCmd >> threadNum;
  ssCmd >> minDepth_;
  ssCmd >> maxDepth_;
  std::cout << "thread_num: " << threadNum
    << "\nsearch depth min, max: " << minDepth_ << ", " << maxDepth_ << std::endl;
  // ���� 1 ��Searcher, Position�������オ���Ă���̂ŁA�w�肵���� - 1 �� Searcher, Position �𗧂��グ��B
  threadNum = std::max<size_t>(0, threadNum - 1);
  std::vector<Searcher> searchers(threadNum);
  for (auto& s : searchers) {
    s.init();
    setLearnOptions(s);
    positions_.push_back(Position(DefaultStartPositionSFEN, s.threads.mainThread(), s.thisptr));
    mts_.push_back(std::mt19937(std::chrono::system_clock::now().time_since_epoch().count()));
    // �����Ńf�t�H���g�R���X�g���N�^��push_back����ƁA
    // �ꎞ�I�u�W�F�N�g��Parse2Data���X�^�b�N�ɏo���邱�ƂŃv���O������������̂ŁA�R�s�[�R���X�g���N�^�ɂ���B
    parse2Datum_.push_back(parse2Data_);
  }
  setLearnOptions(*pos.searcher());
  mt_ = std::mt19937(std::chrono::system_clock::now().time_since_epoch().count());
  for (int i = 0; ; ++i) {
    std::cout << "iteration " << i << std::endl;
    std::cout << "parse1 start" << std::endl;
    learnParse1(pos);
    std::cout << "parse2 start" << std::endl;
    learnParse2(pos);
    break;
  }
}

void Learner::setLearnMoves(Position& pos, std::set<std::pair<Key, Move> >& dict, std::string& s0, std::string& s1) {
  bookMovesDatum_.push_back(std::vector<BookMoveData>());
  BookMoveData bmdBase[ColorNum];
  bmdBase[Black].move = bmdBase[White].move = Move::moveNone();
  std::stringstream ss(s0);
  std::string elem;
  ss >> elem; // �΋ǔԍ�
  ss >> elem; // �΋Ǔ�
  bmdBase[Black].date = bmdBase[White].date = elem;
  ss >> elem; // ��薼
  bmdBase[Black].player = elem;
  ss >> elem; // ��薼
  bmdBase[White].player = elem;
  ss >> elem; // ����������������
  bmdBase[Black].winner = (elem == "1");
  bmdBase[White].winner = (elem == "2");
  pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
  StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
  while (true) {
    const std::string moveStrCSA = s1.substr(0, 6);
    const Move move = csaToMove(pos, moveStrCSA);
    // �w����̕�����̃T�C�Y������Ȃ�������A�����肾�����肷��� move.isNone() == true �ƂȂ�̂ŁAbreak ����B
    if (move.isNone())
      break;
    BookMoveData bmd = bmdBase[pos.turn()];
    bmd.move = move;
    if (dict.find(std::make_pair(pos.getKey(), move)) == std::end(dict) && bmd.winner) {
      // ���̋ǖʂ����̎w����͏��߂Č���̂ŁA�w�K�Ɏg���B
      bmd.useLearning = true;
      dict.insert(std::make_pair(pos.getKey(), move));
    }
    else
      bmd.useLearning = false;

    bookMovesDatum_.back().push_back(bmd);
    s1.erase(0, 6);

    setUpStates->push(StateInfo());
    pos.doMove(move, setUpStates->top());
  }
}

void Learner::readBook(Position& pos, std::istringstream& ssCmd) {
  std::string fileName;
  ssCmd >> fileName;
  std::cout << "book_file: " << fileName << std::endl;
  std::ifstream ifs(fileName.c_str(), std::ios::binary);
  if (!ifs) {
    std::cout << "I cannot read " << fileName << std::endl;
    exit(EXIT_FAILURE);
  }
  std::set<std::pair<Key, Move> > dict;
  std::string s0;
  std::string s1;
  int gameNum;
  ssCmd >> gameNum;
  if (gameNum == 0) {
    // 0 �Ȃ�S���̊�����ǂ�
    gameNum = std::numeric_limits<int>::max();
    std::cout << "read games: all" << std::endl;
  }
  else
    std::cout << "read games: " << gameNum << std::endl;

  for (int i = 0; i < gameNum; ++i) {
    std::getline(ifs, s0);
    std::getline(ifs, s1);
    if (!ifs) break;
    setLearnMoves(pos, dict, s0, s1);
  }
}

void Learner::setLearnOptions(Searcher& s) {
  std::string options[] = { "name Threads value 1",
    "name MultiPV value " + std::to_string(MaxLegalMoves),
    "name OwnBook value false",
    "name Max_Random_Score_Diff value 0" };
  for (auto& str : options) {
    std::istringstream is(str);
    s.setOption(is);
  }
}

template <bool Dump> size_t Learner::lockingIndexIncrement() {
  std::unique_lock<std::mutex> lock(mutex_);
  if (Dump) {
    if (index_ % 500 == 0) std::cout << index_ << std::endl;
    else if (index_ % 100 == 0) std::cout << "o" << std::flush;
    else if (index_ % 10 == 0) std::cout << "." << std::flush;
  }
  return index_++;
}

void Learner::learnParse1Body(Position& pos, std::mt19937& mt) {
  std::uniform_int_distribution<Ply> dist(minDepth_, maxDepth_);
  const size_t endNum = bookMovesDatum_.size();
  pos.searcher()->tt.clear();
  for (size_t i = lockingIndexIncrement<true>(); i < endNum; i = lockingIndexIncrement<true>()) {
    StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
    pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
    auto& gameMoves = bookMovesDatum_[i];
    for (auto& bmd : gameMoves) {
      if (bmd.useLearning) {
        std::istringstream ssCmd("depth " + std::to_string(dist(mt)));
        go(pos, ssCmd);
        pos.searcher()->threads.waitForThinkFinished();
        const auto recordIt = std::find_if(std::begin(pos.searcher()->rootMoves),
          std::end(pos.searcher()->rootMoves),
          [&](const RootMove& rm) { return rm.pv_[0] == bmd.move; });
        const Score recordScore = recordIt->score_;
        bmd.recordIsNth = recordIt - std::begin(pos.searcher()->rootMoves);
        bmd.pvBuffer.clear();
        bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(recordIt->pv_), std::end(recordIt->pv_));

        const auto recordPVSize = bmd.pvBuffer.size();

        if (abs(recordScore) < ScoreMateInMaxPly) {
          for (auto it = recordIt - 1;
          it >= std::begin(pos.searcher()->rootMoves) && FVWindow > (it->score_ - recordScore);
            --it)
          {
            bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(it->pv_), std::end(it->pv_));
          }
          for (auto it = recordIt + 1;
          it < std::end(pos.searcher()->rootMoves) && FVWindow >(recordScore - it->score_);
            ++it)
          {
            bmd.pvBuffer.insert(std::end(bmd.pvBuffer), std::begin(it->pv_), std::end(it->pv_));
          }
        }

        bmd.otherPVExist = (recordPVSize != bmd.pvBuffer.size());
      }
      setUpStates->push(StateInfo());
      pos.doMove(bmd.move, setUpStates->top());
    }
  }
}

void Learner::learnParse1(Position& pos) {
  Time t = Time::currentTime();
  index_ = 0;
  std::vector<std::thread> threads(positions_.size());
  for (size_t i = 0; i < positions_.size(); ++i)
    threads[i] = std::thread([this, i] { learnParse1Body(positions_[i], mts_[i]); });
  learnParse1Body(pos, mt_);
  for (auto& thread : threads)
    thread.join();
  auto total_move = [this] {
    u64 count = 0;
    for (auto& bmds : bookMovesDatum_)
      for (auto& bmd : bmds)
        if (bmd.useLearning)
          ++count;
    return count;
  };
  auto prediction = [this](const int i) {
    std::vector<u64> count(i, 0);
    for (auto& bmds : bookMovesDatum_)
      for (auto& bmd : bmds)
        if (bmd.useLearning)
          for (int j = 0; j < i; ++j)
            if (bmd.recordIsNth <= j)
              ++count[j];
    return count;
  };
  const auto total = total_move();
  std::cout << "\nGames = " << bookMovesDatum_.size()
    << "\nTotal Moves = " << total
    << "\nPrediction = ";
  const auto pred = prediction(8);
  for (auto elem : pred)
    std::cout << static_cast<double>(elem * 100) / total << ", ";
  std::cout << std::endl;
  std::cout << "parse1 elapsed: " << t.elapsed() / 1000 << "[sec]" << std::endl;
}

constexpr double Learner::FVPenalty() { return (0.2 / static_cast<double>(FVScale)); }

template <typename T>
void Learner::updateFV(T& v, float dv) {
  const int step = count1s(mt_() & 3); // 0~2 �̊ԂŁA���K���z�ɋ߂��`�ɂ���B
  if (0 < v) dv -= static_cast<float>(FVPenalty());
  else if (v < 0) dv += static_cast<float>(FVPenalty());

  // T �� enum ���� 0 �ɂȂ邱�Ƃ�����B
  // enum �̂Ƃ��́Astd::numeric_limits<std::underlying_type<T>::type>::max() �Ȃǂ��g���B
  static_assert(std::numeric_limits<T>::max() != 0, "");
  static_assert(std::numeric_limits<T>::min() != 0, "");
  if (0.0 <= dv && v <= std::numeric_limits<T>::max() - step) v += step;
  else if (dv <= 0.0 && std::numeric_limits<T>::min() + step <= v) v -= step;
}

void Learner::copyFromKppkkpkkToOneArray()
{
  int to = eval_.kpps_begin_index();
  for (int i = 0; i < SquareNum; ++i) {
    for (int j = 0; j < fe_end; ++j) {
      for (int k = 0; k < fe_end; ++k) {
        eval_.oneArrayKPP[to++] = Evaluater::KPP[i][j][k];
      }
    }
  }

  to = eval_.kkps_begin_index();
  for (int i = 0; i < SquareNum; ++i) {
    for (int j = 0; j < SquareNum; ++j) {
      for (int k = 0; k < fe_end; ++k) {
        eval_.oneArrayKKP[to++] = Evaluater::KKP[i][j][k];
      }
    }
  }

  to = eval_.kks_begin_index();
  for (int i = 0; i < SquareNum; ++i) {
    for (int j = 0; j < SquareNum; ++j) {
      eval_.oneArrayKK[to++] = Evaluater::KK[i][j];
    }
  }
}

void Learner::updateEval(const std::string& dirName) {
  for (size_t i = eval_.kpps_begin_index(), j = parse2Data_.params.kpps_begin_index(); i < eval_.kpps_end_index(); ++i, ++j)
    updateFV(eval_.oneArrayKPP[i], parse2Data_.params.oneArrayKPP[j]);
  for (size_t i = eval_.kkps_begin_index(), j = parse2Data_.params.kkps_begin_index(); i < eval_.kkps_end_index(); ++i, ++j)
    updateFV(eval_.oneArrayKKP[i], parse2Data_.params.oneArrayKKP[j]);
  for (size_t i = eval_.kks_begin_index(), j = parse2Data_.params.kks_begin_index(); i < eval_.kks_end_index(); ++i, ++j)
    updateFV(eval_.oneArrayKK[i], parse2Data_.params.oneArrayKK[j]);

  eval_.setEvaluate();
  eval_.write(dirName);
  eval_.writeSynthesized(dirName);
  g_evalTable.clear();
}

double Learner::sigmoid(const double x) const {
  const double a = 7.0 / static_cast<double>(FVWindow);
  const double clipx = std::max(static_cast<double>(-FVWindow), std::min(static_cast<double>(FVWindow), x));
  return 1.0 / (1.0 + exp(-a * clipx));
}

double Learner::dsigmoid(const double x) const {
  if (x <= -FVWindow || FVWindow <= x) { return 0.0; }
#if 1
  // ������������؂Ȃ̂ŁA�萔�|����K�v�͖����B
  const double a = 7.0 / static_cast<double>(FVWindow);
  return a * sigmoid(x) * (1 - sigmoid(x));
#else
  // �萔�|���Ȃ������g���B
  return sigmoid(x) * (1 - sigmoid(x));
#endif
}

void Learner::learnParse2Body(Position& pos, Parse2Data& parse2Data) {
  parse2Data.clear();
  const size_t endNum = bookMovesDatum_.size();
  SearchStack ss[2];
  for (size_t i = lockingIndexIncrement<false>(); i < endNum; i = lockingIndexIncrement<false>()) {
    StateStackPtr setUpStates = StateStackPtr(new std::stack<StateInfo>());
    pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
    auto& gameMoves = bookMovesDatum_[i];
    for (auto& bmd : gameMoves) {
#if defined PRINT_PV
      pos.print();
#endif
      if (bmd.useLearning && bmd.otherPVExist) {
        const Color rootColor = pos.turn();
        int recordPVIndex = 0;
#if defined PRINT_PV
        std::cout << "recordpv: ";
#endif
        for (; !bmd.pvBuffer[recordPVIndex].isNone(); ++recordPVIndex) {
#if defined PRINT_PV
          std::cout << bmd.pvBuffer[recordPVIndex].toCSA();
#endif
          setUpStates->push(StateInfo());
          pos.doMove(bmd.pvBuffer[recordPVIndex], setUpStates->top());
        }
        // evaluate() �̍����v�Z�𖳌�������B
        ss[0].staticEvalRaw = ss[1].staticEvalRaw = ScoreNotEvaluated;
        const Score recordScore = (rootColor == pos.turn() ? evaluate(pos, ss + 1) : -evaluate(pos, ss + 1));
#if defined PRINT_PV
        std::cout << ", score: " << recordScore << std::endl;
#endif
        for (int jj = recordPVIndex - 1; 0 <= jj; --jj) {
          pos.undoMove(bmd.pvBuffer[jj]);
        }

        double sum_dT = 0.0;
        for (int otherPVIndex = recordPVIndex + 1; otherPVIndex < static_cast<int>(bmd.pvBuffer.size()); ++otherPVIndex) {
#if defined PRINT_PV
          std::cout << "otherpv : ";
#endif
          for (; !bmd.pvBuffer[otherPVIndex].isNone(); ++otherPVIndex) {
#if defined PRINT_PV
            std::cout << bmd.pvBuffer[otherPVIndex].toCSA();
#endif
            setUpStates->push(StateInfo());
            pos.doMove(bmd.pvBuffer[otherPVIndex], setUpStates->top());
          }
          ss[0].staticEvalRaw = ss[1].staticEvalRaw = ScoreNotEvaluated;
          const Score score = (rootColor == pos.turn() ? evaluate(pos, ss + 1) : -evaluate(pos, ss + 1));
          const auto diff = score - recordScore;
          const double dT = (rootColor == Black ? dsigmoid(diff) : -dsigmoid(diff));
#if defined PRINT_PV
          std::cout << ", score: " << score << ", dT: " << dT << std::endl;
#endif
          sum_dT += dT;
          parse2Data.params.incParam(pos, -dT);
          for (int jj = otherPVIndex - 1; !bmd.pvBuffer[jj].isNone(); --jj) {
            pos.undoMove(bmd.pvBuffer[jj]);
          }
        }

        for (int jj = 0; jj < recordPVIndex; ++jj) {
          setUpStates->push(StateInfo());
          pos.doMove(bmd.pvBuffer[jj], setUpStates->top());
        }
        parse2Data.params.incParam(pos, sum_dT);
        for (int jj = recordPVIndex - 1; 0 <= jj; --jj) {
          pos.undoMove(bmd.pvBuffer[jj]);
        }
      }
      setUpStates->push(StateInfo());
      pos.doMove(bmd.move, setUpStates->top());
    }
  }
}

void Learner::learnParse2(Position& pos) {
  const int MaxStep = 32;
  for (int step = 1; step <= MaxStep; ++step) {
    double startTime = clock() / double(CLOCKS_PER_SEC);
    std::cout << "step " << step << "/" << MaxStep << " " << std::flush;
    index_ = 0;
    std::vector<std::thread> threads(positions_.size());
    for (size_t i = 0; i < positions_.size(); ++i)
      threads[i] = std::thread([this, i] { learnParse2Body(positions_[i], parse2Datum_[i]); });
    learnParse2Body(pos, parse2Data_);
    for (auto& thread : threads)
      thread.join();

    for (auto& parse2 : parse2Datum_) {
      parse2Data_.params += parse2.params;
    }
    parse2Data_.params.lowerDimension();
    std::cout << "update eval ... " << std::flush;
    updateEval(pos.searcher()->options["Eval_Dir"]);
    std::cout << "done" << std::endl;
    double endTime = clock() / double(CLOCKS_PER_SEC);
    std::cout << "parse2 1 step elapsed: " << (endTime - startTime) << "[sec]" << std::endl;
    print();
  }
}

void Learner::print() {
  for (Rank r = Rank9; r < RankNum; ++r) {
    for (File f = FileA; FileI <= f; --f) {
      const Square sq = makeSquare(f, r);
      printf("%5d", Evaluater::KPP[B2][f_gold + C2][f_gold + sq]);
    }
    printf("\n");
  }
  printf("\n");
  fflush(stdout);
}

#endif
