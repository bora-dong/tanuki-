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

  // CSA�t�@�C�����Ő�肪���������ǂ�����Ԃ�
  bool isBlackWin(const std::tr2::sys::path& filepath);
}

#endif
