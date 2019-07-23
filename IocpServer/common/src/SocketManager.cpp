#include "SocketManager.h"
#include "CommEvent.h"

CSocketManager::CSocketManager()
{
	m_mapClientConnect.clear();
	m_lstFreeClientConn.clear();
	m_lstJobItem.clear();
	m_bShutDown = false;
	m_iClientNums = 0;
	m_i64UniqueIndex = 0;

	InitializeCriticalSection(&m_csConnectLock);
	InitializeCriticalSection(&m_csJobLock);
	InitializeCriticalSection(&m_csTimerLock);
}

CSocketManager::~CSocketManager()
{
	DeleteCriticalSection(&m_csConnectLock);
	DeleteCriticalSection(&m_csJobLock);
	DeleteCriticalSection(&m_csTimerLock);
}

//�յ�һ������
CClientSocket* CSocketManager::ActiveOneConnection(SOCKET hSocket)
{
	if(INVALID_SOCKET == hSocket)
	{
		return nullptr;
	}

	EnterCriticalSection(&m_csConnectLock);
	CClientSocket *pClient = nullptr;
	if(m_lstFreeClientConn.size() > 0)
	{
		pClient = *m_lstFreeClientConn.begin();
		m_lstFreeClientConn.pop_front();
		pClient->InitData();
	}
	else
	{
		pClient = new CClientSocket();
	}

	if(nullptr == pClient)
	{
		LeaveCriticalSection(&m_csConnectLock);
		dzlog_error("new CClientSocket fail");
		return nullptr;
	}
	pClient->SetSocket(hSocket);
	__int64 i64Index = ++m_i64UniqueIndex;
	if(i64Index <= 0)
	{
		m_i64UniqueIndex = 1;
		i64Index = 1;
	}
	pClient->SetIndex(i64Index);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	dzlog_info("ActiveOneConnection m_iClientNums=%d, m_i64UniqueIndex=%lld", m_iClientNums, m_i64UniqueIndex);
	LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

//�ر�һ������
/***************************
�벻Ҫֱ�ӵ��ô˺�����Ҫ�ر�һ�����ӵ��ö�Ӧ�ͻ��˵�CloseSocket�������ɣ�
��ɶ˿ڻ��Զ����ô˺����ͷ�����ռ�õ���Դ
***************************/
bool CSocketManager::CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index)
{
	if(nullptr != pClient)
	{
		dzlog_info("CloseOneConnection begin pClient=%x, i64Index=%lld", pClient, pClient->GetIndex());
	}
	else
	{
		dzlog_info("CloseOneConnection begin i64Index=%lld", i64Index);
	}
	//Ч������
	if (0 == i64Index && nullptr != pClient) 
	{
		i64Index = pClient->GetIndex();
	}
	if ((nullptr == pClient) || (0 == pClient->GetIndex()) || (i64Index != pClient->GetIndex())) 
	{
		return false;
	}

	EnterCriticalSection(&m_csConnectLock);
	ReleaseOneConnection(i64Index);
	dzlog_info("CloseOneConnection end i64Index=%lld, m_iClientNums=%d", i64Index, m_iClientNums);
	LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//�ͷ�������Դ
void CSocketManager::ReleaseOneConnection(unsigned __int64 i64Index, bool bErase)
{
	std::map<unsigned __int64, CClientSocket*>::iterator iterFind = m_mapClientConnect.find(i64Index);
	if(iterFind == m_mapClientConnect.end() || nullptr == iterFind->second)
	{
		dzlog_warn("ReleaseOneConnection i64Index=%lld not exist!", i64Index);
		return;
	}

	//��ౣ��MAX_FREE_CLIENT_NUM��������Դ����
	if(m_lstFreeClientConn.size() < MAX_FREE_CLIENT_NUM)
	{
		iterFind->second->CloseSocket();
		iterFind->second->InitData();
		m_lstFreeClientConn.push_back(iterFind->second);
	}
	else
	{
		delete iterFind->second;//������ʱ����Զ��ر�����
		iterFind->second = nullptr;
	}
	if(bErase)
	{
		m_mapClientConnect.erase(iterFind);
	}
	--m_iClientNums;
	return;
}

//�ر���������
/*******************************
�ڵ��ô˺���ǰ����Ҫ�ȹر�������ɶ˿ڣ�������ܵ��³������
��Ϊ�˺������ͷſͻ����������ڴ棬�����ڴ�ᱻ��ɶ˿�ʹ��
********************************/
bool CSocketManager::CloseAllConnection()
{
	dzlog_info("CSocketManager CloseAllConnection! m_iClientNums=%d", m_iClientNums);
	EnterCriticalSection(&m_csConnectLock);
	//�ͷ��������ӵĿͻ���
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end(); iterClient++)
	{
		//�ͷ���Դ
		if(nullptr != iterClient->second)
		{
			//������ʱ����Զ��ر�����
			delete iterClient->second;
			iterClient->second = nullptr;
		}
	}
	m_mapClientConnect.clear();

	//�ͷſ�����Դ
	std::list<CClientSocket *>::iterator lstClient = m_lstFreeClientConn.begin();
	for(;lstClient != m_lstFreeClientConn.end(); lstClient++)
	{
		//�ͷ���Դ
		if(nullptr != *lstClient)
		{
			delete *lstClient;
			*lstClient = nullptr;
		}
	}
	m_lstFreeClientConn.clear();
	LeaveCriticalSection(&m_csConnectLock);

	//�ͷŴ���������
	EnterCriticalSection(&m_csJobLock);
	std::list<sJobItem *>::iterator lstJob = m_lstJobItem.begin();
	for(;lstJob != m_lstJobItem.end();lstJob++)
	{
		m_jobManager.ReleaseJobItem(*lstJob);
	}
	m_lstJobItem.clear();
	LeaveCriticalSection(&m_csJobLock);
	return true;
}

