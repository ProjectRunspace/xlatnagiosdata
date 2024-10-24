#pragma once

#include <map>
#include <string>
#include <vector>
#include "logwriter.hpp"
#include "nagiosparser.hpp"

class InfluxTranslator
{
private:
	ILogWriter &Log;
	size_t LineLengthBase{0};
	const std::string MeasurementName;
	const std::map<const std::string, const std::string> UnitTranslationMap;
	std::string Timestamp{};
	std::map<std::string, std::string> Tags{};
	std::map<std::string, std::string> Fields{};
	std::string TranslateLine(size_t LineStart);

public:
	InfluxTranslator(ILogWriter &Log, const std::string_view &MeasurementName, const std::map<const std::string, const std::string> TranslationMap);
	InfluxTranslator(const InfluxTranslator &) = delete;
	InfluxTranslator &operator=(const InfluxTranslator &) = delete;
	InfluxTranslator(InfluxTranslator &&) = delete;
	InfluxTranslator &operator=(InfluxTranslator &&) = delete;
	~InfluxTranslator() = default;

	std::vector<std::string> TranslateNagiosData(const NagiosPerformanceRecord &NagiosData);
};