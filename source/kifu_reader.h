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
    KifuReader(const std::string& folder_name, bool shuffle);
    virtual ~KifuReader();
    // �����t�@�C������f�[�^��ǂݍ���
    // �����̓ǂݍ��݂͑Ώ̂̃t�H���_���̕����̃t�@�C������s��
    // �t�@�C���̏��Ԃ��V���b�t�����A
    // �擪�̃t�@�C�����珇��kBatchSize�ǂ��ʓǂݍ��݁A
    // �ēx�V���b�t���A���̌�num_records���Ԃ�
    bool Read(int num_records, std::vector<Record>& records);
    bool Close();

  private:
    // �����t�@�C������f�[�^��1�ǖʓǂݍ���
    bool Read(Record& record);

    const std::string folder_name_;
    std::vector<std::string> file_paths_;
    FILE* file_;
    int loop_ = 0;
    int file_index_ = 0;
    int record_index_ = 0;
    std::vector<Record> records_;
    bool shuffle_;

    bool EnsureOpen();
  };

}

#endif
