#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <sys/signal.h>
#include <thread>
#include "signalhandler.hpp"

void SystemSignalHandler::BlockAllSignals()
{
	::sigset_t SignalSet;
	::sigfillset(&SignalSet);
	::pthread_sigmask(SIG_BLOCK, &SignalSet, nullptr);
}

static void SignalHandlerThreadRoutine(std::condition_variable &DaemonAttentionRequiredCondition, std::atomic<bool> &StopRequestedFlag, std::atomic<bool> &ReloadRequestedFlag)
{
	::sigset_t SignalSet;
	::sigemptyset(&SignalSet);
	::sigaddset(&SignalSet, SIGHUP);
	::sigaddset(&SignalSet, SIGINT);
	::sigaddset(&SignalSet, SIGQUIT);
	::sigaddset(&SignalSet, SIGTERM);
	::pthread_sigmask(SIG_BLOCK, &SignalSet, nullptr);

	int Signal{0};
	std::mutex HandlerMutex;

	while (!StopRequestedFlag)
	{
		std::scoped_lock HandlerLock{HandlerMutex};
		::sigwait(&SignalSet, &Signal);
		switch (Signal)
		{
		case SIGQUIT:
			[[fallthrough]];
		case SIGINT:
			[[fallthrough]];
		case SIGTERM:
			StopRequestedFlag = true;
			break;
		case SIGHUP:
			if (ReloadRequestedFlag)
			{ // the daemon has already been told to reload
				continue;
			}
			else
			{
				ReloadRequestedFlag = true;
			}
			break;
		default:
			break;
		}
		DaemonAttentionRequiredCondition.notify_all();
	}
}

void SystemSignalHandler::Start(std::condition_variable &DaemonAttentionRequiredCondition)
{
	if (!SignalHandlerRunning)
	{
		SignalHandlerRunning = true;
		std::jthread SignalManagerThread{SignalHandlerThreadRoutine, std::ref(DaemonAttentionRequiredCondition), std::ref(StopRequested), std::ref(ReloadRequested)};
		SignalManagerThread.detach();
	}
	// TODO: right now there isn't a mechanism to unset SignalHandlerRunning.
	//       Its thread only exits when it receives a stop request, and the daemon currently always respects the request.
}
