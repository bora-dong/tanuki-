#ifndef _KIFU_READER_H_
#define _KIFU_READER_H_

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "experimental_learner.h"
#include "position.h"
#include "shogi.h"

namespace Learner {

  class KifuReader {
  public:
    KifuReader(const std::string& file_path, bool shuffle);
    virtual ~KifuReader();
    // �����t�@�C������f�[�^��1�ǖʓǂݍ���
    // �����̓ǂݍ��݂͑Ώ̂̃t�H���_���̕����̃t�@�C������s��
    // �t�@�C���̏��Ԃ��V���b�t�����A
    // �擪�̃t�@�C�����珇��kBatchSize�ǂ��ʓǂݍ��݁A
    // �ēx�V���b�t���A���̌�num_records���Ԃ�
    bool Read(Record& record);
    bool Close();

  private:

    const std::string file_path_;
    FILE* file_ = nullptr;
    int record_index_ = 0;
    std::vector<Record> records_;
    std::vector<int> permutation_;
    bool shuffle_;

    bool EnsureOpen();
  };

}

#endif
