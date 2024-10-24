#include "threadtimer.hpp"

#include <atomic>
#include <chrono>
#include <ctime>

#include <condition_variable>

void ThreadTimer::ResetTimeout()
{
	LastActivity = time(nullptr);
}

bool ThreadTimer::TimedOut()
{
	std::condition_variable cv;
	std::chrono::seconds TimeDiff{std::time(nullptr) - LastActivity};
	return TimeDiff > ThreadActivityTimeoutSeconds;
}
