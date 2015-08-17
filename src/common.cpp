﻿#include "common.hpp"

std::mt19937_64 g_randomTimeSeed(std::chrono::system_clock::now().time_since_epoch().count());

std::ostream& operator << (std::ostream& os, SyncCout sc) {
	static std::mutex m;
	if (sc == IOLock  ) { m.lock();   }
	if (sc == IOUnlock) { m.unlock(); }
	return os;
}
