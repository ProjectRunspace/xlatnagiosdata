#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include "config_constants.hpp"
#include "config.hpp"
#include "logwriter.hpp"

#define TOML_EXCEPTIONS 0
#include "tomlplusplus/include/toml++/toml.hpp"

constexpr const std::string_view config_file{"/etc/" __XLATPERF_PACKAGE_NAME__ "/" __XLATPERF_PACKAGE_NAME__ "d.toml"};
constexpr const std::string_view ConfigurationLoaded{"Configuration loaded"};

// log levels
static const std::map<const std::string_view, const LogLevels> LogLevelsMap{
	 {ConfigConstants::Values::debug, LogLevels::Debug},
	 {ConfigConstants::Values::info, LogLevels::Info},
	 {ConfigConstants::Values::warn, LogLevels::Warn},
	 {ConfigConstants::Values::error, LogLevels::Error},
	 {ConfigConstants::Values::fatal, LogLevels::Fatal}};

template <typename T>
concept has_size = requires(T t) {
	{ t.size() } -> std::convertible_to<std::size_t>;
};

template <typename T>
bool HasUsableValue(const T &Value)
{
	if constexpr (std::is_class_v<T>)
	{
		if constexpr (std::is_same_v<T, std::string>)
		{
			return !Value.empty();
		}
		else if constexpr (has_size<T>)
		{
			return Value.size() > 0;
		}
		else if constexpr (std::is_same_v<T, std::optional<typename T::value_type>>)
		{
			return Value.has_value() && HasUsableValue(Value.value());
		}
		else
		{
			static_assert(sizeof(T) == 0, "Unsupported type");
		}
	}
	else
	{
		return true; // means that T is a primitive, so about the only place this could be wrong is nullptr
	}
	return false; // unsure what the type is, so always return false
}

template <typename T>
void EmplaceIfNotExists(std::map<const std::string, const std::string> &Map, const std::string &KeyName, const std::optional<T> &OptVal)
{
	if (HasUsableValue(OptVal) && Map.find(KeyName) == Map.end())
	{
		Map.emplace(KeyName, OptVal.value());
	}
}

template <typename T>
void EmplaceIfNotExists(std::vector<std::string> &Vector, const std::optional<T> &OptVal)
{
	if (HasUsableValue(OptVal) && std::find(std::begin(Vector), std::end(Vector), OptVal.value()) == std::end(Vector))
	{
		Vector.emplace_back(OptVal.value());
	}
}

static std::map<const std::string, const std::string> GetConfigurationValueOrDefault(
	 const toml::table &TomlTable, const std::string_view &KeyName, std::function<std::map<const std::string, const std::string>()> SetDefaultValue)
{
	std::map<const std::string, const std::string> OutMap{};
	if (TomlTable.contains(KeyName))
	{
		if (TomlTable[KeyName].is_table())
		{
			for (const auto &element : *TomlTable[KeyName].as_table())
			{
				EmplaceIfNotExists(OutMap, std::string{element.first}, element.second.value<std::string>());
			}
		}
	}
	return OutMap.size() ? OutMap : SetDefaultValue();
}

// currently unused
// static std::vector<std::string> GetConfigurationValueOrDefault(
// 	 const toml::table &TomlTable, const std::string_view &KeyName, std::function<std::vector<std::string>()> SetDefaultValue)
// {
// 	std::vector<std::string> OutVector{};
// 	if (TomlTable.contains(KeyName))
// 	{
// 		if (TomlTable[KeyName].is_array())
// 		{
// 			for (const auto &element : *TomlTable[KeyName].as_array())
// 			{
// 				EmplaceIfNotExists(OutVector, element.value<std::string>());
// 			}
// 		}
// 		else if (TomlTable[KeyName].is_string())
// 		{
// 			EmplaceIfNotExists(OutVector, TomlTable[KeyName].value<std::string>());
// 		}
// 	}
// 	return OutVector.size() ? OutVector : SetDefaultValue();
// }

template <typename ReturnType, typename = std::enable_if<!std::is_class_v<ReturnType>>>
ReturnType GetConfigurationValueOrDefault(const toml::table &TomlTable, const std::string_view &KeyName, ReturnType DefaultValue)
{
	auto OptVal{TomlTable[KeyName].value<ReturnType>()};
	return HasUsableValue(OptVal) ? OptVal.value() : DefaultValue;
}

