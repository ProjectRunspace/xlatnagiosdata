#pragma once

#include <queue>
#include <set>
#include <string>
#include "logwriter.hpp"

struct PendingFile
{
	PendingFile(std::string FileName, size_t FileSize) : FileName{FileName}, FileSize{FileSize} {}
	std::string FileName;
	size_t FileSize;
};

class FileDataCollector
{
private:
	std::string SourcePath{};
	ILogWriter &Log;
	std::queue<PendingFile> PendingFiles{};
	std::queue<std::string> CompletedFiles{};
	std::queue<std::string> UnprocessedLines{};

public:
	FileDataCollector(const std::string &SourcePath, ILogWriter &Log);
	~FileDataCollector(); // assumes Log outlives this object and it is not moved or copied
	FileDataCollector(const FileDataCollector &other) = delete;
	FileDataCollector(FileDataCollector &&other) = delete;
	FileDataCollector &operator=(const FileDataCollector &other) = delete;
	FileDataCollector &operator=(FileDataCollector &&other) = delete;

	bool More() const;
	std::string GetNextLine();
};