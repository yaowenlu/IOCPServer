#include "CommEvent.h"
#include "SpdlogDef.h"

CCommEvent* CCommEvent::m_pInstance = nullptr;

CCommEvent::CCommEvent()
{
	InitializeCriticalSection(&m_csEventLock);
}

CCommEvent::~CCommEvent()
{
	DeleteCriticalSection(&m_csEventLock);
}

CCommEvent* CCommEvent::GetInstance()
{
	if(nullptr == m_pInstance)
	{
		m_pInstance = new CCommEvent();
	}
	return m_pInstance;
}

void CCommEvent::ReleaseInstance()
{
	if(nullptr != m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = nullptr;
	}
	return;
}

//添加一个事件
void CCommEvent::AddOneEvent(std::string strEventName, void *pEventFunc, void *pFuncParam)
{
	EnterCriticalSection(&m_csEventLock);
	map<string, sEventInfo>::iterator itEvent = m_mapEvent.find(strEventName);
	if(itEvent != m_mapEvent.end())
	{
		loggerIns()->warn("{} strEventName={} already exist!", __FUNCTION__, strEventName);
	}
	else
	{
		sEventInfo curEvent;
		curEvent.pEventFunc = pEventFunc;
		curEvent.pFuncParam = pFuncParam;
		m_mapEvent[strEventName] = curEvent;
		loggerIns()->info("{} strEventName={} added!", __FUNCTION__, strEventName);
	}
	LeaveCriticalSection(&m_csEventLock);
}

//移除一个事件
void CCommEvent::RemoveOneEvent(std::string strEventName)
{
	EnterCriticalSection(&m_csEventLock);
	map<string, sEventInfo>::iterator itEvent = m_mapEvent.find(strEventName);
	if(itEvent != m_mapEvent.end())
	{
		m_mapEvent.erase(itEvent);
		loggerIns()->info("{} strEventName={} removed!", __FUNCTION__, strEventName);
	}
	else
	{
		loggerIns()->warn("{} strEventName={} not exist!", __FUNCTION__, strEventName);
	}
	LeaveCriticalSection(&m_csEventLock);
}

//响应一个事件
void CCommEvent::NotifyOneEvent(std::string strEventName)
{
	EnterCriticalSection(&m_csEventLock);
	map<string, sEventInfo>::iterator itEvent = m_mapEvent.find(strEventName);
	if(itEvent == m_mapEvent.end())
	{
		loggerIns()->warn("{} strEventName={} not exist!", __FUNCTION__, strEventName);
	}
	else
	{
		if(nullptr != itEvent->second.pEventFunc)
		{
			m_callBack = reinterpret_cast<func_callback>(itEvent->second.pEventFunc);
			if (m_callBack)
			{
				(*m_callBack)(strEventName, itEvent->second.pFuncParam);
			}
			else
			{
				loggerIns()->warn("{} strEventName={}, m_callBack is null!", __FUNCTION__, strEventName);
			}
		}
	}
	LeaveCriticalSection(&m_csEventLock);
}
