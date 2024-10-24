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

constexpr const size_t MaxBlockSize{1024};									  // 1024 lines per block (approximate; may go over for large files with many small lines)
constexpr const size_t MaxFileLineLength{1024};								  // 1024 characters per line (also approximate and seems high, but it's a reasonable start)
constexpr const size_t MaxReadChunk{MaxFileLineLength * MaxBlockSize}; // max amount of data to read into a block before sending block to caller
static_assert(MaxReadChunk < std::numeric_limits<long>::max(), "MaxReadChunk cannot exceed the maximum size of a long");

// collector logging constants
constexpr const std::string_view AddedPerfFileForProcessing{"Added file for perfdata processing"};
constexpr const std::string_view DeleteFile{"Delete file"};
constexpr const std::string_view FileRead{"Read file"};
constexpr const std::string_view FileSeek{"Move to position in file"};
constexpr const std::string_view SpoolDirectory{"Locating spool directory"};
constexpr const std::string_view ExtractedLine{"Extracted cleaned line"};
constexpr const std::string_view GettingNextBlock{"Getting next data block from disk"};
constexpr const std::string_view IndecipherableContent{"Indecipherable content in file"};
constexpr const std::string_view NoMoreLines{"No more lines to process"};
constexpr const std::string_view OpenFile{"Open file"};
constexpr const std::string_view RolledBackCharacters{"Rolled back characters in a partial line"};
constexpr const std::string_view SkippedEmpty{"Skipped empty file"};

// returns true if it had to log error, false if it logged a debug message
static bool LogIOError(ILogWriter &Log, const std::string_view &Activity, const std::string_view &Item, std::set<std::string> &ItemLog, FILE *CurrentFile)
{
	int &ErrorCode = errno;
	if (CurrentFile)
	{
		ErrorCode = std::ferror(CurrentFile);
	}
	if (ErrorCode != 0)
	{
		ItemLog.insert(std::string{Item});
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

// TODO: this might be a lot for a single function
static void GetNextLineBlock(std::queue<std::string> &UnprocessedLines, std::queue<PendingFile> &PendingFiles, std::queue<std::string> &CompletedFiles, ILogWriter &Log, std::set<std::string> &ErrorFiles)
{
	if (!PendingFiles.empty())
	{
		errno = 0;
		size_t CharsProcessed{0};
		auto GetCharsToRead{[&]() -> size_t
								  {
									  if (CharsProcessed < MaxReadChunk)
									  {
										  size_t MaxChars{MaxReadChunk - CharsProcessed};
										  return PendingFiles.front().FileSize < MaxChars ? PendingFiles.front().FileSize : MaxChars;
									  }
									  return 0;
								  }};
		std::vector<char> Buffer{};
		size_t CharsToRead;
		while (!PendingFiles.empty() && UnprocessedLines.size() < MaxBlockSize && (CharsToRead = GetCharsToRead()) > 0)
		{
			bool FileErrorState{false};
			auto const &[FileName, FileSize, StartPosition]{PendingFiles.front()};
			if (Buffer.size() < CharsToRead)
			{
				Buffer.resize(CharsToRead);
			}

			auto CurrentFile{fopen(PendingFiles.front().FileName.c_str(), "r")};
			FileErrorState = LogIOError(Log, OpenFile, PendingFiles.front().FileName, ErrorFiles, CurrentFile);
			size_t CharactersRead{0};
			if (CurrentFile)
			{
				fseek(CurrentFile, StartPosition, SEEK_SET);
				FileErrorState = LogIOError(Log, FileSeek, PendingFiles.front().FileName, ErrorFiles, CurrentFile);
				if (!std::feof(CurrentFile) && !std::ferror(CurrentFile) && CharactersRead < CharsToRead)
				{
					CharactersRead += std::fread(Buffer.data(), sizeof(char), CharsToRead, CurrentFile);
					FileErrorState = LogIOError(Log, FileRead, PendingFiles.front().FileName, ErrorFiles, CurrentFile);
				}
			}
			if (CurrentFile && !FileErrorState)
			{
				if (std::feof(CurrentFile))
				{
					CompletedFiles.push(std::move(PendingFiles.front().FileName));
				}
				else
				{
					PendingFiles.front().StartPosition = ftell(CurrentFile);
				}
			}
			if (CurrentFile == nullptr || std::feof(CurrentFile) || FileErrorState)
			{
				PendingFiles.pop();
			}
			if (CurrentFile != nullptr)
			{
				std::fclose(CurrentFile);
			}

			if (CharactersRead > 0)
			{
				// non zero-length file always ends with \n.
				// start at the end and walk backward until we find a newline
				// this allows proper reset of the file position for the next read and prevents attempts to parse partial lines
				long NonNewlineChars{0};
				for (auto BufferIterator{Buffer.crbegin()}; BufferIterator != Buffer.crend() && *BufferIterator != '\n'; BufferIterator++)
				{
					NonNewlineChars++;
				}
				if (NonNewlineChars == static_cast<long>(CharactersRead) - 1)
				{
					// TODO: this means that there isn't a newline in the entire thing, which is probably ill-formed input
					Log.WriteWarnAnnotated(IndecipherableContent, PendingFiles.front().FileName, std::string_view{Buffer.data(), CharactersRead});
				}
				else if (NonNewlineChars > 0)
				{
					// if we have non-newline characters, move the file position to the start of the last line
					// TODO: if this is somehow less than zero, potential for lost perfdata, but almost certainly means ill-formed input. loop will handle gracefully
					PendingFiles.front().StartPosition -= NonNewlineChars;
					Log.WriteDebugAnnoted(RolledBackCharacters, std::to_string(NonNewlineChars));
				}
				std::string_view BufferView{Buffer.data(), CharactersRead - NonNewlineChars};
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
					PendingFiles.push(std::move(NextFile));
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
			GetNextLineBlock(UnprocessedLines, PendingFiles, CompletedFiles, Log, ErrorFiles);
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
	std::set<std::string> DestructorFileLog{}; // not doing anything with this, used for LogIOError
	while (!CompletedFiles.empty())
	{
		std::remove(CompletedFiles.front().c_str());
		LogIOError(Log, DeleteFile, CompletedFiles.front(), DestructorFileLog, nullptr);
		CompletedFiles.pop();
	}
}
