#include "SocketManager.h"
#include "CommEvent.h"
#include "SpdlogDef.h"
#include "ProxyDefine.h"

CSocketManager::CSocketManager()
{
	m_mapClientConnect.clear();
	m_lstFreeClientConn.clear();
	m_lstJobItem.clear();
	m_bShutDown = false;
	m_iClientNums = 0;
	m_i64UniqueIndex = 0;

	InitializeCriticalSection(&m_csActiveConnectLock);
	InitializeCriticalSection(&m_csFreeConnectLock);
	InitializeCriticalSection(&m_csJobLock);
	InitializeCriticalSection(&m_csTimerLock);
}

CSocketManager::~CSocketManager()
{
	DeleteCriticalSection(&m_csActiveConnectLock);
	DeleteCriticalSection(&m_csFreeConnectLock);
	DeleteCriticalSection(&m_csJobLock);
	DeleteCriticalSection(&m_csTimerLock);
}

//收到一个连接
CClientSocket* CSocketManager::ActiveOneConnection(SOCKET hSocket)
{
	if(INVALID_SOCKET == hSocket)
	{
		return nullptr;
	}

	
	CClientSocket *pClient = nullptr;
	EnterCriticalSection(&m_csFreeConnectLock);
	if(m_lstFreeClientConn.size() > 0)
	{
		pClient = *m_lstFreeClientConn.begin();
		m_lstFreeClientConn.pop_front();
		LeaveCriticalSection(&m_csFreeConnectLock);
		pClient->InitData();
	}
	else
	{
		LeaveCriticalSection(&m_csFreeConnectLock);
		pClient = new CClientSocket();
	}
	
	if(nullptr == pClient)
	{
		loggerIns()->error("new CClientSocket fail");
		return nullptr;
	}
	pClient->SetSocket(hSocket);
	EnterCriticalSection(&m_csActiveConnectLock);
	__int64 i64Index = ++m_i64UniqueIndex;
	if(i64Index <= 0)
	{
		m_i64UniqueIndex = 1;
		i64Index = 1;
	}
	pClient->SetIndex(i64Index);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	loggerIns()->info("ActiveOneConnection m_iClientNums={}, m_i64UniqueIndex={}", m_iClientNums, m_i64UniqueIndex);
	LeaveCriticalSection(&m_csActiveConnectLock);
	return pClient;
}

//关闭一个连接
bool CSocketManager::CloseOneConnection(CClientSocket *pClient)
{
	if(nullptr == pClient)
	{
		loggerIns()->warn("CloseOneConnection pClient is null!");
		return false;
	}
	loggerIns()->info("CloseOneConnection pClient={}, i64Index={}, i64SrvIndex={}", (void *)(pClient), pClient->GetIndex(), pClient->GetSrvIndex());

	pClient->CloseSocket();
	return true;
}

//释放连接资源
void CSocketManager::ReleaseOneConnection(CClientSocket *pClient)
{
	if (nullptr == pClient)
	{
		loggerIns()->warn("ReleaseOneConnection pClient is null!");
		return;
	}

	//先把资源拿出来
	EnterCriticalSection(&m_csActiveConnectLock);
	if (m_mapClientConnect.find(pClient->GetIndex()) != m_mapClientConnect.end())
	{
		m_mapClientConnect.erase(pClient->GetIndex());
		--m_iClientNums;
	}
	LeaveCriticalSection(&m_csActiveConnectLock);

	//最多保留MAX_FREE_CLIENT_NUM个空闲资源备用
	EnterCriticalSection(&m_csFreeConnectLock);
	if(m_lstFreeClientConn.size() < MAX_FREE_CLIENT_NUM)
	{
		loggerIns()->info("reuse i64Index={}, pClient={}", pClient->GetIndex(), (void*)(pClient));
		pClient->CloseSocket();
		pClient->InitData();
		m_lstFreeClientConn.push_back(pClient);
		LeaveCriticalSection(&m_csFreeConnectLock);
	}
	else
	{
		LeaveCriticalSection(&m_csFreeConnectLock);
		loggerIns()->info("delete i64Index={}, pClient={}", pClient->GetIndex(), (void*)(pClient));
		//析构的时候会自动关闭连接
		delete pClient;
		pClient = nullptr;
	}
	return;
}

