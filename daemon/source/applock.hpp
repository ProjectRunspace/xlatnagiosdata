#pragma once

class AppLock
{
private:
	void *LockFile{nullptr};

public:
	AppLock() = default;
	~AppLock();
	AppLock(const AppLock &) = delete;
	AppLock &operator=(const AppLock &) = delete;
	AppLock(AppLock &&) = delete;
	AppLock &operator=(AppLock &&) = delete;

	bool operator()();
	void Unlock();
};