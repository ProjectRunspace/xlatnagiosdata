#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "logwriter.hpp"

struct NagiosPerformanceData
{
	std::string Label{};
	std::string Value{};
	std::string Warn{};
	std::string Crit{};
	std::string Min{};
	std::string Max{};
	std::string Unit{};
};

struct NagiosPerformanceRecord
{
public:
	std::string HostName{};
	std::string ServiceName{};
	std::string Timestamp{};
	std::vector<NagiosPerformanceData> PerfData{};
};

class NagiosPerfDataParser
{
private:
	ILogWriter &Log;
	std::vector<NagiosPerformanceData> ParseNagiosPerformanceData(const std::string &PerfData);

public:
	NagiosPerfDataParser(ILogWriter &LogWriter) : Log{LogWriter} {}
	std::optional<NagiosPerformanceRecord> ParseNagiosPerformanceRecord(const std::string &NagiosPerfDataLine);
};