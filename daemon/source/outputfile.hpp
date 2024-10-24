#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <time.h>
#include "threadtimer.hpp"

class OutputFile
{
private:
	std::string DirectoryName;
	std::string FileName;

	FILE *File{nullptr};
	bool FallbackToProfile{false};

	int PrepareFile();
	void Cleanup();

	std::condition_variable CleanupWaitCondition;
	std::mutex WriterMutex;
	ThreadTimer CloseTimer;
	std::jthread CleanupThread{};
	std::atomic<bool> FileClosed{true};

public:
	OutputFile(const std::string DirectoryName, const std::string FileName) : DirectoryName{std::move(DirectoryName)}, FileName{std::move(FileName)} {}
	~OutputFile();
	OutputFile(const OutputFile &other) = delete;
	OutputFile(OutputFile &&other) = delete;
	OutputFile &operator=(const OutputFile &other) = delete;
	OutputFile &operator=(OutputFile &&other) = delete;

	std::string GetFilePath() const;
	int Write(const std::string &Message, const bool WithStamp = false);
	int Write(std::queue<std::string> &Messages, const bool WithStamp = false);
};