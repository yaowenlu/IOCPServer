#include "SpdlogDef.h"

CSpdlogImpl* CSpdlogImpl::m_pInstance = nullptr;

CSpdlogImpl* CSpdlogImpl::GetInstance()
{
	if (nullptr == m_pInstance)
	{
		m_pInstance = new CSpdlogImpl();
	}
	return m_pInstance;
}

void CSpdlogImpl::ReleaseInstance()
{
	if (nullptr != m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = nullptr;
	}
}

CSpdlogImpl::CSpdlogImpl()
{
	m_logger = nullptr;
	m_strFileName = "spd_log";
	m_strLoggerName = "logger_1";
	m_usMaxFileNum = 5;
	m_uFileSize = 100 * 1024 * 1024;
	LoadCfg();
}

CSpdlogImpl::~CSpdlogImpl()
{

}

//读取配置文件
void CSpdlogImpl::LoadCfg()
{
	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(sizeof(szPath), szPath);
	std::string strCfgFileName = szPath;
	strCfgFileName += "/conf/spdlogCfg.ini";

	std::string strKey = "config";
	char szValue[128] = { 0 };
	GetPrivateProfileStringA(strKey.c_str(), "FileName", "spd_log", szValue, sizeof(szValue), strCfgFileName.c_str());
	m_strFileName = szValue;

	memset(szValue, 0, sizeof(szValue));
	GetPrivateProfileStringA(strKey.c_str(), "LoggerName", "logger_1", szValue, sizeof(szValue), strCfgFileName.c_str());
	m_strLoggerName = szValue;
	//默认文件数5
	m_usMaxFileNum = GetPrivateProfileIntA(strKey.c_str(), "MaxFileNum", 5, strCfgFileName.c_str());
	//默认100M
	m_uFileSize = GetPrivateProfileIntA(strKey.c_str(), "FileSize", 100, strCfgFileName.c_str());
	m_uFileSize *= 1024 * 1024;
	//默认打印debug级别的日志
	m_logLevel = static_cast<spdlog::level::level_enum>(GetPrivateProfileIntA(strKey.c_str(), "LogLevel", 1, strCfgFileName.c_str()));
	//日志格式
	memset(szValue, 0, sizeof(szValue));
	GetPrivateProfileStringA(strKey.c_str(), "LogPattern", "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l]:%v", szValue, sizeof(szValue), strCfgFileName.c_str());
	m_strLogPattern = szValue;
}

//获取logger实例
std::shared_ptr<spdlog::logger> CSpdlogImpl::GetLogger()
{
	if (nullptr == m_logger)
	{
		m_logger = spdlog::get(m_strLoggerName);
		if (nullptr == m_logger)
		{
			m_logger = spdlog::rotating_logger_mt(m_strLoggerName, m_strFileName, m_uFileSize, m_usMaxFileNum);
			m_logger->set_pattern(m_strLogPattern);
			m_logger->set_level(m_logLevel);
		}
	}

	return m_logger;
}

