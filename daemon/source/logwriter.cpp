#include <condition_variable>
#include <cstring>
#include <mutex>
#include <queue>
#include <ranges>
#include <string_view>
#include <syslog.h>
#include <thread>
#include <utility>
#include "config_constants.hpp"
#include "logwriter.hpp"
#include "threadtimer.hpp"

constexpr const std::string_view FailedOpenFile{"Failed to open file"};
constexpr const std::string_view FailedWriteFile{"Failed to write file"};
constexpr const std::string_view FailedUploadLine{"Failed to upload line: "};

std::mutex ActiveLogWriter::WriterMutex;

// helper functions
std::string FormatMessage(const std::string_view &Message, const LogLevels Severity)
{
	std::string FormattedMessage;
	switch (Severity)
	{
	case LogLevels::Debug:
		FormattedMessage = "[DEBUG]";
		break;
	case LogLevels::Info:
		FormattedMessage = "[INFO]";
		break;
	case LogLevels::Warn:
		FormattedMessage = "[WARN]";
		break;
	case LogLevels::Error:
		FormattedMessage = "[ERROR]";
		break;
	case LogLevels::Fatal:
		FormattedMessage = "[FATAL]";
		break;
	default:
		FormattedMessage = "[INFO]";
		break;
	}
	FormattedMessage.push_back(' ');
	FormattedMessage.append(Message);
	return FormattedMessage;
}

static void WriteToSysLog(const std::string &Message, const LogLevels Severity)
{
	int SysLogLevel{0};
	switch (Severity)
	{
	case LogLevels::Debug:
		SysLogLevel = LOG_DEBUG;
		break;
	case LogLevels::Info:
		SysLogLevel = LOG_INFO;
		break;
	case LogLevels::Warn:
		SysLogLevel = LOG_WARNING;
		break;
	case LogLevels::Error:
		SysLogLevel = LOG_ERR;
		break;
	case LogLevels::Fatal:
		SysLogLevel = LOG_CRIT;
		break;
	default:
		SysLogLevel = LOG_INFO;
		break;
	}
	syslog(SysLogLevel, "%s", Message.c_str());
}

// private functions
ActiveLogWriter::ActiveLogWriter(const LogLevels MinimumSeverity, const std::string_view &LogDirectory, const std::string_view &LogFileName, const std::string_view &FailedWritesFileName, const bool FallbackFailedWritesToSyslog)
	 : MinimumSeverity(MinimumSeverity),
		LogDirectory{LogDirectory},
		FailedWritesFileName{FailedWritesFileName},
		FallbackFailedWritesToSyslog{FallbackFailedWritesToSyslog},
		LogFile{std::string{LogDirectory}, std::string{LogFileName}},
		UploadErrorsFile{std::string{LogDirectory}, std::string{FailedWritesFileName}}
{
}

ActiveLogWriter::~ActiveLogWriter()
{
	WriterThread.request_stop();
	SignalWriter(false);
}

void ActiveLogWriter::Writer()
{
	WriteTimer.ResetTimeout();
	bool UseSyslog{false};
	int LogFileErrorCode{0};
	while (!WriterThread.get_stop_token().stop_requested() && !WriteTimer.TimedOut())
	{
		std::unique_lock WriterLock(WriterMutex);
		WriterCondition.wait_for(WriterLock, WriteTimer.GetTimeoutLength(), [this]
										 {
											std::scoped_lock CheckLock{QueueMutex};
											return WriterThread.get_stop_token().stop_requested() ||
											!LogQueue.empty() ||
											!UploadErrors.empty() ||
											WriteTimer.TimedOut(); });
		if (!UploadErrors.empty())
		{
			std::queue<std::string> LocalQueue{};
			{
				std::scoped_lock QueueLock{QueueMutex};
				LocalQueue.swap(UploadErrors);
			}
			while (!LocalQueue.empty())
			{
				LogFileErrorCode = UploadErrorsFile.Write(LocalQueue.front());
				if (LogFileErrorCode && FallbackFailedWritesToSyslog)
				{
					std::string UploadReportString{FormatMessage(FailedUploadLine, LogLevels::Error)};
					UploadReportString.append(std::move(LocalQueue.front()));
					WriteToSysLog(UploadReportString, LogLevels::Error);
				}
				LocalQueue.pop();
			}
		}

		if (!LogQueue.empty())
		{
			std::queue<std::pair<LogLevels, std::string>> LocalQueue{};
			{
				std::scoped_lock QueueLock{QueueMutex};
				LocalQueue.swap(LogQueue);
			}
			while (!LocalQueue.empty())
			{
				auto [Severity, Message]{std::move(LocalQueue.front())};
				std::string FormattedMessage{FormatMessage(Message, Severity)};
				if (UseSyslog)
				{
					WriteToSysLog(FormattedMessage, Severity);
				}
				else
				{
					LogFileErrorCode = LogFile.Write(FormattedMessage, true);
					if (LogFileErrorCode)
					{
						WriteErrorAnnotated(FailedWriteFile, LogFile.GetFilePath(), std::strerror(LogFileErrorCode));
						UseSyslog = true;
						continue; // skip the pop() so that the file write error happens first, then the original error goes into the syslog
					}
				}
				LocalQueue.pop();
			}
			WriteTimer.ResetTimeout();
		}
	}
	WriterExited = true;
}

void ActiveLogWriter::SignalWriter(const bool StartWriter)
{
	if (WriterExited && StartWriter)
	{
		WriterExited = false;
		WriterThread = std::jthread(&ActiveLogWriter::Writer, this);
	}
	WriterCondition.notify_one();
}

void ActiveLogWriter::WriteEntry(const LogLevels Severity, const std::string_view &Message)
{
	if (ShouldWrite(Severity))
	{
		std::scoped_lock QueueLock(QueueMutex);
		LogQueue.push(std::make_pair(Severity, std::move(std::string{Message})));
		SignalWriter(true);
	}
}

void ActiveLogWriter::WriteUploadError(const std::string &BadStrings)
{
	std::scoped_lock QueueLock(QueueMutex);
	UploadErrors.push(BadStrings);
	SignalWriter(true);
}

std::unique_ptr<ILogWriter> LogWriterFactory::CreateLogWriter(const LogLevels MinimumSeverity, const std::string_view &LogDirectory, const std::string_view &LogFileName, const std::string_view &FailedWritesFileName, const bool FallbackFailedWritesToSyslog)
{
	if (MinimumSeverity != LogLevels::None) // should have caught a "none" setting in the config load, but enforce here because the config loader checks as a convenience. it's not its job and a future updater might remove that check for separation of concerns
	{
		return std::make_unique<ActiveLogWriter>(MinimumSeverity, LogDirectory, LogFileName, FailedWritesFileName, FallbackFailedWritesToSyslog);
	}
	return std::make_unique<PassiveLogWriter>();
}