//关闭所有连接
/*******************************
在调用此函数前必须要先关闭所有完成端口，否则可能导致程序崩溃
因为此函数会释放客户端所属的内存，而此内存会被完成端口使用
********************************/
bool CSocketManager::CloseAllConnection()
{
	loggerIns()->info("CSocketManager CloseAllConnection! m_iClientNums={}", m_iClientNums);
	EnterCriticalSection(&m_csActiveConnectLock);
	//释放所有连接的客户端
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end(); iterClient++)
	{
		//释放资源
		if(nullptr != iterClient->second)
		{
			//析构的时候会自动关闭连接
			delete iterClient->second;
			iterClient->second = nullptr;
		}
	}
	m_mapClientConnect.clear();
	LeaveCriticalSection(&m_csActiveConnectLock);

	EnterCriticalSection(&m_csFreeConnectLock);
	//释放空闲资源
	std::list<CClientSocket *>::iterator lstClient = m_lstFreeClientConn.begin();
	for(;lstClient != m_lstFreeClientConn.end(); lstClient++)
	{
		//释放资源
		if(nullptr != *lstClient)
		{
			delete *lstClient;
			*lstClient = nullptr;
		}
	}
	m_lstFreeClientConn.clear();
	LeaveCriticalSection(&m_csFreeConnectLock);

	//释放待处理任务
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

//处理IO消息
bool CSocketManager::ProcessIOMessage(CClientSocket *pClient, sOverLapped *pOverLapped, DWORD dwIOSize)
{
	if(nullptr == pClient || nullptr == pOverLapped)
	{
		loggerIns()->error("ProcessIOMessage pClient={}, pOverLapped={}", (void *)(pClient), (void *)(pOverLapped));
		return false;
	}

	loggerIns()->debug("ProcessIOMessage m_i64Index={}, i64SrvIndex={}, uOperationType={}, dwIOSize={}", 
		pClient->GetIndex(), pClient->GetSrvIndex(), pOverLapped->uOperationType, dwIOSize);
	switch(pOverLapped->uOperationType)
	{
		//开始接收数据
	case SOCKET_REV:
		{
			return pClient->OnRecvBegin();
		}
		//接收数据完成
	case SOCKET_REV_FINISH:
		{				
			//有新job
			std::list<sJobItem *> lstJobs;
			pClient->OnRecvCompleted(dwIOSize, lstJobs);
			//有新job
			if(lstJobs.size() > 0)
			{
				std::list<sJobItem *>::iterator iterJob = lstJobs.begin();
				for(;iterJob != lstJobs.end();iterJob++)
				{
					if (AddOneJob(*iterJob))
					{
						CCommEvent::GetInstance()->NotifyOneEvent(EVENT_NEW_JOB_ADD);
					}
				}
			}
			//继续接收数据
			return pClient->OnRecvBegin();
		}
		//开始发送数据
	case SOCKET_SND:
		{
			return pClient->OnSendBegin();
		}
		//发送数据完成
	case SOCKET_SND_FINISH:
		{
			return pClient->OnSendCompleted(dwIOSize);
		}
	default:
		loggerIns()->warn("unknow uOperationType={} i64Index={}, i64SrvIndex={}!", pOverLapped->uOperationType, pOverLapped->i64Index, pClient->GetSrvIndex());
		break;
	}
	return true;
}

//发送数据
int CSocketManager::SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	EnterCriticalSection(&m_csActiveConnectLock);
	//所有客户端
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
				--m_iClientNums;
			}
		}
	}
	//指定客户端
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
	LeaveCriticalSection(&m_csActiveConnectLock);
	return 0;
}

//增加任务
bool CSocketManager::AddOneJob(sJobItem *pJob)
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
			loggerIns()->warn("AddOneJob failed, too much job! i64Index={}", pJob->i64Index);
			//释放内存
			m_jobManager.ReleaseJobItem(pJob);
		}
		LeaveCriticalSection(&m_csJobLock);
	}
	return bSucc;
}

