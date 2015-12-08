﻿#ifndef APERY_BENCHMARK_HPP
#define APERY_BENCHMARK_HPP

#include "common.hpp"

class Position;
void benchmark(Position& pos);
void benchmarkElapsedForDepthN(Position& pos);
void benchmarkSearchWindow(Position& pos);
void benchmarkGenerateMoves(Position& pos);

#endif // #ifndef APERY_BENCHMARK_HPP
