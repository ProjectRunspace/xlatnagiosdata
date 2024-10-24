#pragma once

#include <map>
#include <string>
#include <string_view>
#include "logwriter.hpp"

#include <iostream>

class Configuration
{
public:
	int DataReadDelay{0};
	std::map<const std::string, const std::string> UnitConversionMap{};
	std::string InfluxHostName{};
	long InfluxPort{};
	std::string InfluxDatabaseName{};
	std::string InfluxMeasurementName{};
	std::string NagiosSpoolDirectory{};

	Configuration() = default;
	~Configuration() = default;

	/// @brief Loads the current configuration from disk, if available. Otherwise, loads defaults. Generates a log writer based on the loaded configuration.
	/// @return std::unique_ptr<ILogWriter> The log writer to use for the duration of the program.
	std::unique_ptr<ILogWriter> Load();
};
