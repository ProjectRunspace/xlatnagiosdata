#pragma once

#include <atomic>
#include <chrono>
#include <ctime>

class ThreadTimer
{
private:
	std::atomic<std::time_t> LastActivity{0};
	std::chrono::seconds ThreadActivityTimeoutSeconds;

public:
	ThreadTimer(std::chrono::seconds TimeoutSeconds = std::chrono::seconds{1}) : ThreadActivityTimeoutSeconds{TimeoutSeconds} {}

	const std::chrono::seconds &GetTimeoutLength() const { return ThreadActivityTimeoutSeconds; }
	void ResetTimeout();
	bool TimedOut(); // not thread safe, intended for use within a single thread of the owning object
};