#include <atomic>
#include <condition_variable>

class SystemSignalHandler
{
public:
	SystemSignalHandler() = default;
	~SystemSignalHandler() = default;

	/// @brief Blocks all signals in the calling thread
	static void BlockAllSignals();

	std::atomic_bool SignalHandlerRunning{false};
	std::atomic_bool ReloadRequested{false};
	std::atomic_bool StopRequested{false};

	void Start(std::condition_variable &DaemonAttentionRequiredCondition);
};