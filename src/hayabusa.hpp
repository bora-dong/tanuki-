#ifndef HAYABUSA_HPP
#define HAYABUSA_HPP

#include "evalList.hpp"
#include "score.hpp"

namespace hayabusa {
  struct TeacherData {
    Square squareBlackKing;
    Square squareWhiteKing;
    int list0[EvalList::ListSize];
    int list1[EvalList::ListSize];
    Score material;
    // ���`�d��A���͂̋��t�M��
    // ���̒l�ɋ߂Â��悤��KPP��KKP�̒l�𒲐�����
    Score teacher;
  };

  extern const std::tr2::sys::path DEFAULT_INPUT_CSA_DIRECTORY_PATH;
  extern const std::tr2::sys::path DEFAULT_OUTPUT_TEACHER_DATA_FILE_PATH;
  extern const std::tr2::sys::path DEFAULT_INPUT_TEACHER_DATA_FILE_PATH;

  // HAYABUSA�w�K���\�b�h�Ŏg�p���鋳�t�f�[�^���쐬����
  bool createTeacherData(
    const std::tr2::sys::path& inputCsaDirectoryPath = DEFAULT_INPUT_CSA_DIRECTORY_PATH,
    const std::tr2::sys::path& outputTeacherDataFilePath = DEFAULT_OUTPUT_TEACHER_DATA_FILE_PATH,
    int maxNumberOfPlays = INT_MAX);

  // HAYABUSA�w�K���\�b�h�ŏd�݂𒲐�����
  bool adjustWeights(
    const std::tr2::sys::path& inputTeacherFilePath = DEFAULT_INPUT_TEACHER_DATA_FILE_PATH,
    int numberOfIterations = 100);
}

#endif
