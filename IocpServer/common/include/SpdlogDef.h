#ifndef SPDLOG_DEF_H
#define SPDLOG_DEF_H
#include <string>

#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG
#define SPDLOG_ENABLE_MESSAGE_COUNTER

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/null_sink.h"
#include "spdlog/sinks/ostream_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/logger.h"

//logger名
const std::string logger_name = "spd_logger";
//日志文件名
const std::string log_file_name = "spd_log";
//每个文件最大100M
const int log_file_size = 100 * 1024 * 1024;
//最多保留5个文件
const int log_file_num = 5;

static std::shared_ptr<spdlog::logger> g_logger;
static std::shared_ptr<spdlog::logger> loggerIns()
{
	if (nullptr == g_logger)
	{
		g_logger = spdlog::get(logger_name);
		if (nullptr == g_logger)
		{
			g_logger = spdlog::rotating_logger_mt(logger_name, log_file_name, log_file_size, log_file_num);
			g_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l]:%v");
			g_logger->set_level(spdlog::level::debug);
		}
	}

	return g_logger;
}

#endif