std::map<const std::string, const std::string> GetDefaultConversionMap()
{
	std::map<const std::string, const std::string> NewConversionMap{};
	// https://github.com/grafana/grafana/blob/main/packages/grafana-data/src/valueFormats/categories.ts
	NewConversionMap.emplace("%", "percent");
	NewConversionMap.emplace("s", "seconds");
	NewConversionMap.emplace("b", "bits");
	NewConversionMap.emplace("B", "bytes");
	NewConversionMap.emplace("kB", "deckbytes");
	NewConversionMap.emplace("KB", "deckbytes");
	NewConversionMap.emplace("KiB", "kbytes");
	NewConversionMap.emplace("MB", "decmbytes");
	NewConversionMap.emplace("MiB", "mbytes");
	NewConversionMap.emplace("GB", "decgbytes");
	NewConversionMap.emplace("GiB", "gbytes");
	NewConversionMap.emplace("TB", "dectbytes");
	NewConversionMap.emplace("TiB", "tbytes");
	NewConversionMap.emplace("PB", "decpbytes");
	NewConversionMap.emplace("PiB", "pbytes");
	return NewConversionMap;
}

static std::unique_ptr<ILogWriter> GetLog(const toml::table &TomlConfig)
{
	const toml::table &TomlLogConfig{TomlConfig.contains(ConfigConstants::Headers::logging) ? *TomlConfig[ConfigConstants::Headers::logging].as_table() : toml::table{}};
	bool LoggingEnabled{GetConfigurationValueOrDefault(TomlLogConfig, ConfigConstants::Fields::enabled, true)};

	if (LoggingEnabled)
	{
		const std::string LogLevelString{GetConfigurationValueOrDefault(TomlLogConfig, ConfigConstants::Fields::level, ConfigConstants::DefaultValues::logLevel)};
		LogLevels LogLevel;
		if (LogLevelsMap.contains(LogLevelString))
		{
			LogLevel = LogLevelsMap.at(LogLevelString);
		}
		else
		{
			LogLevel = LogLevels::Info;
		}
		std::string_view WritesFailedFileName;
		bool LogFailedWrites{GetConfigurationValueOrDefault(TomlLogConfig, ConfigConstants::Fields::save_failed_writes, true)};
		if (LogFailedWrites)
		{
			WritesFailedFileName = ConfigConstants::FailedWritesFileName;
		}
		else
		{
			WritesFailedFileName = "";
		}
		return LogWriterFactory::CreateLogWriter(LogLevel, ConfigConstants::LogRootPath, ConfigConstants::DaemonLogFileName, WritesFailedFileName, LogFailedWrites);
	}

	return LogWriterFactory::CreateEmptyLogWriter();
}

std::unique_ptr<ILogWriter> Configuration::Load()
{
	toml::table TomlConfig;
	if (std::filesystem::exists(config_file))
	{
		auto TomlParseResult{toml::parse_file(config_file)};
		if (!TomlParseResult)
		{
			std::cerr << "Unable to parse configuration file (" << config_file << "): " << TomlParseResult.error() << std::endl;
		}
		else
		{
			TomlConfig = std::move(TomlParseResult.table());
		}
	}
	else
	{
		TomlConfig = toml::table{};
	}

	std::unique_ptr<ILogWriter> Log{GetLog(TomlConfig)};

	auto DaemonConfigTable{TomlConfig.contains(ConfigConstants::Headers::daemon) ? *TomlConfig[ConfigConstants::Headers::daemon].as_table() : toml::table{}};
	DataReadDelay = GetConfigurationValueOrDefault(DaemonConfigTable, ConfigConstants::Fields::delay, ConfigConstants::DefaultValues::dataReadDelay);

	auto InfluxConfigTable{TomlConfig.contains(ConfigConstants::Headers::influx) ? *TomlConfig[ConfigConstants::Headers::influx].as_table() : toml::table{}};
	InfluxHostName = GetConfigurationValueOrDefault(InfluxConfigTable, ConfigConstants::Fields::host, ConfigConstants::DefaultValues::influxHostName);
	InfluxDatabaseName = GetConfigurationValueOrDefault(InfluxConfigTable, ConfigConstants::Fields::database, ConfigConstants::DefaultValues::influxDatabaseName);
	InfluxMeasurementName = GetConfigurationValueOrDefault(InfluxConfigTable, ConfigConstants::Fields::measurement, ConfigConstants::DefaultValues::influxMetricName);
	InfluxPort = GetConfigurationValueOrDefault(InfluxConfigTable, ConfigConstants::Fields::port, ConfigConstants::DefaultValues::influxPort);
	// todo: protocol
	// todo: user/pass

	auto NagiosConfigTable{TomlConfig.contains(ConfigConstants::Headers::nagios) ? *TomlConfig[ConfigConstants::Headers::nagios].as_table() : toml::table{}};
	NagiosSpoolDirectory = GetConfigurationValueOrDefault(NagiosConfigTable, ConfigConstants::Fields::spoolDirectory, ConfigConstants::DefaultValues::nagiosSpoolDirectory);

	UnitConversionMap = GetConfigurationValueOrDefault(TomlConfig, ConfigConstants::Headers::unitConversionMap, std::function(GetDefaultConversionMap));
	Log->WriteInfo(ConfigurationLoaded);
	return Log;
}