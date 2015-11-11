﻿#ifndef APERY_BOOK_HPP
#define APERY_BOOK_HPP

#include "position.hpp"

struct BookEntry {
  Key key;
  u16 fromToPro;
  u16 count;
  Score score;
};

class Book : private std::ifstream {
public:
  Book() : random_(std::random_device()()) {}
  std::tuple<Move, Score> probe(const Position& pos, const std::string& fName, const bool pickBest);
  static void init();
  static Key bookKey(const Position& pos);

private:
  bool open(const char* fName);
  void binary_search(const Key key);

  static std::mt19937_64 mt64bit_; // 定跡のhash生成用なので、seedは固定でデフォルト値を使う。
  std::mt19937_64 random_; // ハードウェア乱数をseedにして色々指すようにする。
  std::string fileName_;
  size_t size_;

  static Key ZobPiece[PieceNone][SquareNum];
  static Key ZobHand[HandPieceNum][19];
  static Key ZobTurn;
};

void makeBook(Position& pos, std::istringstream& ssCmd);

#endif // #ifndef APERY_BOOK_HPP