//����IO��Ϣ
bool CSocketManager::ProcessIOMessage(CClientSocket *pClient, sOverLapped *pOverLapped, DWORD dwIOSize)
{
	if(nullptr == pClient || nullptr == pOverLapped)
	{
		dzlog_error("ProcessIOMessage pClient=%x, pOverLapped=%x", pClient, pOverLapped);
		return false;
	}

	dzlog_debug("ProcessIOMessage m_i64Index=%lld, uOperationType=%d, dwIOSize=%d", 
		pClient->GetIndex(), pOverLapped->uOperationType, dwIOSize);
	switch(pOverLapped->uOperationType)
	{
		//��ʼ��������
	case SOCKET_REV:
		{
			return pClient->OnRecvBegin();
		}
		//�����������
	case SOCKET_REV_FINISH:
		{				
			//����job
			std::list<sJobItem *> lstJobs;
			pClient->OnRecvCompleted(dwIOSize, lstJobs);
			//����job
			if(lstJobs.size() > 0)
			{
				std::list<sJobItem *>::iterator iterJob = lstJobs.begin();
				for(;iterJob != lstJobs.end();iterJob++)
				{
					if(AddJob(*iterJob))
					{
						CCommEvent::GetInstance()->NotifyOneEvent(ENEVT_NEW_JOB_ADD);
					}
				}
			}
			//������������
			return pClient->OnRecvBegin();
		}
		//��ʼ��������
	case SOCKET_SND:
		{
			return pClient->OnSendBegin();
		}
		//�����������
	case SOCKET_SND_FINISH:
		{
			return pClient->OnSendCompleted(dwIOSize);
		}
	default:
		dzlog_warn("unknow uOperationType=%d i64Index=%lld!", pOverLapped->uOperationType, pOverLapped->i64Index);
		break;
	}
	return true;
}

