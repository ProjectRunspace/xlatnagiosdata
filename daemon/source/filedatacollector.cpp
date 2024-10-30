#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <filesystem>
#include <limits>
#include <queue>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "filedatacollector.hpp"
#include "logwriter.hpp"
#include "utility.hpp"

constexpr const size_t MaxFileSize{std::numeric_limits<long>::max()};
static_assert(MaxFileSize <= std::numeric_limits<size_t>::max(), "MaxFileSize cannot exceed maximum size of type long");
constexpr const size_t MaxBlockSize{1024}; // 1024 lines per block

// collector logging constants
constexpr const std::string_view AddedPerfFileForProcessing{"Added file for perfdata processing"};
constexpr const std::string_view DeleteFile{"Delete file"};
constexpr const std::string_view FileRead{"Read file"};
constexpr const std::string_view FileSeek{"Move to position in file"};
constexpr const std::string_view FileTooLarge{"File too large to process"};
constexpr const std::string_view SpoolDirectory{"Locating spool directory"};
constexpr const std::string_view ExtractedLine{"Extracted cleaned line"};
constexpr const std::string_view GettingNextBlock{"Getting next data block from disk"};
constexpr const std::string_view NoMoreLines{"No more lines to process"};
constexpr const std::string_view OpenFile{"Open file"};
constexpr const std::string_view SkippedEmpty{"Skipped empty file"};

// returns true if it had to log error, false if it logged a debug message
static bool LogIOError(ILogWriter &Log, const std::string_view &Activity, const std::string_view &Item, FILE *CurrentFile)
{
	int &ErrorCode = errno;
	if (CurrentFile)
	{
		ErrorCode = std::ferror(CurrentFile);
	}
	if (ErrorCode != 0)
	{
		Log.WriteErrorAnnotated(Activity, Item, std::strerror(ErrorCode));
		errno = 0;
		if (CurrentFile)
		{
			std::clearerr(CurrentFile);
		}
		return true;
	}
	Log.WriteDebugAnnoted(Activity, Item);
	return false;
}

static void GetNextLineBlock(std::queue<std::string> &UnprocessedLines, std::queue<PendingFile> &PendingFiles, std::queue<std::string> &CompletedFiles, ILogWriter &Log)
{
	if (!PendingFiles.empty())
	{
		errno = 0;
		std::vector<char> Buffer{};
		while (!PendingFiles.empty() && UnprocessedLines.size() < MaxBlockSize)
		{
			bool FileInErrorState{false};
			auto const &[FileName, FileSize]{PendingFiles.front()};
			if (Buffer.size() < FileSize)
			{
				Buffer.resize(FileSize, '\0');
			}

			auto CurrentFile{fopen(PendingFiles.front().FileName.c_str(), "r")};
			FileInErrorState = LogIOError(Log, OpenFile, PendingFiles.front().FileName, CurrentFile);
			size_t CharactersRead{0};
			if (CurrentFile)
			{
				FileInErrorState = LogIOError(Log, FileSeek, PendingFiles.front().FileName, CurrentFile);
				if (!std::feof(CurrentFile) && !std::ferror(CurrentFile) && CharactersRead < FileSize)
				{
					CharactersRead += std::fread(Buffer.data(), sizeof(char), FileSize, CurrentFile);
					FileInErrorState = LogIOError(Log, FileRead, PendingFiles.front().FileName, CurrentFile);
				}
			}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wparentheses"
			if ((CurrentFile && !FileInErrorState) && ((CharactersRead == PendingFiles.front().FileSize)) || std::feof(CurrentFile))
			{
				CompletedFiles.push(std::move(PendingFiles.front().FileName));
			}
			PendingFiles.pop();
#pragma GCC diagnostic pop

			if (CurrentFile != nullptr)
			{
				std::fclose(CurrentFile);
			}

			if (CharactersRead > 0)
			{
				std::string_view BufferView{Buffer.data(), CharactersRead};
				Utility::DelimitedBlockProcessor RawDataProcessor{BufferView, '\n'};
				while (RawDataProcessor.More())
				{
					auto Line{RawDataProcessor.GetNextBlock()};
					if (!Line.empty())
					{
						std::string CleanedLine{};
						CleanedLine.reserve(Line.size());
						for (const auto &c : Line)
						{
							if (std::isprint(c) || c == '\t')
							{
								CleanedLine.push_back(c);
							}
						}
						if (!CleanedLine.empty())
						{
							Log.WriteDebugAnnoted(ExtractedLine, CleanedLine);
							UnprocessedLines.push(std::move(CleanedLine));
						}
					}
				}
			}
		}
	}
}

FileDataCollector::FileDataCollector(const std::string &SourcePath, ILogWriter &Log) : SourcePath{SourcePath}, Log{Log}
{
	std::error_code FSErrorCode{};
	if (std::filesystem::exists(SourcePath, FSErrorCode))
	{
		for (const auto &direntry : std::filesystem::directory_iterator(SourcePath, FSErrorCode))
		{
			std::string FilePath{direntry.path().string()};
			if (direntry.is_regular_file())
			{
				if (direntry.file_size() == 0)
				{
					Log.WriteDebugAnnoted(SkippedEmpty, FilePath);
					CompletedFiles.emplace(std::move(FilePath));
				}
				else
				{
					Log.WriteDebugAnnoted(AddedPerfFileForProcessing, FilePath);
					PendingFile NextFile{std::move(FilePath), direntry.file_size()};
					if (direntry.file_size() > MaxFileSize)
					{
						Log.WriteErrorAnnotated(FileTooLarge, NextFile.FileName, std::to_string(direntry.file_size()));
					}
					else
					{
						PendingFiles.push(std::move(NextFile));
					}
				}
			}
		}
	}
	if (FSErrorCode.value() != 0)
	{
		Log.WriteErrorAnnotated(SpoolDirectory, SourcePath, FSErrorCode.message());
	}
}

bool FileDataCollector::More() const
{
	return !PendingFiles.empty() || !UnprocessedLines.empty();
}

std::string FileDataCollector::GetNextLine()
{
	std::string ReturnLine;
	if (UnprocessedLines.empty())
	{
		Log.WriteDebug(GettingNextBlock);
		while (!PendingFiles.empty() && UnprocessedLines.size() < MaxBlockSize)
		{
			GetNextLineBlock(UnprocessedLines, PendingFiles, CompletedFiles, Log);
		}
	}
	if (UnprocessedLines.empty())
	{
		Log.WriteDebug(NoMoreLines);
		ReturnLine = std::string{};
	}
	else
	{
		ReturnLine = std::move(UnprocessedLines.front());
		UnprocessedLines.pop();
	}
	return ReturnLine;
}

FileDataCollector::~FileDataCollector()
{
	while (!CompletedFiles.empty())
	{
		std::remove(CompletedFiles.front().c_str());
		LogIOError(Log, DeleteFile, CompletedFiles.front(), nullptr);
		CompletedFiles.pop();
	}
}
