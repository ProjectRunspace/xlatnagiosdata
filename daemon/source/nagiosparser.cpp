#include <tuple>
#include <vector>
#include "nagiosparser.hpp"
#include "utility.hpp"

constexpr const std::string_view InvalidTimestamp{"Timestamp is not a number."};
constexpr const std::string_view ExtraneousData{"Extra data found in Nagios performance record. Discarding."};

static std::tuple<std::string, std::string, std::string> ParseNagiosPerfValue(const std::string_view &RawValue)
{
	std::string Label{Utility::GetDelimitedBlock(RawValue, '=')};
	size_t Position = Label.size();
	if (RawValue.size() > Position && RawValue[Position] == '=')
	{
		Position++; // consume the =
	}
	if (Position >= RawValue.size()) // this is ill-formed, but nothing we can do about it. log it?
	{
		return std::make_tuple(Label, std::string{}, std::string{});
	}

	std::string Value{};
	auto AfterValuePosition{Utility::GetFirstNonNumericPosition(RawValue.substr(Position))};
	AfterValuePosition += Position;
	if (AfterValuePosition > Position)
	{
		Value.append(Utility::GetStringViewSubrange(RawValue, Position, AfterValuePosition - 1));
		Position = AfterValuePosition;
	}

	if (Position >= RawValue.size()) // not all measurements have a unit
	{
		return std::make_tuple(Label, Value, std::string{});
	}

	std::string Unit{Utility::GetStringViewSubrange(RawValue, Position, RawValue.size())};
	return std::make_tuple(Label, Value, Unit);
}

std::vector<NagiosPerformanceData> NagiosPerfDataParser::ParseNagiosPerformanceData(const std::string &PerfData)
{
	std::vector<NagiosPerformanceData> Result{};
	Utility::DelimitedBlockProcessor PerfDataProcessor{PerfData, ' '};
	while (PerfDataProcessor.More())
	{
		auto ParsedData{NagiosPerformanceData()};
		auto PerfDataItem{PerfDataProcessor.GetNextBlock()};

		Utility::DelimitedBlockProcessor PerfDataItemProcessor{PerfDataItem, ';'};
		while (PerfDataItemProcessor.More())
		{
			auto PerfDataItemComponent{PerfDataItemProcessor.GetNextBlock()};

			switch (PerfDataItemProcessor.GetProcessedBlocks())
			{
			case 1:
				std::tie(ParsedData.Label, ParsedData.Value, ParsedData.Unit) = ParseNagiosPerfValue(PerfDataItemComponent);
				break;
			case 2:
				ParsedData.Warn.append(PerfDataItemComponent);
				break;
			case 3:
				ParsedData.Crit.append(PerfDataItemComponent);
				break;
			case 4:
				ParsedData.Min.append(PerfDataItemComponent);
				break;
			case 5:
				ParsedData.Max.append(PerfDataItemComponent);
				break;
			default:
				break;
			}
		}
		Result.push_back(std::move(ParsedData));
	}
	return Result;
}

std::optional<NagiosPerformanceRecord> NagiosPerfDataParser::ParseNagiosPerformanceRecord(const std::string &NagiosPerfDataLine)
{
	NagiosPerformanceRecord Record{};
	std::string LineComponent{};
	size_t index{0};
	Utility::DelimitedBlockProcessor PerfDataLineProcessor{NagiosPerfDataLine, '\t'};
	while (PerfDataLineProcessor.More())
	{
		LineComponent.assign(PerfDataLineProcessor.GetNextBlock());
		switch (index)
		{
		case 0:
			if (!Utility::IsDigitsOnly(LineComponent))
			{
				Log.WriteErrorAnnotated(InvalidTimestamp, LineComponent);
				Log.WriteUploadError(NagiosPerfDataLine);
				return std::nullopt;
			}
			Record.Timestamp = LineComponent;
			break;
		case 1:
			Record.HostName = LineComponent;
			break;
		case 2:
			Record.ServiceName = LineComponent;
			break;
		case 3:
			for (auto &ParsedData : ParseNagiosPerformanceData(LineComponent))
			{
				Record.PerfData.emplace_back(ParsedData);
			}
			break;
		default:
			Log.WriteWarnAnnotated(ExtraneousData, LineComponent);
			break;
		}
		index++;
	}
	return Record;
}