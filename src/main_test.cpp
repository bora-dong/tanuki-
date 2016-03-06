#include <gtest/gtest.h>
#include "init.hpp"
#include "thread.hpp"
#include "search.hpp"
#include "usi.hpp"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  initTable();
  Position::initZobrist();
  auto s = std::unique_ptr<Searcher>(new Searcher);
  s->init();
  // �ꎞ�I�u�W�F�N�g�̐����Ɣj��
  std::unique_ptr<Evaluater>(new Evaluater)->init(USI::Options[OptionNames::EVAL_DIR], true);
  int statusCode = RUN_ALL_TESTS();
  s->threads.exit();
  return statusCode;
}
