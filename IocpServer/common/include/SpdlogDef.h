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

class CSpdlogImpl
{
public:
	static CSpdlogImpl* GetInstance();
	static void ReleaseInstance();
private:
	CSpdlogImpl();
	~CSpdlogImpl();
public:
	//读取配置
	void LoadCfg();

	//获取logger实例
	std::shared_ptr<spdlog::logger> GetLogger();
private:
	static CSpdlogImpl* m_pInstance;//单例
	std::shared_ptr<spdlog::logger> m_logger;//logger指针
	std::string m_strFileName;//日志文件名
	std::string m_strLoggerName;//logger名称
	unsigned short m_usMaxFileNum;//最多保留几个个文件
	unsigned int m_uFileSize;//每个文件大小
	spdlog::level::level_enum m_logLevel;//当前日志等级
	std::string m_strLogPattern;//日志格式
};

//获取logger
static std::shared_ptr<spdlog::logger> loggerIns()
{
	return CSpdlogImpl::GetInstance()->GetLogger();
}

#endif



