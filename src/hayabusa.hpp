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
  extern const std::tr2::sys::path DEFAULT_INPUT_SHOGIDOKORO_CSA_DIRECTORY_PATH;
  extern const std::tr2::sys::path DEFAULT_INPUT_SFEN_FILE_PATH;

  // SFEN�t�@�C������`��A���f���̋��t�f�[�^�ɕϊ�����
  // inputSfenFilePath SFEN�t�@�C���p�X
  // outputTeacherDataFilePath ���t�f�[�^�t�@�C���p�X
  // maxNumberOfPlays ��������ő�ǖʐ�
  bool convertSfenToTeacherData(
    const std::tr2::sys::path& inputSfenFilePath = DEFAULT_INPUT_SFEN_FILE_PATH,
    const std::tr2::sys::path& outputTeacherDataFilePath = DEFAULT_OUTPUT_TEACHER_DATA_FILE_PATH,
    int maxNumberOfPlays = INT_MAX);

  // �������̎��ȑΐ팋�ʂ����t�f�[�^�ɒǉ�����
  // inputShogidokoroCsaDirectoryPath �������̏o�͂���CSA�t�@�C�����܂܂ꂽ�f�B���N�g���p�X
  // outputTeacherFilePath �X�V����鋳�t�f�[�^�t�@�C���p�X
  // maxNumberOfPlays ��������ő�ǖʐ�
  bool addTeacherData(
    const std::tr2::sys::path& inputShogidokoroCsaDirectoryPath = DEFAULT_INPUT_SHOGIDOKORO_CSA_DIRECTORY_PATH,
    const std::tr2::sys::path& outputTeacherFilePath = DEFAULT_OUTPUT_TEACHER_DATA_FILE_PATH,
    int maxNumberOfPlays = INT_MAX);

  // HAYABUSA�w�K���\�b�h�ŏd�݂𒲐�����
  // inputTeacherFilePath ���t�f�[�^�t�@�C���p�X
  // numberOfIterations 
  bool adjustWeights(
    const std::tr2::sys::path& inputTeacherFilePath = DEFAULT_INPUT_TEACHER_DATA_FILE_PATH,
    int numberOfIterations = 32);
}

#endif