//处理任务
bool CSocketManager::ProcessJob()
{
	sJobItem* pJob = nullptr;
	EnterCriticalSection(&m_csJobLock);
	loggerIns()->debug("ProcessJob m_lstJobItem.size()={}", m_lstJobItem.size());
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
	loggerIns()->debug("ProcessJob i64Index={}, dwBufLen={}", pJob->i64Index, pJob->dwBufLen);
	EnterCriticalSection(&m_csActiveConnectLock);
	//发送给所有人
	if(0 == pJob->i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end();)
		{
			if(nullptr != iterClient->second)
			{
				iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
				++iterClient;
			}
			else
			{
				iterClient = m_mapClientConnect.erase(iterClient);
				--m_iClientNums;
			}
		}
	}
	else
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(pJob->i64Index);
		if(iterClient != m_mapClientConnect.end() && nullptr != iterClient->second)
		{
			enHeadType* pHeadType = reinterpret_cast<enHeadType*>(pJob->pJobBuff + sizeof(DWORD));
			//代理消息，直接转发
			if (pHeadType && PROXY_HEAD == *pHeadType)
			{
				sProxyHead* pProxyHead = reinterpret_cast<sProxyHead*>(pJob->pJobBuff);
				if (pProxyHead && pProxyHead->iDstType != INVALID_SRV)
				{
					std::map<unsigned __int64, CClientSocket*>::iterator iterClt = m_mapClientConnect.begin();
					//找到对应的服务
					for (;iterClt != m_mapClientConnect.end();iterClt++)
					{
						//点对点
						if (pProxyHead->iTransType == trans_p2p)
						{
							if (iterClt->second->GetSrvType() == pProxyHead->iDstType && iterClt->second->GetSrvID() == pProxyHead->uDstID)
							{
								//一般都是请求后返回的结果
								if (pProxyHead->i64Index != 0)
								{
									//指定ID
									if (pProxyHead->i64Index == iterClt->first)
									{
										iterClient->second->SendProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
									}
								}
								//一般都是发起的请求
								else
								{
									//记录请求者，在应答时返回结果给请求者
									pProxyHead->i64Index = iterClt->first;
									iterClient->second->SendProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
								}
							}
							break;
						}
						//点到组
						else if (pProxyHead->iTransType == trans_p2g)
						{
							if (iterClt->second->GetSrvType() == pProxyHead->iDstType)
							{
								pProxyHead->i64Index = iterClt->first;
								iterClient->second->SendProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
							}
						}
						else
						{
							loggerIns()->error("iTransType={} is illegal!");
							break;
						}
					}
				}
				else
				{
					loggerIns()->warn("ProcessJob pProxyHead invalid!");
				}
			}
			//正常消息直接处理
			else if (pHeadType && MSG_HEAD == *pHeadType)
			{
				iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
			}
			else
			{
				loggerIns()->warn("ProcessJob pHeadType invalid!");
			}
		}
		else
		{
			loggerIns()->warn("ProcessJob i64Index={} not exists!", pJob->i64Index);
		}
	}
	LeaveCriticalSection(&m_csActiveConnectLock);
	//释放Job内存
	m_jobManager.ReleaseJobItem(pJob);
	return true;
}

//定时器
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
			//循环命中
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

//设置定时器
int CSocketManager::SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount)
{
	loggerIns()->info("SetTimer dwTimerID={}, dwDelayTime={}, iTimerCount={}", dwTimerID, dwDelayTime, iTimerCount);
	EnterCriticalSection(&m_csTimerLock);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.find(dwTimerID);
	if(itTimer != m_mapAllTimer.end())
	{
		loggerIns()->warn("SetTimer dwTimerID={} exist, update it!", dwTimerID);
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

//触发定时器
int CSocketManager::OnTimer(DWORD dwTimerID)
{
	loggerIns()->info("OnTimer begin dwTimerID={}", dwTimerID);
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
	loggerIns()->info("OnTimer finish dwTimerID={}", dwTimerID);
	return 0;
}

//删除定时器
int CSocketManager::KillTimer(DWORD dwTimerID)
{
	loggerIns()->info("KillTimer dwTimerID={}", dwTimerID);
	EnterCriticalSection(&m_csTimerLock);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.find(dwTimerID);
	if(itTimer == m_mapAllTimer.end())
	{
		loggerIns()->warn("KillTimer dwTimerID={} not exist!", dwTimerID);
	}
	else
	{
		m_mapAllTimer.erase(itTimer);
	}
	LeaveCriticalSection(&m_csTimerLock);
	return 0;
}

//增加心跳Job
void CSocketManager::AddHeartJob()
{
	//发送心跳数据
	NetMsgHead msgHead;
	msgHead.dwMsgSize = sizeof(NetMsgHead);
	msgHead.iHeadType = MSG_HEAD;
	msgHead.dwMainID = MAIN_KEEP_ALIVE;
	msgHead.dwAssID = ASS_SC_KEEP_ALIVE;
	msgHead.dwHandleCode = 0;
	msgHead.dwReserve = 0;
	sJobItem* pJob = m_jobManager.NewJobItem(msgHead.dwMsgSize);
	if(nullptr != pJob)
	{
		pJob->i64Index = 0;
		pJob->dwBufLen = msgHead.dwMsgSize;
		memcpy(pJob->pJobBuff, &msgHead, pJob->dwBufLen);
		if (AddOneJob(pJob))
		{
			CCommEvent::GetInstance()->NotifyOneEvent(EVENT_NEW_JOB_ADD);
		}
	}
}