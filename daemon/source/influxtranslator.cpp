#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "logwriter.hpp"
#include "influxtranslator.hpp"
#include "utility.hpp"

constexpr const std::string_view ExpectedVsActualFinalStringSize{"Expected number of chars vs actual number of chars"};

// helper functions
const std::string ConvertFromNagiosUnit(const std::string &NagiosUnit, const std::map<const std::string, const std::string> &UnitTranslationMap)
{
	auto unitsearch{UnitTranslationMap.find(NagiosUnit)};
	if (unitsearch == UnitTranslationMap.end())
	{
		return NagiosUnit;
	}
	return unitsearch->second;
}

std::string EscapeInfluxString(const std::string &InfluxString, const bool Enquote)
{
	std::string EscapedString{};
	EscapedString.reserve(InfluxString.size() + (Enquote * 2) + 5); // +5 is a magic number to over-account for the probability of escaping characters

	if (Enquote)
		EscapedString.push_back('"'); // only non-numeric fields require quoting, but cheaper for us to quote everything and let influxdb's parser worry about it
	for (const auto &Character : InfluxString)
	{
		switch (Character)
		{
		case ' ':
			[[fallthrough]];
		case ',':
			[[fallthrough]];
		case '=':
			EscapedString.push_back('\\');
			break;
		default:
			break;
		}
		EscapedString.push_back(Character);
	}
	if (Enquote)
		EscapedString.push_back('"');
	return EscapedString;
}

size_t SetItem(std::map<std::string, std::string> &TargetMap, const std::string &Key, const std::string &Value, const bool IsField)
{
	if (Value.empty())
	{
		TargetMap.erase(Key);
		return 0;
	}
	std::string FinalValue;
	if (Utility::IsNumber(Value))
	{
		FinalValue = Value;
	}
	else
	{
		FinalValue = EscapeInfluxString(Value, IsField);
	}
	TargetMap[Key] = FinalValue;
	return Key.size() + FinalValue.size() + 1; // +1 for = sign
}

std::string &AppendInfluxKVPFromMap(std::string &InfluxLine, const std::map<std::string, std::string> &KVPMap)
{
	bool First{true};
	for (const auto &[Key, Value] : KVPMap)
	{
		if (First)
		{
			First = !First;
		}
		else
		{
			InfluxLine.push_back(',');
		};
		InfluxLine.append(Key).append(1, '=').append(Value);
	}
	return InfluxLine;
}

// private functions
std::string InfluxTranslator::TranslateLine(size_t LineStart)
{
	size_t ComputedLineLength{LineLengthBase + LineStart + Tags.size() + Fields.size()}; // Tags.size() and Fields.size() because tags and fields have a comma between every entry and a space after the block
	std::string OutputLine{};
	OutputLine.reserve(ComputedLineLength);
	OutputLine.append(MeasurementName).push_back(',');
	AppendInfluxKVPFromMap(OutputLine, Tags).push_back(' ');
	AppendInfluxKVPFromMap(OutputLine, Fields).push_back(' ');
	OutputLine.append(Timestamp);
	return OutputLine;
}

// public functions
InfluxTranslator::InfluxTranslator(ILogWriter &Log, const std::string_view &MeasurementName, const std::map<const std::string, const std::string> TranslationMap)
	 : Log{Log}, MeasurementName{MeasurementName}, UnitTranslationMap{std::move(TranslationMap)}
{
	LineLengthBase = MeasurementName.size() + 1; // +1 for comma after name
}

std::vector<std::string> InfluxTranslator::TranslateNagiosData(const NagiosPerformanceRecord &NagiosData)
{
	std::vector<std::string> TranslatedData{};
	TranslatedData.reserve(NagiosData.PerfData.size());
	size_t BaseLineLength{0};
	BaseLineLength += SetItem(Tags, "host", NagiosData.HostName, false);
	BaseLineLength += SetItem(Tags, "service", NagiosData.ServiceName, false);
	Timestamp = NagiosData.Timestamp;
	BaseLineLength += Timestamp.size();
	for (const auto &PerfData : NagiosData.PerfData)
	{
		size_t LineLength{BaseLineLength};
		LineLength += SetItem(Tags, "label", PerfData.Label, false);
		LineLength += SetItem(Fields, "value", PerfData.Value, true);
		LineLength += SetItem(Fields, "warn", PerfData.Warn, true);
		LineLength += SetItem(Fields, "crit", PerfData.Crit, true);
		LineLength += SetItem(Fields, "min", PerfData.Min, true);
		LineLength += SetItem(Fields, "max", PerfData.Max, true);
		LineLength += SetItem(Tags, "unit", ConvertFromNagiosUnit(PerfData.Unit, UnitTranslationMap), true);
		TranslatedData.emplace_back(TranslateLine(LineLength));
	}
	return TranslatedData;
}