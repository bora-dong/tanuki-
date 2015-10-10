#ifndef APERY_LIMITS_TYPE_HPP
#define APERY_LIMITS_TYPE_HPP

#include <atomic>

#include "color.hpp"
#include "common.hpp"
#include "score.hpp"

// ���Ԃ�T���[���̐������i�[����ׂ̍\����
struct LimitsType {
  // �R�}���h�󂯎��X���b�h����ύX����
  // ���C���X���b�h�œǂ܂�邽�� std::atomic<> ������
  std::atomic<int> time[ColorNum];
  std::atomic<int> increment[ColorNum];
  std::atomic<int> movesToGo;
  std::atomic<Ply> depth;
  std::atomic<u32> nodes;
  std::atomic<int> byoyomi;
  std::atomic<int> ponderTime;
  std::atomic<bool> infinite;
  std::atomic<bool> ponder;

  LimitsType();
  void set(const LimitsType& rh);
  std::string outputInfoString() const;
};

#endif
