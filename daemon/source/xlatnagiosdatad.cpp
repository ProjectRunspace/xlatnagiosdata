#include "applock.hpp"
#include "daemon.hpp"

int main()
{
	AppLock Lock{};
	if (Lock())
	{
		N2IDaemon Daemon{};
		Daemon.Run();
		return 0;
	}
	return 1;
}
