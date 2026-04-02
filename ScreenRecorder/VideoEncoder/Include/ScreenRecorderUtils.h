#pragma once
#include <chrono>

namespace screen_recorder 
{
	inline uint64_t GetCurrentNanoseconds() 
	{
		auto now = std::chrono::high_resolution_clock::now();
		auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
		return static_cast<uint64_t>(ns);
	}
}  // namespace screen_recorder
