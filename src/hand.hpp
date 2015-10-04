﻿#ifndef APERY_HAND_HPP
#define APERY_HAND_HPP

#include "common.hpp"
#include "piece.hpp"

namespace HandConstants
{
  static constexpr int HPawnShiftBits = 0;
  static constexpr int HLanceShiftBits = 6;
  static constexpr int HKnightShiftBits = 10;
  static constexpr int HSilverShiftBits = 14;
  static constexpr int HGoldShiftBits = 18;
  static constexpr int HBishopShiftBits = 22;
  static constexpr int HRookShiftBits = 25;
  static constexpr u32 HPawnMask = 0x1f << HPawnShiftBits;
  static constexpr u32 HLanceMask = 0x7 << HLanceShiftBits;
  static constexpr u32 HKnightMask = 0x7 << HKnightShiftBits;
  static constexpr u32 HSilverMask = 0x7 << HSilverShiftBits;
  static constexpr u32 HGoldMask = 0x7 << HGoldShiftBits;
  static constexpr u32 HBishopMask = 0x3 << HBishopShiftBits;
  static constexpr u32 HRookMask = 0x3 << HRookShiftBits;
  static constexpr u32 HandPieceExceptPawnMask = (HLanceMask | HKnightMask |
    HSilverMask | HGoldMask |
    HBishopMask | HRookMask);
  static constexpr int HandPieceShiftBits[HandPieceNum] = {
    HPawnShiftBits,
    HLanceShiftBits,
    HKnightShiftBits,
    HSilverShiftBits,
    HGoldShiftBits,
    HBishopShiftBits,
    HRookShiftBits
  };
  static constexpr u32 HandPieceMask[HandPieceNum] = {
    HPawnMask,
    HLanceMask,
    HKnightMask,
    HSilverMask,
    HGoldMask,
    HBishopMask,
    HRookMask
  };
  // 特定の種類の持ち駒を 1 つ増やしたり減らしたりするときに使用するテーブル
  static constexpr u32 HandPieceOne[HandPieceNum] = {
    1 << HPawnShiftBits,
    1 << HLanceShiftBits,
    1 << HKnightShiftBits,
    1 << HSilverShiftBits,
    1 << HGoldShiftBits,
    1 << HBishopShiftBits,
    1 << HRookShiftBits
  };
  static constexpr u32 BorrowMask = ((HPawnMask + (1 << HPawnShiftBits)) |
    (HLanceMask + (1 << HLanceShiftBits)) |
    (HKnightMask + (1 << HKnightShiftBits)) |
    (HSilverMask + (1 << HSilverShiftBits)) |
    (HGoldMask + (1 << HGoldShiftBits)) |
    (HBishopMask + (1 << HBishopShiftBits)) |
    (HRookMask + (1 << HRookShiftBits)));
}

// 手駒
// 手駒の状態 (32bit に pack する)
// 手駒の優劣判定を高速に行う為に各駒の間を1bit空ける。
// xxxxxxxx xxxxxxxx xxxxxxxx xxx11111  Pawn
// xxxxxxxx xxxxxxxx xxxxxxx1 11xxxxxx  Lance
// xxxxxxxx xxxxxxxx xxx111xx xxxxxxxx  Knight
// xxxxxxxx xxxxxxx1 11xxxxxx xxxxxxxx  Silver
// xxxxxxxx xxx111xx xxxxxxxx xxxxxxxx  Gold
// xxxxxxxx 11xxxxxx xxxxxxxx xxxxxxxx  Bishop
// xxxxx11x xxxxxxxx xxxxxxxx xxxxxxxx  Rook
class Hand {
public:
  Hand() {}
  explicit Hand(u32 v) : value_(v) {}
  u32 value() const { return value_; }
  template <HandPiece HP> u32 numOf() const {
    return (HP == HPawn ? ((value() & HandConstants::HPawnMask) >> HandConstants::HPawnShiftBits) :
      HP == HLance ? ((value() & HandConstants::HLanceMask) >> HandConstants::HLanceShiftBits) :
      HP == HKnight ? ((value() & HandConstants::HKnightMask) >> HandConstants::HKnightShiftBits) :
      HP == HSilver ? ((value() & HandConstants::HSilverMask) >> HandConstants::HSilverShiftBits) :
      HP == HGold ? ((value() & HandConstants::HGoldMask) >> HandConstants::HGoldShiftBits) :
      HP == HBishop ? ((value() & HandConstants::HBishopMask) >> HandConstants::HBishopShiftBits) :
      /*HP == HRook   ?*/ ((value() /*& HRookMask*/) >> HandConstants::HRookShiftBits));
  }
  u32 numOf(const HandPiece handPiece) const {
    return (value() & HandConstants::HandPieceMask[handPiece]) >> HandConstants::HandPieceShiftBits[handPiece];
  }
  // 2つの Hand 型変数の、同じ種類の駒の数を比較する必要があるため、
  // bool じゃなくて、u32 型でそのまま返す。
  template <HandPiece HP> u32 exists() const {
    return (HP == HPawn ? (value() & HandConstants::HPawnMask) :
      HP == HLance ? (value() & HandConstants::HLanceMask) :
      HP == HKnight ? (value() & HandConstants::HKnightMask) :
      HP == HSilver ? (value() & HandConstants::HSilverMask) :
      HP == HGold ? (value() & HandConstants::HGoldMask) :
      HP == HBishop ? (value() & HandConstants::HBishopMask) :
      /*HP == HRook   ?*/ (value() & HandConstants::HRookMask));
  }
  u32 exists(const HandPiece handPiece) const { return value() & HandConstants::HandPieceMask[handPiece]; }
  u32 exceptPawnExists() const { return value() & HandConstants::HandPieceExceptPawnMask; }
  // num が int だけどまあ良いか。
  void orEqual(const int num, const HandPiece handPiece) { value_ |= num << HandConstants::HandPieceShiftBits[handPiece]; }
  void plusOne(const HandPiece handPiece) { value_ += HandConstants::HandPieceOne[handPiece]; }
  void minusOne(const HandPiece handPiece) { value_ -= HandConstants::HandPieceOne[handPiece]; }
  bool operator == (const Hand rhs) const { return this->value() == rhs.value(); }
  bool operator != (const Hand rhs) const { return this->value() != rhs.value(); }
  // 手駒の優劣判定
  // 手駒が ref と同じか、勝っていれば true
  // 勝っている状態とは、全ての種類の手駒が、ref 以上の枚数があることを言う。
  bool isEqualOrSuperior(const Hand ref) const {
#if 0
    // 全ての駒が ref 以上の枚数なので、true
    return (ref.exists<HKnight>() <= this->exists<HKnight>()
      && ref.exists<HSilver>() <= this->exists<HSilver>()
      && ref.exists<HGold  >() <= this->exists<HGold  >()
      && ref.exists<HBishop>() <= this->exists<HBishop>()
      && ref.exists<HRook  >() <= this->exists<HRook  >());
#else
    // こちらは、同じ意味でより高速
    // ref の方がどれか一つでも多くの枚数の駒を持っていれば、Borrow の位置のビットが立つ。
    return ((this->value() - ref.value()) & HandConstants::BorrowMask) == 0;
#endif
  }

private:
  u32 value_;
};

#endif // #ifndef APERY_HAND_HPP
