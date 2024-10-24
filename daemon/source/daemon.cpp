#include <condition_variable>
#include <curl/curl.h>
#include <chrono>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include "config.hpp"
#include "daemon.hpp"
#include "influxclient.hpp"
#include "filedatacollector.hpp"
#include "signalhandler.hpp"

// service logging constants
constexpr const std::string_view DaemonStarted{"Daemon started"};
constexpr const std::string_view DaemonStopped{"Daemon stopped"};
constexpr const std::string_view SignalHandlerStarted{"Signal handler started"};
constexpr const std::string_view ProcessingConfigReloadRequest{"Processing configuration reload request"};

void N2IDaemon::LoadConfiguration()
{
	Log = Config.Load();
}

void N2IDaemon::Run()
{
	std::atomic<bool> DaemonProcessing{true};

	SystemSignalHandler::BlockAllSignals();

	curl_global_init(CURL_GLOBAL_ALL);

	LoadConfiguration();
	Log->WriteInfoAnnotated(DaemonStarted, std::to_string(getpid()));

	std::condition_variable DaemonAttentionRequiredCondition;
	SystemSignalHandler SignalHandler{};
	SignalHandler.Start(DaemonAttentionRequiredCondition);
	Log->WriteDebug(SignalHandlerStarted);

	std::mutex DaemonMutex;
	do
	{
		if (SignalHandler.ReloadRequested)
		{
			Log->WriteDebug(ProcessingConfigReloadRequest);
			LoadConfiguration();
			SignalHandler.ReloadRequested = false;
		}

		InfluxClient Influx{*Log, Config.InfluxHostName, Config.InfluxPort, Config.InfluxDatabaseName, Config.InfluxMeasurementName, Config.UnitConversionMap};
		if (Influx.TestConnection() && Influx.CreateDatabaseIfNotExists())
		{
			FileDataCollector Collector{Config.NagiosSpoolDirectory, *Log};
			NagiosPerfDataParser Parser{*Log};
			std::string SourceLine;
			while (Collector.More() && !SignalHandler.StopRequested)
			{
				SourceLine = Collector.GetNextLine();
				auto PerfRecord{Parser.ParseNagiosPerformanceRecord(SourceLine)};
				if (PerfRecord.has_value())
				{
					Influx.TransmitNagiosLine(PerfRecord.value(), std::move(SourceLine));
				}
			}
		}
		DaemonProcessing = !SignalHandler.StopRequested;
		if (!SignalHandler.StopRequested)
		{
			std::unique_lock DaemonLock{DaemonMutex};
			DaemonAttentionRequiredCondition.wait_for(DaemonLock, std::chrono::seconds(Config.DataReadDelay), [&SignalHandler]
																	{ return SignalHandler.ReloadRequested || SignalHandler.StopRequested; });
		}
	} while (!SignalHandler.StopRequested);

	curl_global_cleanup();
	Log->WriteInfo(DaemonStopped);
}