#include <map>
#include <string>
#include <string_view>
#include <curl/curl.h>
#include "curlclient.hpp"
#include "influxclient.hpp"
#include "influxtranslator.hpp"
#include "logwriter.hpp"

// used as paths in the URL
constexpr const std::string_view CommandPing{"ping"};
constexpr const std::string_view CommandQuery{"query"};
constexpr const std::string_view CommandWrite{"write"};

// used as query parameters in the URL
constexpr const std::string_view InfluxDatabaseParameter("db");
constexpr const std::string_view InfluxQueryParameter("q");

// log messages
constexpr const std::string_view CheckHealth{"Checking Influx connectivity"};
constexpr const std::string_view ListingDatabases{"Listing Influx databases"};
constexpr const std::string_view InfluxDatabaseExists{"Influx database exists"};
constexpr const std::string_view CreatingDatabase{"Creating Influx database"};
constexpr const std::string_view Write{"Writing to Influx"};

static bool LogInfluxError(const CurlResponse &Response, ILogWriter &Log, const std::string_view &Activity)
{
	if (Response.CurlResult != CURLE_OK)
	{
		Log.WriteErrorAnnotated(Activity, std::to_string(Response.CurlResult), curl_easy_strerror(Response.CurlResult));
		return true;
	}
	if (Response.ResponseCode < 200 || Response.ResponseCode >= 300)
	{
		Log.WriteErrorAnnotated(Activity, std::to_string(Response.ResponseCode), Response.Body.value_or(std::string{}));
		return true;
	}
	Log.WriteDebugAnnoted(Activity, std::to_string(Response.ResponseCode), "Success");
	return false;
}

static CurlRequest GetInfluxRequest(const std::string_view &Path, const bool WantHeaders = false, const bool WantBody = true)
{
	CurlRequest InfluxRequest{WantHeaders, WantBody, true};
	InfluxRequest.SetPath(Path);
	return InfluxRequest;
}

static CurlRequest GetInfluxQueryRequest(const std::string_view &QueryParameter, const bool WantHeaders = false, const bool WantBody = true)
{
	CurlRequest InfluxQueryRequest{GetInfluxRequest(CommandQuery, WantHeaders, WantBody)};
	InfluxQueryRequest.AddQueryParameter(InfluxQueryParameter, QueryParameter);
	return InfluxQueryRequest;
}

static CurlResponse ShowDatabases(CurlClient &Curl)
{
	auto DatabaseQueryRequest{GetInfluxQueryRequest("SHOW DATABASES")};
	return Curl.Get(DatabaseQueryRequest);
}

static CurlResponse CreateDatabase(CurlClient &Curl, const std::string_view &DatabaseName)
{
	auto CreateDatabaseRequest{GetInfluxQueryRequest(std::string{"CREATE DATABASE \""}.append(DatabaseName).append(1, '"'))};
	return Curl.Post(CreateDatabaseRequest);
}

InfluxClient::InfluxClient(ILogWriter &Log, std::string HostName, const long Port, std::string DatabaseName, std::string MeasurementName, std::map<const std::string, const std::string> &UnitConversionMap)
	 : Log{Log}, HostName{HostName}, DatabaseName{DatabaseName}, Curl{CurlClient{Log, std::string{HostName}, Port}}, Translator{Log, MeasurementName, UnitConversionMap} {}

bool InfluxClient::TestConnection()
{
	auto PingResult{GetInfluxRequest(CommandPing)};
	return !LogInfluxError(Curl.Get(PingResult), Log, CheckHealth);
}

bool InfluxClient::CreateDatabaseIfNotExists()
{
	auto DatabaseList{ShowDatabases(Curl)};
	if (!LogInfluxError(DatabaseList, Log, ListingDatabases))
	{
		if (DatabaseList.Body->find(std::string{"[\""}.append(DatabaseName).append("\"]")) != std::string::npos)
		{
			Log.WriteDebug(InfluxDatabaseExists);
			return true;
		}
		auto CreateDatabaseResult(CreateDatabase(Curl, DatabaseName));
		return !LogInfluxError(CreateDatabaseResult, Log, CreatingDatabase);
	}
	return false;
}

void InfluxClient::TransmitNagiosLine(const NagiosPerformanceRecord &NagiosData, std::string &&SourceLine)
{
	auto WriteLineRequest{GetInfluxRequest(CommandWrite)};
	for (auto &line : Translator.TranslateNagiosData(NagiosData))
	{
		WriteLineRequest.ClearQuery();
		WriteLineRequest.AddQueryParameter(InfluxDatabaseParameter, DatabaseName);
		WriteLineRequest.AddQueryParameter("precision", "s");
		WriteLineRequest.SetPostData(std::move(line));
		auto WriteResult{Curl.Post(WriteLineRequest)};
		if (LogInfluxError(WriteResult, Log, Write))
		{
			Log.WriteUploadError(SourceLine);
			break;
		}
	}
}

//** Following code works, but almost certainly uses more memory than it's worth and needs refinement (no great way to catch lines that influx didn't like)
//** Resurrect if bulk inserts become necessary
// constexpr const size_t InitialRecordsSize{1024 * 1024}; // start with a 1MB buffer. the service will learn as it goes to make more appropriate sizes
// constexpr const size_t RecordsSizePadding{512};			  // after the first pass, uses a buffer sized by previous experience. add a bit of padding to reduce odds of reallocation
// size_t InfluxClient::LongestLineSeen{0};
// void InfluxClient::TransmitNagiosBlock(std::queue<NagiosPerformanceRecord> &NagiosData, size_t &LineCount)
// {
// 	std::string RecordsString{};
// 	if (LongestLineSeen == 0)
// 	{
// 		RecordsString.reserve(InitialRecordsSize);
// 	}
// 	else
// 	{
// 		RecordsString.reserve((LongestLineSeen * LineCount) + LineCount + RecordsSizePadding);
// 	}
// 	while (!NagiosData.empty())
// 	{
// 		auto &NagiosRecord{NagiosData.front()};
// 		for (auto &line : Translator.TranslateNagiosData(NagiosRecord))
// 		{
// 			if (line.size() > LongestLineSeen)
// 			{
// 				LongestLineSeen = line.size();
// 			}
// 			RecordsString.append(std::move(line)).append(1, '\n');
// 		}
// 		NagiosData.pop();
// 	}

// 	auto WriteLineRequest{CurlRequest{false, false, true}};
// 	WriteLineRequest.SetPath(CommandWrite);
// 	WriteLineRequest.AddQueryParameter(InfluxDatabaseParameter, DatabaseName);
// 	WriteLineRequest.SetPostData(std::move(RecordsString));
// 	auto PostResult{Curl.Post(WriteLineRequest)};
// 	if (PostResult.CurlResult == CURLE_OK)
// 	{
// 		Log.WriteDebug(InfluxWriteSucceeded);
// 	}
// 	else
// 	{
// 		Log.WriteErrorAnnotated(InfluxWriteFailed, std::to_string(PostResult.CurlResult), curl_easy_strerror(PostResult.CurlResult));
// 	}
// }