#include <filesystem>
#include <fstream>
#include "csa.hpp"
#include "search.hpp"
#include "string_util.hpp"
#include "time_util.hpp"

using namespace std::tr2::sys;

namespace
{
  int getNumberOfFiles(const std::tr2::sys::path& directory)
  {
    int numberOfFiles = 0;
    for (std::tr2::sys::recursive_directory_iterator it(directory);
    it != std::tr2::sys::recursive_directory_iterator();
      ++it)
    {
      if (++numberOfFiles % 100000 == 0) {
        std::cout << numberOfFiles << std::endl;
      }
    }
    return numberOfFiles;
  }
}

const std::tr2::sys::path csa::DEFAULT_INPUT_CSA1_FILE_PATH = "../2chkifu_csa/2chkifu.csa1";
const std::tr2::sys::path csa::DEFAULT_OUTPUT_SFEN_FILE_PATH = "../bin/kifu.sfen";

bool csa::toSfen(const std::tr2::sys::path& filepath, std::vector<std::string>& sfen) {
  sfen.clear();
  sfen.push_back("startpos");
  sfen.push_back("moves");

  Position position;
  setPosition(position, string_util::concat(sfen));

  std::ifstream ifs(filepath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open a CSA file." << std::endl;
    return false;
  }

  // Position::doMove()�͑O��ƈႤ�A�h���X�Ɋm�ۂ��ꂽStateInfo��v�����邽��
  // list���g���ĉߋ���StateInfo��ێ�����B
  std::list<StateInfo> stateInfos;

  std::string line;
  while (std::getline(ifs, line)) {
    // �������̏o�͂���CSA�̎w����̖�����",T1"�ȂǂƂ�����
    // ","�ȍ~���폜����
    if (line.find(',') != std::string::npos) {
      line = line.substr(0, line.find(','));
    }

    if (line.size() != 7 || (line[0] != '+' && line[0] != '-')) {
      continue;
    }

    std::string csaMove = line.substr(1);
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
  std::ifstream ifs(filepath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open a CSA file." << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    if (line.find("%TORYO") == 0) {
      return true;
    }
  }

  return false;
}

bool csa::isTanukiBlack(const std::tr2::sys::path& filepath) {
  std::ifstream ifs(filepath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open a CSA file." << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    if (line.find("N+tanuki-") == 0) {
      return true;
    }
  }

  return false;
}

Color csa::getWinner(const std::tr2::sys::path& filepath) {
  assert(isFinished(filepath));

  std::ifstream ifs(filepath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open a CSA file." << std::endl;
    return ColorNum;
  }

  char turn = 0;
  std::string line;
  while (std::getline(ifs, line)) {
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

  return ColorNum;
}

// ������̔z����X�y�[�X��؂�Ō�������
static void concat(const std::vector<std::string>& words, std::string& out) {
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
    std::cout << "!!! Failed to open the input directory: inputDirectoryPath="
      << inputDirectoryPath
      << std::endl;
    return false;
  }

  std::ofstream ofs(outputFilePath, std::ios::out);
  if (!ofs.is_open()) {
    std::cout << "!!! Failed to create the output file: outputTeacherFilePath="
      << outputFilePath
      << std::endl;
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
    std::vector<std::string> sfen;
    if (!toSfen(inputFilePath, sfen)) {
      std::cout << "!!! Failed to convert the input csa file to SFEN: inputFilePath="
        << inputFilePath
        << std::endl;
      continue;
    }

    std::string line;
    concat(sfen, line);
    ofs << line << std::endl;
  }

  return true;
}

