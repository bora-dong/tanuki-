#ifndef GAME_RECORD_HPP
#define GAME_RECORD_HPP

#include <string>
#include <vector>

// �ȉ��̂悤�ȃt�H�[�}�b�g�����͂����B
// <�����ԍ�> <���t> <��薼> <��薼> <0:��������, 1:��菟��, 2:��菟��> <���萔> <���햼�O> <��`>
// <CSA1�s�`���̎w����>
//
// (��)
// 1 2003/09/08 �H���P�� �J��_�i 2 126 ���ʐ� ���̑��̐�^
// 7776FU3334FU2726FU4132KI
struct GameRecord
{
  // �����ԍ�
  int gameRecordIndex;
  // ���t
  std::string date;
  // ��薼
  std::string blackPlayerName;
  // ��薼
  std::string whitePlayerName;
  // 0:��������, 1:��菟��, 2:��菟��
  int winner;
  // ���萔
  int numberOfPlays;
  // ���햼�O
  std::string leagueName;
  // ��`
  std::string strategy;
  // �w����
  std::vector<Move> moves;
};

#endif
