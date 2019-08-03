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
	//��ȡ����
	void LoadCfg();

	//��ȡloggerʵ��
	std::shared_ptr<spdlog::logger> GetLogger();
private:
	static CSpdlogImpl* m_pInstance;//����
	std::shared_ptr<spdlog::logger> m_logger;//loggerָ��
	std::string m_strFileName;//��־�ļ���
	std::string m_strLoggerName;//logger����
	unsigned short m_usMaxFileNum;//��ౣ���������ļ�
	unsigned int m_uFileSize;//ÿ���ļ���С
	spdlog::level::level_enum m_logLevel;//��ǰ��־�ȼ�
	std::string m_strLogPattern;//��־��ʽ
};

//��ȡlogger
static std::shared_ptr<spdlog::logger> loggerIns()
{
	return CSpdlogImpl::GetInstance()->GetLogger();
}

#endif



