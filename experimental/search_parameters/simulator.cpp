#include <random>
#include <chrono>
#include <bitset>
#include <deque>
#include <list>
#include <map>
#include <queue>
#include <set>
#include <stack>
#include <vector>
#include <algorithm>
#include <functional>
#include <iterator>
#include <locale>
#include <memory>
#include <stdexcept>
#include <utility>
#include <string>
#include <fstream>
#include <ios>
#include <iostream>
#include <iosfwd>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <streambuf>
#include <complex>
#include <numeric>
#include <valarray>
#include <exception>
#include <limits>
#include <new>
#include <typeinfo>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cfloat>
#include <climits>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <cstddef>
#include <cstdarg>
#include <ctime>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <cwctype>
using namespace std;
static const double EPS = 1e-8;
static const double PI = 4.0 * atan(1.0);
static const double PI2 = 8.0 * atan(1.0);
using ll = long long;
using ull = unsigned long long;

#define ALL(c) (c).begin(), (c).end()
#define CLEAR(v) memset(v,0,sizeof(v))
#define MP(a,b) make_pair((a),(b))
#define REP(i,n) for(int i=0, i##_len=(n); i<i##_len; ++i)
#define ABS(a) ((a)>0?(a):-(a))
template<class T> T MIN(const T& a, const T& b) { return a < b ? a : b; }
template<class T> T MAX(const T& a, const T& b) { return a > b ? a : b; }
template<class T> void MIN_UPDATE(T& a, const T& b) { if (a > b) a = b; }
template<class T> void MAX_UPDATE(T& a, const T& b) { if (a < b) a = b; }

constexpr int mo = 1000000007;
int qp(int a, ll b) { int ans = 1; do { if (b & 1)ans = 1ll * ans*a%mo; a = 1ll * a*a%mo; } while (b >>= 1); return ans; }
int qp(int a, ll b, int mo) { int ans = 1; do { if (b & 1)ans = 1ll * ans*a%mo; a = 1ll * a*a%mo; } while (b >>= 1); return ans; }
int gcd(int a, int b) { return b ? gcd(b, a%b) : a; }
int dx[4] = { 1,0,-1,0 };
int dy[4] = { 0,1,0,-1 };

namespace
{
  // �����p�����[�^�[���̃��[�e�B���O
  static constexpr double RATE_BASELINE = 3000.0;
  // �����p�����[�^�[
  static std::vector<int> CORRECT_PARAMETERS;
  // �p�����[�^1.0�ɑ΂��鐮��
  static constexpr int PARAMETER_SCALE = 1024;
  // �p�����[�^�[���烌�[�e�B���O�ւ̕ϊ��W��
  static constexpr double PARAMETER_TO_RATE_SCALE = 0.005;
  // �p�����[�^�[��
  static constexpr int NUMBER_OF_PARAMETERS = 16;
  // �[��������
  static constexpr int NUMBER_OF_GAMES = 1000;
  // �p�����[�^�[����x�ɂǂ̂��炢�ω������邩
  static constexpr double PARAMETER_CHANGE_RATIO = 0.1;
  // nPm
  static double P[1024][1024];
  // �p�����[�^�[�ω����󗝂���L�Ӎ���臒l
  // �L�Ӎ��������荂���ꍇ�ɁA�ω���̃p�����[�^�[���󗝂���
  static double SIGNIFICANT_DIFFERENCE_THRESHOLD = 0.999;

  std::mt19937_64 MT;
  std::normal_distribution<> NORMAL_DISTRIBUTION;

  // ���[�e�B���O���珟�����v�Z����
  // https://ja.wikipedia.org/wiki/%E3%82%A4%E3%83%AD%E3%83%AC%E3%83%BC%E3%83%86%E3%82%A3%E3%83%B3%E3%82%B0
  double calculateWinningRate(double Ra, double Rb) {
    return 1.0 / (1.00 + pow(10.0, (Rb - Ra) / 400.0));
  }

  // �p�����[�^�[����[�����[�e�B���O���v�Z����
  double calculateRate(const vector<int>& parameters) {
    assert(CORRECT_PARAMETERS.size() == parameters.size());
    double rate = RATE_BASELINE;
    REP(i, parameters.size()) {
      double diff = parameters[i] - CORRECT_PARAMETERS[i];
      diff *= PARAMETER_TO_RATE_SCALE;
      rate -= diff * diff;
    }
    return rate;
  }
}

//TODO:��Ԃ�\���^/�\���̂��쐬����
class STATE {
public:
  //TODO:�R���X�g���N�^�ɕK�v�Ȉ�����ǉ�����
  explicit STATE();
  void next();
  void prev();
  double calculateTransitionProbability();

  std::vector<int> parameters;
  std::vector<int> prevParameters;
};

class SimulatedAnnealing {
public:
  //TODO:�R���X�g���N�^�ɕK�v�Ȉ�����ǉ�����
  SimulatedAnnealing();
  STATE run();
private:
  double calculateProbability(double score, double scoreNeighbor, double temperature);
};

