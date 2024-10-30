#include <cstdio>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <string>
#include <string_view>
#include <queue>
#include <thread>
#include "outputfile.hpp"

OutputFile::~OutputFile()
{
	CleanupThread.request_stop();
	CleanupWaitCondition.notify_one();
}

std::string OutputFile::GetFilePath() const
{
	std::filesystem::path FullPath{DirectoryName};
	FullPath /= FileName;
	return FullPath.string();
}

int OutputFile::PrepareFile()
{
	std::error_code ErrorCode{};
	std::filesystem::create_directory(DirectoryName, ErrorCode);
	int ReturnValue{ErrorCode.value()};
	if (ReturnValue == 0)
	{
		File = std::fopen(GetFilePath().c_str(), "a");
		if (File == nullptr)
		{
			ReturnValue = errno;
		}
	}
	return ReturnValue;
}

void OutputFile::Cleanup()
{
	std::unique_lock TimedLock{CleanupMutex};
	std::unique_lock WriterLock{WriterMutex, std::defer_lock};
	while (!CloseTimer.TimedOut() && !CleanupThread.get_stop_token().stop_requested())
	{
		if (WriterLock.owns_lock())
		{
			WriterLock.unlock();
		}
		CleanupWaitCondition.wait_for(TimedLock, CloseTimer.GetTimeoutLength(), [this]
												{ return CleanupThread.get_stop_token().stop_requested() || CloseTimer.TimedOut(); });
		WriterLock.lock(); // gives the writer a chance to wrap up in the event that we try to close the file right as it tries to signal the cleanup routine
	}
	if (File != nullptr)
	{
		std::fclose(File);
	}
	FileClosed = true;
	File = nullptr;
}

int OutputFile::Write(const std::string &Message, const bool WithStamp)
{
	std::unique_lock WriterLock(WriterMutex);

	int FileActionResult{0};
	if (File == nullptr || FileClosed)
	{
		FileActionResult = PrepareFile();
	}

	if (FileActionResult == 0)
	{
		if (WithStamp)
		{
			FileActionResult = std::fputc('[', File);
			std::time_t CurrentTime{std::time(nullptr)};
			std::tm *TimeInfo{std::localtime(&CurrentTime)};
			char TimeBuffer[128];
			std::strftime(TimeBuffer, sizeof(TimeBuffer), "%Y-%m-%d %H:%M:%S", TimeInfo);
			FileActionResult = std::fputs(TimeBuffer, File);
			FileActionResult = std::fputs("]: ", File);
		}
		FileActionResult = std::fputs(Message.c_str(), File);
		if (FileActionResult != EOF)
		{
			FileActionResult = std::fputc('\n', File);
			CloseTimer.ResetTimeout();
		}

		if (FileActionResult == EOF)
		{
			FileActionResult = std::ferror(File);
		}
		else
		{
			FileActionResult = 0;
		}
	}

	if (File != nullptr && FileClosed)
	{
		FileClosed = false;
		CleanupThread = std::jthread(&OutputFile::Cleanup, this);
	}
	return FileActionResult;
}

int OutputFile::Write(std::queue<std::string> &Messages, const bool WithStamp)
{
	while (!Messages.empty())
	{
		int Result{Write(Messages.front(), WithStamp)};
		if (Result)
		{
			return Result;
		}
		Messages.pop();
	}
	return 0;
}
