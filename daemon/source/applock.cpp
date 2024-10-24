#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sys/file.h>
#include "applock.hpp"
#include "config_constants.hpp"

bool AppLock::operator()()
{
	std::error_code ErrorCode{};
	std::filesystem::path LockPath{"/var/run"};
	LockPath /= ConfigConstants::appname;

	if (!std::filesystem::exists(LockPath, ErrorCode))
	{
		std::filesystem::create_directory(LockPath, ErrorCode);
	}

	if (ErrorCode.value() != 0)
	{
		std::fprintf(stderr, "Failed to create lock directory: %s\n", ErrorCode.message().c_str());
		return false;
	}
	LockPath /= ConfigConstants::DaemonLockFileName;
	LockFile = std::fopen(LockPath.c_str(), "w");
	if (LockFile == nullptr)
	{
		std::fprintf(stderr, "Failed to open lock file \"%s\": %s\n", LockPath.c_str(), std::strerror(errno));
		return false;
	}
	bool Locked{::flock(fileno(static_cast<FILE *>(LockFile)), LOCK_EX | LOCK_NB) == 0};
	if (!Locked)
	{
		std::fprintf(stderr, "Failed to lock file: \"%s\": %s\n", LockPath.c_str(), std::strerror(errno));
		std::fclose(static_cast<FILE *>(LockFile));
		Unlock();
	}
	return Locked;
}

void AppLock::Unlock()
{
	if (LockFile != nullptr)
	{
		std::fclose(static_cast<FILE *>(LockFile));
		LockFile = nullptr;
	}
}

AppLock::~AppLock()
{
	Unlock();
}