//��������
int CSocketManager::SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	EnterCriticalSection(&m_csConnectLock);
	//���пͻ���
	if(0 == i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end();)
		{
			if(nullptr != iterClient->second)
			{
				iterClient->second->SendData(pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
				++iterClient;
			}
			else
			{
				iterClient = m_mapClientConnect.erase(iterClient);
			}
		}
	}
	//ָ���ͻ���
	else
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(i64Index);
		if(iterClient != m_mapClientConnect.end())
		{
			if(nullptr != iterClient->second)
			{
				iterClient->second->SendData(pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
			}
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
	return 0;
}

//��������
bool CSocketManager::AddJob(sJobItem *pJob)
{
	bool bSucc = false;
	if(nullptr != pJob)
	{
		EnterCriticalSection(&m_csJobLock);
		if(m_lstJobItem.size() < MAX_WAIT_JOB_COUNT)
		{
			m_lstJobItem.push_back(pJob);
			bSucc = true;
		}
		else
		{
			dzlog_warn("AddJob failed, too much job! i64Index=%lld", pJob->i64Index);
			//�ͷ��ڴ�
			m_jobManager.ReleaseJobItem(pJob);
		}
		LeaveCriticalSection(&m_csJobLock);
	}
	return bSucc;
}

//��������
bool CSocketManager::ProcessJob()
{
	sJobItem* pJob = nullptr;
	EnterCriticalSection(&m_csJobLock);
	dzlog_debug("ProcessJob m_lstJobItem.size()=%d", m_lstJobItem.size());
	if(m_lstJobItem.size() > 0)
	{
		pJob = *(m_lstJobItem.begin());
		m_lstJobItem.pop_front();
	}
	LeaveCriticalSection(&m_csJobLock);
	if(nullptr == pJob)
	{
		return false;
	}
	dzlog_debug("ProcessJob i64Index=%lld, dwBufLen=%d", pJob->i64Index, pJob->dwBufLen);
	EnterCriticalSection(&m_csConnectLock);
	//���͸�������
	if(0 == pJob->i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end();)
		{
			if(nullptr != iterClient->second)
			{
				//������ʱ
				if(iterClient->second->GetTimeOutCount() > MAX_CONNECT_TIME_OUT_COUNT)
				{
					ReleaseOneConnection(iterClient->first, false);
					iterClient = m_mapClientConnect.erase(iterClient);
					continue;
				}
				iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
				++iterClient;
			}
			else
			{
				iterClient = m_mapClientConnect.erase(iterClient);
			}
		}
	}
	else
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(pJob->i64Index);
		if(iterClient != m_mapClientConnect.end() && nullptr != iterClient->second)
		{
			iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
		}
		else
		{
			dzlog_warn("ProcessJob i64Index=%lld not exists!", pJob->i64Index);
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
	//�ͷ�Job�ڴ�
	m_jobManager.ReleaseJobItem(pJob);
	return true;
}

//��ʱ��
void CSocketManager::OnBaseTimer()
{
	EnterCriticalSection(&m_csTimerLock);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.begin();
	for(;itTimer != m_mapAllTimer.end();)
	{
		DWORD dwNowTime = GetTickCount();
		if(dwNowTime > itTimer->second.dwEnterTime)
		{
			OnTimer(itTimer->first);
			//ѭ������
			if((-1 == itTimer->second.iTimerCount) || (--itTimer->second.iTimerCount > 0))
			{
				itTimer->second.dwEnterTime += itTimer->second.dwDelayTime;
				++itTimer;
			}
			else
			{
				itTimer = m_mapAllTimer.erase(itTimer);
			}
		}
		else
		{
			++itTimer;
		}
	}
	LeaveCriticalSection(&m_csTimerLock);
	return;
}

//���ö�ʱ��
int CSocketManager::SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount)
{
	dzlog_info("SetTimer dwTimerID=%d, dwDelayTime=%d, iTimerCount=%d", dwTimerID, dwDelayTime, iTimerCount);
	EnterCriticalSection(&m_csTimerLock);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.find(dwTimerID);
	if(itTimer != m_mapAllTimer.end())
	{
		dzlog_warn("SetTimer dwTimerID=%d exist, update it!", dwTimerID);
		itTimer->second.dwBeginTime = GetTickCount();
		itTimer->second.dwDelayTime = dwDelayTime;
		itTimer->second.dwEnterTime = itTimer->second.dwBeginTime + itTimer->second.dwDelayTime;
		itTimer->second.iTimerCount = iTimerCount;
	}
	else
	{
		sTimer curTimer;
		curTimer.dwTimerID = dwTimerID;
		curTimer.iTimerCount = iTimerCount;
		curTimer.dwBeginTime = GetTickCount();
		curTimer.dwDelayTime = dwDelayTime;	
		curTimer.dwEnterTime = curTimer.dwBeginTime + curTimer.dwDelayTime;
		m_mapAllTimer[dwTimerID] = curTimer;
	}
	LeaveCriticalSection(&m_csTimerLock);
	return 0;
}

//������ʱ��
int CSocketManager::OnTimer(DWORD dwTimerID)
{
	dzlog_info("OnTimer begin dwTimerID=%d", dwTimerID);
	switch(dwTimerID)
	{
	case TIMER_ID_KEEP_ALIVE:
		{
			AddHeartJob();
			break;
		}
	default:
		break;
	}
	dzlog_info("OnTimer finish dwTimerID=%d", dwTimerID);
	return 0;
}

//ɾ����ʱ��
int CSocketManager::KillTimer(DWORD dwTimerID)
{
	dzlog_info("KillTimer dwTimerID=%d", dwTimerID);
	EnterCriticalSection(&m_csTimerLock);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.find(dwTimerID);
	if(itTimer == m_mapAllTimer.end())
	{
		dzlog_warn("KillTimer dwTimerID=%d not exist!", dwTimerID);
	}
	else
	{
		m_mapAllTimer.erase(itTimer);
	}
	LeaveCriticalSection(&m_csTimerLock);
	return 0;
}

//��������Job
void CSocketManager::AddHeartJob()
{
	//������������
	NetMsgHead msgHead;
	msgHead.dwMsgSize = sizeof(NetMsgHead);
	msgHead.dwMainID = MAIN_KEEP_ALIVE;
	msgHead.dwAssID = ASS_KEEP_ALIVE;
	msgHead.dwHandleCode = 0;
	msgHead.dwReserve = 0;
	sJobItem* pJob = m_jobManager.NewJobItem(msgHead.dwMsgSize);
	if(nullptr != pJob)
	{
		pJob->i64Index = 0;
		pJob->dwBufLen = msgHead.dwMsgSize;
		memcpy(pJob->pJobBuff, &msgHead, pJob->dwBufLen);
		if(AddJob(pJob))
		{
			CCommEvent::GetInstance()->NotifyOneEvent(ENEVT_NEW_JOB_ADD);
		}
	}
}