bool csa::convertCsa1LineToSfen(
  const std::tr2::sys::path& inputFilePath,
  const std::tr2::sys::path& outputFilePath) {
  std::ifstream ifs(inputFilePath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open the input file: inputFilePath="
      << inputFilePath
      << std::endl;
    return false;
  }

  std::ofstream ofs(outputFilePath, std::ios::out);
  if (!ofs.is_open()) {
    std::cout << "!!! Failed to create the output file: outputTeacherFilePath="
      << outputFilePath
      << std::endl;
    return false;
  }

  std::string line;
  while (std::getline(ifs, line)) {
    int kifuIndex;
    std::stringstream ss(line);
    ss >> kifuIndex;

    // �i���󋵕\��
    if (kifuIndex % 1000 == 0) {
      std::cout << kifuIndex << std::endl;
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
    pos.set(DefaultStartPositionSFEN, pos.searcher()->threads.mainThread());
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

    ofs << std::endl;
  }

  return true;
}

bool csa::readCsa(const std::tr2::sys::path& filepath, GameRecord& gameRecord)
{
  gameRecord.gameRecordIndex = 0;
  gameRecord.date = "??/??/??";
  gameRecord.winner = 0;
  gameRecord.leagueName = "???";
  gameRecord.strategy = "???";

  Position position;
  setPosition(position, "startpos moves");

  std::ifstream ifs(filepath);
  if (!ifs.is_open()) {
    std::cout << "!!! Failed to open the input file: filepath="
      << filepath
      << std::endl;
    return false;
  }

  // Position::doMove()�͑O��ƈႤ�A�h���X�Ɋm�ۂ��ꂽStateInfo��v�����邽��
  // list���g���ĉߋ���StateInfo��ێ�����B
  std::list<StateInfo> stateInfos;

  std::string line;
  bool toryo = false;
  while (std::getline(ifs, line)) {
    // �������̏o�͂���CSA�̎w����̖�����",T1"�ȂǂƂ�����
    // ","�ȍ~���폜����
    if (line.find(',') != std::string::npos) {
      line = line.substr(0, line.find(','));
    }

    if (line.find("N+") == 0) {
      gameRecord.blackPlayerName = line.substr(2);
    }
    else if (line.find("N-") == 0) {
      gameRecord.whitePlayerName = line.substr(2);
    }
    else if (line.size() == 7 && (line[0] == '+' || line[0] == '-')) {
      std::string csaMove = line.substr(1);
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

      gameRecord.moves.push_back(move);
    }
    else if (line.find(gameRecord.blackPlayerName + " win") != std::string::npos) {
      gameRecord.winner = 1;
    }
    else if (line.find(gameRecord.whitePlayerName + " win") != std::string::npos) {
      gameRecord.winner = 2;
    }
    else if (line.find("toryo") != std::string::npos) {
      toryo = true;
    }
  }

  // �����ȊO�̊����̓X�L�b�v����
  if (!toryo) {
    gameRecord.numberOfPlays = -1;
  }

  gameRecord.numberOfPlays = gameRecord.moves.size();
  return true;
}

bool csa::readCsas(
  const std::tr2::sys::path& directory,
  const std::function<bool(const std::tr2::sys::path&)>& pathFilter,
  const std::function<bool(const GameRecord&)>& gameRecordFilter,
  std::vector<GameRecord>& gameRecords)
{
  std::cout << "Listing files ..." << std::endl;
  int numberOfFiles = getNumberOfFiles(directory);

  double startClockSec = clock() / double(CLOCKS_PER_SEC);
  int fileIndex = 0;
  for (std::tr2::sys::recursive_directory_iterator it(directory);
  it != std::tr2::sys::recursive_directory_iterator();
    ++it)
  {
    if (++fileIndex % 10000 == 0) {
      std::cout << time_util::formatRemainingTime(
        startClockSec, fileIndex, numberOfFiles);
    }

    if (!pathFilter(it->path())) {
      continue;
    }

    GameRecord gameRecord;
    RETURN_IF_FALSE(readCsa(it->path(), gameRecord));
    if (!gameRecordFilter(gameRecord)) {
      continue;
    }

    gameRecords.push_back(gameRecord);
  }

  return true;
}
