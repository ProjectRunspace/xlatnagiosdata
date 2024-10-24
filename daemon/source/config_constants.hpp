#pragma once

#define __XLATPERF_PACKAGE_NAME__ "xlatnagiosdata"

#include <string>
#include <string_view>

namespace ConfigConstants
{
	constexpr const std::string_view packagename{__XLATPERF_PACKAGE_NAME__};
	constexpr const std::string_view appname{__XLATPERF_PACKAGE_NAME__ "d\0"}; // used in C APIs, do not assume NUL-termination
	constexpr const std::string_view LogRootPath{"/var/log/" __XLATPERF_PACKAGE_NAME__};
	constexpr const std::string_view DaemonLogFileName{"daemon.log"};
	constexpr const std::string_view DaemonLockFileName{"daemon.lock"};
	constexpr const std::string_view FailedWritesFileName{"failed_writes.log"};

	namespace Headers
	{
		constexpr const std::string_view daemon{"daemon"};
		constexpr const std::string_view logging{"logging"};
		constexpr const std::string_view influx{"influx"};
		constexpr const std::string_view nagios{"nagios"};
		constexpr const std::string_view unitConversionMap{"unit_conversion_map"};
	};

	namespace Fields
	{
		constexpr const std::string_view delay{"delay"};
		constexpr const std::string_view enabled{"enabled"};
		constexpr const std::string_view level{"level"};
		constexpr const std::string_view save_failed_writes{"save_failed_writes"};
		constexpr const std::string_view failed_writes_failback{"failed_writes_fallback"};
		constexpr const std::string_view host{"host"};
		constexpr const std::string_view port{"port"};
		constexpr const std::string_view database{"database"};
		constexpr const std::string_view measurement{"measurement"};
		constexpr const std::string_view protocol{"protocol"};
		constexpr const std::string_view spoolDirectory{"spool_directory"};
	};

	namespace Values
	{
		constexpr const std::string_view debug{"debug"};
		constexpr const std::string_view info{"info"};
		constexpr const std::string_view warn{"warn"};
		constexpr const std::string_view error{"error"};
		constexpr const std::string_view fatal{"fatal"};
		constexpr const std::string_view protocol{"http"};
	};

	namespace DefaultValues
	{
		constexpr const int dataReadDelay{30};
		constexpr const std::string_view logLevel{Values::info};
		constexpr const std::string_view influxHostName{"localhost"};
		constexpr const long influxPort{8086};
		constexpr const std::string_view influxDatabaseName{"nagiosrecords"};
		constexpr const std::string_view influxMetricName{"perfdata"};
		constexpr const std::string_view nagiosSpoolDirectory{"/usr/local/nagios/var/spool/" __XLATPERF_PACKAGE_NAME__};
	};
}
