#include <filesystem>
#include <fstream>
#include "csa.hpp"
#include "thread.hpp"
#include "usi.hpp"

using namespace std;
using namespace std::tr2::sys;

const std::tr2::sys::path csa::DEFAULT_INPUT_CSA1_FILE_PATH = "../2chkifu_csa/2chkifu.csa1";
const std::tr2::sys::path csa::DEFAULT_OUTPUT_SFEN_FILE_PATH = "../bin/kifu.sfen";

bool csa::toSfen(const std::tr2::sys::path& filepath, std::vector<std::string>& sfen) {
  sfen.clear();
  sfen.push_back("startpos");
  sfen.push_back("moves");

  Position position(DefaultStartPositionSFEN, g_threads.mainThread());

  ifstream ifs(filepath);
  if (!ifs.is_open()) {
    cout << "!!! Failed to open a CSA file." << endl;
    return false;
  }

  // Position::doMove()�͑O��ƈႤ�A�h���X�Ɋm�ۂ��ꂽStateInfo��v�����邽��
  // list���g���ĉߋ���StateInfo��ێ�����B
  list<StateInfo> stateInfos;

  string line;
  while (getline(ifs, line)) {
    // �������̏o�͂���CSA�̎w����̖�����",T1"�ȂǂƂ�����
    // ","�ȍ~���폜����
    if (line.find(',') != string::npos) {
      line = line.substr(0, line.find(','));
    }

    if (line.size() != 7 || (line[0] != '+' && line[0] != '-')) {
      continue;
    }

    string csaMove = line.substr(1);
    Move move = csaToMove(position, csaMove);

#if !defined NDEBUG
    if (!position.moveIsLegal(move)) {
      cout << "!!! Found an illegal move." << endl;
      break;
    }
#endif

    stateInfos.push_back(StateInfo());
    position.doMove(move, stateInfos.back());
    //position.print();

    sfen.push_back(move.toUSI());
  }

  return true;
}

bool csa::isFinished(const std::tr2::sys::path& filepath) {
  ifstream ifs(filepath);
  if (!ifs.is_open()) {
    cout << "!!! Failed to open a CSA file." << endl;
    return false;
  }

  string line;
  while (getline(ifs, line)) {
    if (line.find("%TORYO") == 0) {
      return true;
    }
  }

  return false;
}

bool csa::isTanukiBlack(const std::tr2::sys::path& filepath) {
  ifstream ifs(filepath);
  if (!ifs.is_open()) {
    cout << "!!! Failed to open a CSA file." << endl;
    return false;
  }

  string line;
  while (getline(ifs, line)) {
    if (line.find("N+tanuki-") == 0) {
      return true;
    }
  }

  return false;
}

Color csa::getWinner(const std::tr2::sys::path& filepath) {
  assert(isFinished(filepath));

  ifstream ifs(filepath);
  if (!ifs.is_open()) {
    cout << "!!! Failed to open a CSA file." << endl;
    return ColorNum;
  }

  char turn = 0;
  string line;
  while (getline(ifs, line)) {
    if (line.empty()) {
      continue;
    }
    if (line[0] == '+' || line[0] == '-') {
      turn = line[0];
    }
    if (line.find("%TORYO") == 0) {
      return turn == '+' ? Black : White;
    }
  }

  throw exception("Failed to detect which player is win.");
}

// ������̔z����X�y�[�X��؂�Ō�������
static void concat(const vector<string>& words, string& out) {
  out.clear();
  for (const auto& word : words) {
    if (!out.empty()) {
      out += " ";
    }
    out += word;
  }
}

bool csa::convertCsaToSfen(
  const std::tr2::sys::path& inputDirectoryPath,
  const std::tr2::sys::path& outputFilePath) {
  if (!is_directory(inputDirectoryPath)) {
    cout << "!!! Failed to open the input directory: inputDirectoryPath="
      << inputDirectoryPath
      << endl;
    return false;
  }

  ofstream ofs(outputFilePath, std::ios::out);
  if (!ofs.is_open()) {
    cout << "!!! Failed to create an output file: outputTeacherFilePath="
      << outputFilePath
      << endl;
    return false;
  }

  int numberOfFiles = distance(
    directory_iterator(inputDirectoryPath),
    directory_iterator());
  int fileIndex = 0;
  for (auto it = directory_iterator(inputDirectoryPath); it != directory_iterator(); ++it) {
    if (++fileIndex % 1000 == 0) {
      printf("(%d/%d)\n", fileIndex, numberOfFiles);
    }

    const auto& inputFilePath = *it;
    vector<string> sfen;
    if (!toSfen(inputFilePath, sfen)) {
      cout << "!!! Failed to convert the input csa file to SFEN: inputFilePath="
        << inputFilePath
        << endl;
      continue;
    }

    string line;
    concat(sfen, line);
    ofs << line << endl;
  }

  return true;
}

bool csa::convertCsa1LineToSfen(
  const std::tr2::sys::path& inputFilePath,
  const std::tr2::sys::path& outputFilePath) {
  std::ifstream ifs(inputFilePath);
  if (!ifs.is_open()) {
    cout << "!!! Failed to open the input file: inputFilePath="
      << inputFilePath
      << endl;
    return false;
  }

  ofstream ofs(outputFilePath, std::ios::out);
  if (!ofs.is_open()) {
    cout << "!!! Failed to create an output file: outputTeacherFilePath="
      << outputFilePath
      << endl;
    return false;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    int kifuIndex;
    std::stringstream ss(line);
    ss >> kifuIndex;

    // �i���󋵕\��
    if (kifuIndex % 1000 == 0) {
      cout << kifuIndex << endl;
    }

    std::string elem;
    ss >> elem; // �΋Ǔ����΂��B
    ss >> elem; // ���
    const std::string sente = elem;
    ss >> elem; // ���
    const std::string gote = elem;
    ss >> elem; // (0:��������,1:���̏���,2:���̏���)

    if (!std::getline(ifs, line)) {
      std::cout << "!!! header only !!!" << std::endl;
      return false;
    }

    ofs << "startpos moves";

    Position pos;
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
      line.erase(0, 6); // �擪����6�����폜

      ofs << " " << move.toUSI();

      SetUpStates->push(StateInfo());
      pos.doMove(move, SetUpStates->top());
    }

    ofs << endl;
  }

  return true;
}
