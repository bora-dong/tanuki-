#ifndef CSA_HPP
#define CSA_HPP

#include <string>
#include <vector>
#include "position.hpp"

namespace csa {
  // CSA�t�@�C����sfen�`���֕ϊ�����
  bool toSfen(const std::string& filepath, std::vector<std::string>& sfen);
}

#endif
