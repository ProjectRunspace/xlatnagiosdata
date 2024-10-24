#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <string_view>
#include <thread>
#include <vector>
#include "outputfile.hpp"
#include "threadtimer.hpp"

enum class LogLevels
{
	None,
	Debug,
	Info,
	Warn,
	Error,
	Fatal
};

// TODO: Could improve by creating overloads that accept a callback for strings that need to be built/calculated. This would allow the log writer to only call the callback if the log level is enabled and alleviate the need for callers to check in advance. Shelved to avoid pre-emptive optimization, especially since the code already gives lots of hints for predictive CPUs to catch on.
// TODO: Could improve with variadics that push string assembly into the writer's thread where it won't impact the caller's performance. Again, probably would burn more dev time than compute time saved.
class ILogWriter
{
public:
	ILogWriter() = default;
	virtual ~ILogWriter() = default;
	ILogWriter(const ILogWriter &) = delete;
	ILogWriter &operator=(const ILogWriter &) = delete;
	ILogWriter(ILogWriter &&) = default;
	ILogWriter &operator=(ILogWriter &&) = default;
	virtual bool ShouldWrite(const LogLevels Severity) const = 0;
	virtual void WriteEntry(const LogLevels, const std::string_view &Message) = 0;
	void WriteDebug(const std::string_view &Message) { WriteEntry(LogLevels::Debug, Message); }
	void WriteInfo(const std::string_view &Message) { WriteEntry(LogLevels::Info, Message); }
	void WriteWarn(const std::string_view &Message) { WriteEntry(LogLevels::Warn, Message); }
	void WriteError(const std::string_view &Message) { WriteEntry(LogLevels::Error, Message); }
	void WriteFatal(const std::string_view &Message) { WriteEntry(LogLevels::Fatal, Message); }
	virtual void WriteUploadError(const std::string &BadStrings) = 0;

	void WriteAnnotatedEntry(const LogLevels Severity, const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		if (ShouldWrite(Severity))
		{
			std::string Message{ProcessMessage};
			Message.append(" (").append(Item).append(1, ')');
			if (!ErrorMessage.empty())
			{
				Message.append(": ").append(ErrorMessage);
			}
			WriteEntry(Severity, Message);
		}
	}

	void WriteDebugAnnoted(const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		WriteAnnotatedEntry(LogLevels::Debug, ProcessMessage, Item, ErrorMessage);
	}

	void WriteInfoAnnotated(const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		WriteAnnotatedEntry(LogLevels::Info, ProcessMessage, Item, ErrorMessage);
	}

	void WriteWarnAnnotated(const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		WriteAnnotatedEntry(LogLevels::Warn, ProcessMessage, Item, ErrorMessage);
	}

	void WriteErrorAnnotated(const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		WriteAnnotatedEntry(LogLevels::Error, ProcessMessage, Item, ErrorMessage);
	}

	void WriteFatalAnnotated(const std::string_view &ProcessMessage, const std::string_view &Item, const std::string_view &ErrorMessage = "")
	{
		WriteAnnotatedEntry(LogLevels::Fatal, ProcessMessage, Item, ErrorMessage);
	}
};

class ActiveLogWriter : public ILogWriter
{
private:
	std::condition_variable WriterCondition;
	std::mutex QueueMutex;
	std::queue<std::pair<LogLevels, std::string>> LogQueue{};
	std::queue<std::string> UploadErrors{};
	void SignalWriter(const bool StartWriter);

	LogLevels MinimumSeverity;
	std::string LogDirectory;
	std::string FailedWritesFileName;
	bool FallbackFailedWritesToSyslog{true};
	OutputFile LogFile;
	OutputFile UploadErrorsFile;
	static std::mutex WriterMutex; // this app only creates one log class but future-proof
	ThreadTimer WriteTimer{};
	void Writer();
	std::jthread WriterThread{};
	std::atomic<bool> WriterExited{true};

public:
	ActiveLogWriter(const LogLevels MinimumSeverity, const std::string_view &LogDirectory, const std::string_view &LogFileName, const std::string_view &FailedWritesFileName, const bool FallbackFailedWritesToSyslog);
	virtual ~ActiveLogWriter();
	virtual bool ShouldWrite(const LogLevels Severity) const override { return Severity >= MinimumSeverity; }
	virtual void WriteEntry(const LogLevels, const std::string_view &Message) override;
	virtual void WriteUploadError(const std::string &BadStrings) override;

	/// @brief Performs simple concatenation of the provided messages and then writes the log entry.
	/// @tparam ...T Must be nothrow convertible to std::string_view.
	/// @param LogLevel Severity of message.
	/// @param ...Messages Message components to concatenate.
	template <typename... T>
		requires std::conjunction_v<std::is_nothrow_convertible<T, std::string_view>...>
	void WriteLogEntry(const LogLevels LogLevel, const T &...Messages)
	{
		std::string ConcatenatedMessage{};
		((ConcatenatedMessage.append(Messages)), ...);
		WriteLogEntry(LogLevel, ConcatenatedMessage);
	}
};

class PassiveLogWriter : public ILogWriter
{
public:
	PassiveLogWriter() = default;
	virtual ~PassiveLogWriter() = default;
	virtual bool ShouldWrite(const LogLevels) const override { return false; }
	virtual void WriteEntry(const LogLevels, const std::string_view &) override {}
	virtual void WriteUploadError(const std::string &) override {}
};

class LogWriterFactory
{
private:
	LogWriterFactory() = default;

public:
	static std::unique_ptr<ILogWriter> CreateEmptyLogWriter() { return std::make_unique<PassiveLogWriter>(); }
	static std::unique_ptr<ILogWriter> CreateLogWriter(const LogLevels MinimumSeverity, const std::string_view &LogDirectory, const std::string_view &LogFileName, const std::string_view &FailedWritesFileName, const bool FallbackFailedWritesToSyslog);
};
