#ifndef _LEARNER_H_
#define _LEARNER_H_

#include <sstream>

#include "position.h"
#include "shogi.h"

namespace Learner
{
  struct Record {
    PackedSfen packed;
    int16_t value;
    int16_t win_color;
  };
  static_assert(sizeof(Record) == 36, "Size of Record is not 36");

  void InitializeLearner(USI::OptionsMap& o);
  void ShowProgress(const time_t& start_time, int64_t current_data, int64_t total_data,
    int64_t show_per);
  void Learn(std::istringstream& iss);
  void MeasureError();
  void BenchmarkKifuReader();
  void MeasureFillingFactor();
}

#endif