//TODO:�R���X�g���N�^�ɕK�v�Ȉ�����ǉ�����
SimulatedAnnealing::SimulatedAnnealing() {
}

STATE SimulatedAnnealing::run() {
  const auto startTime = std::chrono::system_clock::now();
  STATE state;
  STATE result = state;
  int counter = 0;
  REP(loop, 100) {
    state.next();
    std::uniform_real_distribution<> dist;
    const double probability = state.calculateTransitionProbability();
    if (dist(MT) < probability) {
      //Accept
      result = state;
      fprintf(stderr, "��: rating=%f\n", calculateRate(state.parameters));
    }
    else {
      //Decline
      state.prev();
      //fprintf(stderr, "���p\n");
    }
    ++counter;
  }

  return result;
}

double SimulatedAnnealing::calculateProbability(double energy, double energyNeighbor, double temperature) {
  if (energyNeighbor < energy) {
    return 1;
  }
  else {
    const double result = exp((energy - energyNeighbor) / (temperature + 1e-9) * 1.0);
    //fprintf(stderr, "%lf -> %lf * %lf = %lf\n", energy, energyNeighbor, temperature, result);
    return result;
  }
}

// ������Ԃ����
STATE::STATE() {
  REP(i, NUMBER_OF_PARAMETERS) {
    parameters.push_back(int(NORMAL_DISTRIBUTION(MT) * PARAMETER_SCALE));
  }
  cout << "�����p�����[�^�[:" << endl;
  copy(ALL(parameters), ostream_iterator<int>(cout, ", "));
  cout << endl;
  cout << "�������[�e�B���O: " << calculateRate(parameters) << endl;
}

// �J�ڌ�̏�Ԃ����
void STATE::next() {
  prevParameters = parameters;
  std::uniform_int_distribution<> dist(0, NUMBER_OF_PARAMETERS - 1);
  int parameterIndex = dist(MT);
  int diff = int(NORMAL_DISTRIBUTION(MT) * PARAMETER_SCALE * PARAMETER_CHANGE_RATIO);
  parameters[parameterIndex] += diff;
}

// �J�ڑO�̏�Ԃɖ߂�
void STATE::prev() {
  parameters = prevParameters;
}

// �J�ڊm�����v�Z����
double STATE::calculateTransitionProbability() {
  // �[���������s�������񐔂��v�Z����
  int numberOfWins = 0;
  // �����̓p�����[�^�[�ω��O�ƕω���̃��[�e�B���O����v�Z����
  double winningRate = calculateWinningRate(
    calculateRate(parameters),
    calculateRate(prevParameters));
  std::uniform_real_distribution<> dist;
  REP(i, NUMBER_OF_GAMES) {
    if (dist(MT) < winningRate) {
      ++numberOfWins;
    }
  }

  // �L�Ӎ����v�Z����
  // �L�Ӎ��͓񍀕��z�ɂ�����0�`numberOfWins�܂ł̒l�𑫂����� / 2^(������)
  // �c�ō����Ă�̂��Ȃ��B(�v�m�F)
  double acc = 0.0;
  REP(i, numberOfWins + 1) {
    acc += P[NUMBER_OF_GAMES][i];
  }
  double significantDifference = acc / pow(2.0, NUMBER_OF_GAMES);

  // �L�Ӎ���臒l�𒴂��Ă�����100%�J�ڂ���
  // �����łȂ��ꍇ�͑J�ڂ��Ȃ�
  if (significantDifference > SIGNIFICANT_DIFFERENCE_THRESHOLD) {
    return 1.0;
  }
  else {
    return 0.0;
  }
}

int main()
{
  P[0][0] = 1.0;
  for (int i = 1; i < 1024; ++i) {
    P[i][0] = 1.0;
    for (int j = 1; j < 1024; ++j) {
      P[i][j] = P[i - 1][j - 1] + P[i - 1][j];
    }
  }

  cout << "�L�Ӎ���臒l: " << SIGNIFICANT_DIFFERENCE_THRESHOLD << endl;

  REP(i, NUMBER_OF_PARAMETERS) {
    CORRECT_PARAMETERS.push_back(int(NORMAL_DISTRIBUTION(MT) * PARAMETER_SCALE));
  }
  cout << "�����p�����[�^�[:" << endl;
  copy(ALL(CORRECT_PARAMETERS), ostream_iterator<int>(cout, ", "));
  cout << endl;
  cout << "���[�e�B���O: " << calculateRate(CORRECT_PARAMETERS) << endl;

  //REP(loop, 10) {
  //  vector<int> parameters;
  //  REP(i, NUMBER_OF_PARAMETERS) {
  //    parameters.push_back(int(NORMAL_DISTRIBUTION(MT) * PARAMETER_SCALE));
  //  }
  //  cout << calculateRate(parameters) << endl;
  //}

  STATE result = SimulatedAnnealing().run();

  cout << "���������V�~�����[�V��������" << endl;
  copy(ALL(result.parameters), ostream_iterator<int>(cout, ", "));
  cout << endl;
  cout << "���[�e�B���O: " << calculateRate(result.parameters) << endl;
}
