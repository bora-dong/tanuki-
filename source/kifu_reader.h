#ifndef _KIFU_READER_H_
#define _KIFU_READER_H_

#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "position.h"
#include "shogi.h"

namespace Learner {

  struct Record {
    std::string sfen;
    Value value;
  };

  class KifuReader
  {
  public:
    KifuReader(const std::string& folder_name);
    virtual ~KifuReader();
    // �����t�@�C������f�[�^��ǂݍ���
    // �����̓ǂݍ��݂͑Ώ̂̃t�H���_���̕����̃t�@�C������s��
    // �擪�̃t�@�C�����珇��kBatchSize�ǂ��ʓǂݍ��݁A
    // �V���b�t�����Ďg�p����
    // �ő��kMaxLoop��܂ŁA�����̃t�@�C���S�̂�ǂݍ���
    bool Read(Position& pos, Value& value);
    bool Close();

  private:
    const std::string folder_name_;
    std::vector<std::string> file_paths_;
    std::ifstream file_stream_;
    int loop_ = 0;
    int file_index_ = 0;
    int record_index_ = 0;
    std::vector<Record> records_;
    std::mutex mutex_;

    bool EnsureOpen();
  };

}

#endif
