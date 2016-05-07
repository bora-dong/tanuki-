#ifndef CSA1_HPP
#define CSA1_HPP

#include <string>
#include <vector>

#include "position.hpp"
#include "game_record.hpp"

namespace csa {
  // CSA1�t�@�C����ǂݍ���
  bool readCsa1(
    const std::string& filepath,
    Position& pos,
    std::vector<GameRecord>& gameRecords);

  // CSA1�t�@�C����ۑ�����
  bool writeCsa1(
    const std::string& filepath,
    const std::vector<GameRecord>& gameRecords);

  // CSA1�t�@�C�����}�[�W����
  bool mergeCsa1s(
    const std::vector<std::string>& inputFilepaths,
    const std::string& outputFilepath,
    Position& pos);
}

#endif
