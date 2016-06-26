﻿#ifndef _MOVE_PICKER_H_
#define _MOVE_PICKER_H_

#include "shogi.h"

// -----------------------
//   MovePicker
// -----------------------

// 探索部ごとに必要なMovePickerが異なるので、必要なものをincludeして使う。

#ifdef USE_MOVE_PICKER_2015
#include "extra/move_picker_2015.h"
#endif

#ifdef USE_MOVE_PICKER_2016Q2
#include "extra/move_picker_2016Q2.h"
#endif

#endif // _MOVE_PICKER_H_
