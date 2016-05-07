#include "timeManager.hpp"
#include <gtest/gtest.h>
#include "init.hpp"
#include "thread.hpp"
#include "search.hpp"
#include "usi.hpp"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  initTable();
  Position::initZobrist();
  Search::init();
  // �ꎞ�I�u�W�F�N�g�̐����Ɣj��
  std::unique_ptr<Evaluater>(new Evaluater)->init(Options[USI::OptionNames::EVAL_DIR], true);
  int statusCode = RUN_ALL_TESTS();
  Threads.exit();
  return statusCode;
}
