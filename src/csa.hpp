#ifndef CSA_HPP
#define CSA_HPP

#include <filesystem>
#include <string>
#include <vector>
#include "position.hpp"

namespace csa {
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
    const std::tr2::sys::path& inputFilePath,
    const std::tr2::sys::path& outputFilePath);
}

#endif
