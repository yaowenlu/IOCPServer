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
		loggerIns()->warn("AddOneEvent strEventName=%s already exist!", strEventName.c_str());
	}
	else
	{
		sEventInfo curEvent;
		curEvent.pEventFunc = pEventFunc;
		curEvent.pFuncParam = pFuncParam;
		m_mapEvent[strEventName] = curEvent;
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
		loggerIns()->warn("NotifyOneEvent strEventName=%s not exist!", strEventName.c_str());
	}
	else
	{
		if(nullptr != itEvent->second.pEventFunc)
		{
			m_callBack = reinterpret_cast<func_callback>(itEvent->second.pEventFunc);
			(*m_callBack)(strEventName, itEvent->second.pFuncParam);
		}
	}
	LeaveCriticalSection(&m_csEventLock);
}
