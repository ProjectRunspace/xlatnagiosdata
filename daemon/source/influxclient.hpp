#pragma once

#include <curl/curl.h>
#include <map>
#include <queue>
#include <string_view>
#include "curlclient.hpp"
#include "influxtranslator.hpp"
#include "logwriter.hpp"
#include "nagiosparser.hpp"

class InfluxClient
{
private:
	ILogWriter &Log;
	const std::string HostName;
	const std::string DatabaseName;
	CurlClient Curl;
	InfluxTranslator Translator;

public:
	InfluxClient(ILogWriter &Log, std::string HostName, long Port, std::string DatabaseName, std::string MeasurementName, std::map<const std::string, const std::string> &UnitConversionMap);
	~InfluxClient() = default;
	InfluxClient(const InfluxClient &) = delete;
	InfluxClient &operator=(const InfluxClient &) = delete;
	InfluxClient(InfluxClient &&) = delete;
	InfluxClient &operator=(InfluxClient &&) = delete;

	bool TestConnection();
	bool CreateDatabaseIfNotExists();
	void TransmitNagiosLine(const NagiosPerformanceRecord &NagiosData, std::string &&SourceLine);
};
