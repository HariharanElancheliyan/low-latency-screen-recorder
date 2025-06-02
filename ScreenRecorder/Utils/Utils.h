#pragma once
#include <string>
#include <chrono>

namespace RecorderUtils
{
	static std::wstring GetCurrentDateTime()
	{
		auto now = std::chrono::system_clock::now();

		std::time_t current_time = std::chrono::system_clock::to_time_t(now);
		std::tm local_time;

		localtime_s(&local_time, &current_time);

		std::wostringstream time_stream;
		time_stream << std::put_time(&local_time, L"%d-%m-%Y_%H-%M-%S");

		return time_stream.str();
	}
}