#ifndef CSA_HPP
#define CSA_HPP

#include <filesystem>
#include <string>
#include <vector>

#include "position.hpp"
#include "game_record.hpp"

namespace csa {
  extern const std::tr2::sys::path DEFAULT_INPUT_CSA1_FILE_PATH;
  extern const std::tr2::sys::path DEFAULT_OUTPUT_SFEN_FILE_PATH;

  // CSA�t�@�C����sfen�`���֕ϊ�����
  bool toSfen(const std::tr2::sys::path& filepath, std::vector<std::string>& sfen);

  // CSA�t�@�C�����������I����Ă��邩�ǂ�����Ԃ�
  bool isFinished(const std::tr2::sys::path& filepath);

  // CSA�t�@�C������tanuki-����肩�ǂ�����Ԃ�
  bool isTanukiBlack(const std::tr2::sys::path& filepath);

  // CSA�t�@�C�����łǂ��炪����������Ԃ�
  // ���������̏ꍇ��ColorNum��Ԃ�
  Color getWinner(const std::tr2::sys::path& filepath);

  // floodgate��CSA�t�@�C����SFEN�`���֕ϊ�����
  bool convertCsaToSfen(
    const std::tr2::sys::path& inputDirectoryPath,
    const std::tr2::sys::path& outputFilePath);

  // 2chkifu.csa1��SFEN�`���֕ϊ�����
  bool convertCsa1LineToSfen(
    const std::tr2::sys::path& inputFilePath = DEFAULT_INPUT_CSA1_FILE_PATH,
    const std::tr2::sys::path& outputFilePath = DEFAULT_OUTPUT_SFEN_FILE_PATH);

  // CSA�t�@�C����ǂݍ���
  bool readCsa(const std::tr2::sys::path& filepath, GameRecord& gameRecord);

  // �T�u�f�B���N�g�����܂߂�CSA�t�@�C����ǂݍ���
  // filter��true�ƂȂ�t�@�C���̂ݏ�������
  bool readCsas(
    const std::tr2::sys::path& directory,
    const std::function<bool(const std::tr2::sys::path&)>& pathFilter,
    const std::function<bool(const GameRecord&)>& gameRecordFilter,
    std::vector<GameRecord>& gameRecords);

  // CSA1�t�@�C����ǂݍ���
  bool readCsa1(
    const std::tr2::sys::path& filepath,
    std::vector<GameRecord>& gameRecords);

  // CSA1�t�@�C����ۑ�����
  bool writeCsa1(
    const std::tr2::sys::path& filepath,
    const std::vector<GameRecord>& gameRecords);

  // CSA1�t�@�C�����}�[�W����
  bool mergeCsa1s(
    const std::vector<std::tr2::sys::path>& inputFilepaths,
    const std::tr2::sys::path& outputFilepath);
}

#endif
