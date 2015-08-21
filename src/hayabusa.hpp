#ifndef HAYABUSA_HPP
#define HAYABUSA_HPP

#include "evalList.hpp"

namespace hayabusa {
  struct TeacherData {
    int list0[EvalList::ListSize];
    int list1[EvalList::ListSize];
    // ���`�d��A���͂̋��t�M��
    // ���̒l�ɋ߂Â��悤��KPP��KKP�̒l�𒲐�����
    int teacher;
  };

  extern const std::tr2::sys::path DEFAULT_INPUT_CSA_DIRECTORY_PATH;
  extern const std::tr2::sys::path DEFAULT_OUTPUT_TEACHER_FILE_PATH;

  // HAYABUSA�w�K���\�b�h�Ŏg�p���鋳�t�f�[�^���쐬����
  bool createTeacherData(
    const std::tr2::sys::path& inputCsaDirectoryPath = DEFAULT_INPUT_CSA_DIRECTORY_PATH,
    const std::tr2::sys::path& outputTeacherFilePath = DEFAULT_OUTPUT_TEACHER_FILE_PATH,
    int maxNumberOfPlays = INT_MAX);
}

#endif
