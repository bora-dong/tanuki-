#ifndef HAYABUSA_HPP
#define HAYABUSA_HPP

#include <string>

namespace hayabusa {
  extern const std::tr2::sys::path DEFAULT_INPUT_DIRECTORY_PATH;
  extern const std::tr2::sys::path DEFAULT_OUTPUT_DIRECTORY_PATH;

  // ��̑唽�Ȃ̂��߂ɓ��͂̊e�Ֆʂ̕]���l���܂ރL���b�V���t�@�C�����쐬����
  bool createEvaluationCache(
    const std::tr2::sys::path& inputDirectoryPath = DEFAULT_INPUT_DIRECTORY_PATH,
    const std::tr2::sys::path& outputDirectoryPath = DEFAULT_OUTPUT_DIRECTORY_PATH,
    int maxNumberOfPlays = INT_MAX);
}

#endif
