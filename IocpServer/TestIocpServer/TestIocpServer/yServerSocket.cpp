#include "stdafx.h"
#include "yServerSocket.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"zlog.lib")

CClientManager::CClientManager()
{
	m_mapClientConnect.clear();
	m_lstFreeClientConn.clear();
	m_lstJobItem.clear();
	m_bShutDown = false;
	m_iClientNums = 0;
	m_i64UniqueIndex = 0;

	::InitializeCriticalSection(&m_csConnectLock);
	::InitializeCriticalSection(&m_csJob);
	//::InitializeCriticalSection(&m_csEvent);
	::InitializeCriticalSection(&m_csTimer);
}

CClientManager::~CClientManager()
{
	::DeleteCriticalSection(&m_csConnectLock);
	::DeleteCriticalSection(&m_csJob);
	//::DeleteCriticalSection(&m_csEvent);
	::DeleteCriticalSection(&m_csTimer);
}

//收到一个连接
CClientSocket* CClientManager::ActiveOneConnection(SOCKET hSocket)
{
	if(INVALID_SOCKET == hSocket)
	{
		return nullptr;
	}

	::EnterCriticalSection(&m_csConnectLock);
	CClientSocket *pClient = nullptr;
	if(m_lstFreeClientConn.size() > 0)
	{
		pClient = *m_lstFreeClientConn.begin();
		m_lstFreeClientConn.pop_front();
		pClient->InitData();
	}
	else
	{
		pClient = new CClientSocket;
	}

	if(nullptr == pClient)
	{
		::LeaveCriticalSection(&m_csConnectLock);
		dzlog_error("new CClientSocket fail");
		return nullptr;
	}
	pClient->SetSocket(hSocket);
	__int64 i64Index = ++m_i64UniqueIndex;
	pClient->SetIndex(i64Index);
	pClient->SetClientManager(this);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	dzlog_info("ActiveOneConnection m_iClientNums=%d, m_i64UniqueIndex=%lld", m_iClientNums, m_i64UniqueIndex);
	::LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

//关闭一个连接
/***************************
请不要直接调用此函数，要关闭一个连接调用对应客户端的CloseSocket函数即可，
完成端口会自动调用此函数释放连接占用的资源
***************************/
bool CClientManager::CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index)
{
	if(nullptr != pClient)
	{
		dzlog_info("CloseOneConnection begin i64Index=%lld", pClient->GetIndex());
	}
	else
	{
		dzlog_info("CloseOneConnection begin i64Index=%lld", i64Index);
	}
	//效验数据
	if (0 == i64Index && nullptr != pClient) 
	{
		i64Index = pClient->GetIndex();
	}
	if ((nullptr == pClient) || (0 == pClient->GetIndex()) || (i64Index != pClient->GetIndex())) 
	{
		return false;
	}

	::EnterCriticalSection(&m_csConnectLock);
	ReleaseOneConnection(i64Index);
	dzlog_info("CloseOneConnection end i64Index=%lld, m_iClientNums=%d", i64Index, m_iClientNums);
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//释放连接资源
void CClientManager::ReleaseOneConnection(unsigned __int64 i64Index, bool bErase)
{
	std::map<unsigned __int64, CClientSocket*>::iterator iterFind = m_mapClientConnect.find(i64Index);
	if(iterFind == m_mapClientConnect.end())
	{
		return;
	}

	//最多保留MAX_FREE_CLIENT_NUM个空闲资源备用
	if(m_lstFreeClientConn.size() < MAX_FREE_CLIENT_NUM)
	{
		iterFind->second->CloseSocket();
		m_lstFreeClientConn.push_back(iterFind->second);
	}
	else
	{
		delete iterFind->second;//析构的时候会自动关闭连接
		iterFind->second = nullptr;
	}
	if(bErase)
	{
		m_mapClientConnect.erase(iterFind);
	}
	--m_iClientNums;
	return;
}

//关闭所有连接
/*******************************
在调用此函数前必须要先关闭所有完成端口，否则可能导致程序崩溃
因为此函数会释放客户端所属的内存，而此内存会被完成端口使用
********************************/
bool CClientManager::CloseAllConnection()
{
	dzlog_info("CClientManager CloseAllConnection! m_iClientNums=%d", m_iClientNums);
	::EnterCriticalSection(&m_csConnectLock);
	//释放所有连接的客户端
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end(); iterClient++)
	{
		//释放资源
		if(nullptr != iterClient->second)
		{
			delete iterClient->second;//析构的时候会自动关闭连接
			iterClient->second = nullptr;
		}
	}
	m_mapClientConnect.clear();

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

	//释放任务
	std::list<sJobItem *>::iterator lstJob = m_lstJobItem.begin();
	for(;lstJob != m_lstJobItem.end(); lstJob++)
	{
		//释放资源
		if(nullptr != *lstJob)
		{
			delete[] (*lstJob)->pJobBuff;
			(*lstJob)->pJobBuff = nullptr;
			delete *lstJob;
			*lstJob = nullptr;
		}
	}
	m_lstJobItem.clear();
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//发送数据
int CClientManager::SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	EnterCriticalSection(&m_csConnectLock);
	//所有客户端
	if(0 == i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end();)
		{
			if(nullptr != iterClient->second)
			{
				//客户端长时间没响应
				if(iterClient->second->GetTimeOutCount() > MAX_CONNECT_TIME_OUT_COUNT)
				{
					ReleaseOneConnection(iterClient->first, false);
					iterClient = m_mapClientConnect.erase(iterClient);
				}
				else
				{
					iterClient->second->SendData(pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
					++iterClient;
				}
			}
			else
			{
				iterClient = m_mapClientConnect.erase(iterClient);
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
				//客户端长时间没响应
				if(iterClient->second->GetTimeOutCount() > MAX_CONNECT_TIME_OUT_COUNT)
				{
					ReleaseOneConnection(iterClient->first);
				}
				else
				{
					iterClient->second->SendData(pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
				}
			}
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
	return 0;
}

//增加任务
bool CClientManager::AddJob(sJobItem *pJob)
{
	bool bSucc = false;
	if(nullptr != pJob)
	{
		EnterCriticalSection(&m_csJob);
		if(m_lstJobItem.size() < MAX_WAIT_JOB_COUNT)
		{
			m_lstJobItem.push_back(pJob);
			bSucc = true;
		}
		else
		{
			dzlog_warn("AddJob failed, too much job! i64Index=%lld", pJob->i64Index);
			//释放内存
			if(nullptr != pJob->pJobBuff)
			{
				delete [] pJob->pJobBuff;
				pJob->pJobBuff = nullptr;
			}
			delete pJob;
			pJob = nullptr;
		}
		LeaveCriticalSection(&m_csJob);
	}
	return bSucc;
}

//处理任务
bool CClientManager::ProcessJob()
{
	sJobItem* pJob = nullptr;
	EnterCriticalSection(&m_csJob);
	dzlog_debug("ProcessJob m_lstJobItem.size()=%d", m_lstJobItem.size());
	if(m_lstJobItem.size() > 0)
	{
		pJob = *(m_lstJobItem.begin());
		m_lstJobItem.pop_front();
	}
	LeaveCriticalSection(&m_csJob);
	if(nullptr == pJob)
	{
		return false;
	}
	EnterCriticalSection(&m_csConnectLock);
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(pJob->i64Index);
	if(iterClient != m_mapClientConnect.end())
	{
		iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
	}
	else
	{
		dzlog_warn("ProcessJob i64Index=%lld not exists!", pJob->i64Index);
	}
	//释放申请的内存
	delete[] pJob->pJobBuff;
	pJob->pJobBuff = nullptr;
	delete pJob;
	pJob = nullptr;
	LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//定时器
void CClientManager::OnBaseTimer()
{
	EnterCriticalSection(&m_csTimer);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.begin();
	for(;itTimer != m_mapAllTimer.end();itTimer++)
	{
		DWORD dwNowTime = GetTickCount();
		if(dwNowTime > itTimer->second.dwEnterTime)
		{
			OnTimer(itTimer->first);
			//循环命中
			if((-1 == itTimer->second.iTimerCount) || (--itTimer->second.iTimerCount > 0))
			{
				itTimer->second.dwEnterTime += itTimer->second.dwDelayTime;
			}
			else
			{
				itTimer = m_mapAllTimer.erase(itTimer);
			}
		}
	}
	LeaveCriticalSection(&m_csTimer);
	return;
}

//设置定时器
int CClientManager::SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount)
{
	dzlog_info("SetTimer dwTimerID=%d, dwDelayTime=%d, iTimerCount=%d", dwTimerID, dwDelayTime, iTimerCount);
	EnterCriticalSection(&m_csTimer);
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
	LeaveCriticalSection(&m_csTimer);
	return 0;
}

//触发定时器
int CClientManager::OnTimer(DWORD dwTimerID)
{
	switch(dwTimerID)
	{
	case TIMER_ID_KEEP_ALIVE:
		{
			//发送心跳数据
			SendData(0, nullptr, 0, MAIN_KEEP_ALIVE, ASS_KEEP_ALIVE, 0);
			break;
		}
	default:
		break;
	}
	return 0;
}

//删除定时器
int CClientManager::KillTimer(DWORD dwTimerID)
{
	dzlog_info("KillTimer dwTimerID=%d", dwTimerID);
	EnterCriticalSection(&m_csTimer);
	std::map<UINT, sTimer>::iterator itTimer = m_mapAllTimer.find(dwTimerID);
	if(itTimer == m_mapAllTimer.end())
	{
		dzlog_warn("KillTimer dwTimerID=%d not exist!", dwTimerID);
	}
	else
	{
		m_mapAllTimer.erase(itTimer);
	}
	LeaveCriticalSection(&m_csTimer);
	return 0;
}


/*********************************************************
*********************************************************
*********************************************************
*********************************************************/
CClientSocket::CClientSocket()
{
	::InitializeCriticalSection(&m_csRecvLock);
	::InitializeCriticalSection(&m_csSendLock);
	InitData();
}

CClientSocket::~CClientSocket()
{
	::DeleteCriticalSection(&m_csRecvLock);
	::DeleteCriticalSection(&m_csSendLock);
	CloseSocket();
}

void CClientSocket::InitData()
{
	m_hSocket = INVALID_SOCKET;
	m_i64Index = 0;
	m_lBeginTime=0;
	m_dwSendBuffLen=0;
	m_dwRecvBuffLen=0;
	m_pManage = NULL;
	memset(&m_SendOverData,0,sizeof(m_SendOverData));
	memset(&m_RecvOverData,0,sizeof(m_RecvOverData));
	m_SendOverData.uOperationType=SOCKET_SND;
	m_RecvOverData.uOperationType=SOCKET_REV;
	m_dwTimeOutCount = 0;
}

//关闭连接
bool CClientSocket::CloseSocket(bool bGraceful)
{
	if(INVALID_SOCKET != m_hSocket)
	{
		if (bGraceful) 
		{
			LINGER lingerStruct;
			lingerStruct.l_onoff = 1;
			lingerStruct.l_linger = 0;
			setsockopt(m_hSocket, SOL_SOCKET, SO_LINGER, (char *)&lingerStruct, sizeof(lingerStruct));
		}
		CancelIo(reinterpret_cast<HANDLE>(m_hSocket));
		closesocket(m_hSocket);
		m_hSocket = INVALID_SOCKET;
	}
	return true;
}

bool CClientSocket::OnRecvBegin()
{
	DWORD dwRecvCount = 0;
	DWORD dwFlags = MSG_PARTIAL;
	EnterCriticalSection(&m_csRecvLock);
	m_RecvOverData.i64HandleID = m_i64Index;
	m_RecvOverData.WSABuffer.buf = m_szRecvBuf + m_dwRecvBuffLen;
	m_RecvOverData.WSABuffer.len = RCV_SIZE - m_dwRecvBuffLen;
	m_RecvOverData.uOperationType = SOCKET_REV_FINISH;
	int iRet = WSARecv(m_hSocket, &m_RecvOverData.WSABuffer, 1, &dwRecvCount, &dwFlags, &m_RecvOverData.OverLapped, NULL);
	if (SOCKET_ERROR == iRet && WSAGetLastError() != WSA_IO_PENDING)
	{
		dzlog_error("OnRecvBegin error=%d, m_i64Index=%lld", WSAGetLastError(), m_i64Index);
		CloseSocket();
		LeaveCriticalSection(&m_csRecvLock);
		return false;
	}
	LeaveCriticalSection(&m_csRecvLock);
	return true;
}

//接收完成函数
int CClientSocket::OnRecvCompleted(DWORD dwRecvCount)
{
	if(dwRecvCount <= 0)
	{
		return 0;
	}
	EnterCriticalSection(&m_csRecvLock);
	//处理数据
	m_dwRecvBuffLen += dwRecvCount;
	int iNewJobCount = 0;
	//获取本次消息的大小
	DWORD* pMsgSize = nullptr;
	while(m_dwRecvBuffLen > sizeof(DWORD))
	{
		pMsgSize = reinterpret_cast<DWORD *>(m_szRecvBuf);
		//接收到了完整的消息
		if(nullptr != pMsgSize && m_dwRecvBuffLen >= *pMsgSize)
		{
			NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead *>(m_szRecvBuf);
			//非法数据包
			if(!pMsgHead || pMsgHead->dwMsgSize < sizeof(NetMsgHead))
			{
				dzlog_error("msg null or size illegal! pMsgHead=%x", pMsgHead);
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return iNewJobCount;
			}
			//将任务交给专门的业务线程处理
			sJobItem *pJob = new sJobItem;
			if(!pJob)
			{
				dzlog_error("new sJobItem fail!");
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return iNewJobCount;
			}
			pJob->i64Index = m_i64Index;
			pJob->dwBufLen = *pMsgSize;
			pJob->pJobBuff = new BYTE[pJob->dwBufLen];
			if(!pJob->pJobBuff)
			{
				dzlog_error("new pJobBuff fail! dwBufLen=%d", pJob->dwBufLen);
				delete pJob;
				pJob = nullptr;
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return iNewJobCount;
			}
			memcpy(pJob->pJobBuff, m_szRecvBuf, pJob->dwBufLen*sizeof(BYTE));
			if(m_pManage->AddJob(pJob))
			{
				++iNewJobCount;
			}

			//删除处理过的缓存数据
			::MoveMemory(m_szRecvBuf, m_szRecvBuf + *pMsgSize, m_dwRecvBuffLen - *pMsgSize);
			m_dwRecvBuffLen -= *pMsgSize;
		}
		else
		{
			break;
		}
	}	
	LeaveCriticalSection(&m_csRecvLock);
	return iNewJobCount;
}

//发送数据函数
int CClientSocket::SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	if (dwDataLen <= MAX_SEND_SIZE)
	{
		//锁定数据
		UINT uSendSize = sizeof(NetMsgHead) + dwDataLen;
		EnterCriticalSection(&m_csSendLock);
		//缓冲区满了
		if (uSendSize > (SED_SIZE - m_dwSendBuffLen))
		{
			LeaveCriticalSection(&m_csSendLock);
			return SOCKET_ERROR;
		}

		//发送数据
		NetMsgHead* pNetHead=(NetMsgHead*)(m_szSendBuf + m_dwSendBuffLen);
		pNetHead->dwMsgSize = uSendSize;
		pNetHead->dwMainID = dwMainID;
		pNetHead->dwAssID = dwAssID;
		pNetHead->dwHandleCode = dwHandleCode;		
		pNetHead->dwReserve = 0;
		if (pData!=NULL) 
			CopyMemory(pNetHead+1, pData, dwDataLen);
		m_dwSendBuffLen += uSendSize;
		int iFinishSize = OnSendBegin()?dwDataLen:0;
		if(MAIN_KEEP_ALIVE == dwMainID && ASS_KEEP_ALIVE == dwAssID)
		{
			++m_dwTimeOutCount;
		}
		LeaveCriticalSection(&m_csSendLock);
		return iFinishSize;
	}
	else
	{
		dzlog_warn("SendData aborted dwDataLen=%d", dwDataLen);
	}
	return 0;
}

//开始发送
bool CClientSocket::OnSendBegin()
{
	EnterCriticalSection(&m_csSendLock);
	if (m_dwSendBuffLen > 0)
	{
		DWORD dwSendCount = 0;
		m_SendOverData.i64HandleID = m_i64Index;
		m_SendOverData.WSABuffer.buf = m_szSendBuf;
		m_SendOverData.WSABuffer.len = m_dwSendBuffLen;
		m_SendOverData.uOperationType = SOCKET_SND_FINISH;
		int iRet = WSASend(m_hSocket,&m_SendOverData.WSABuffer,1,&dwSendCount,0,(LPWSAOVERLAPPED)&m_SendOverData,NULL);
		if (SOCKET_ERROR == iRet && WSAGetLastError() != WSA_IO_PENDING)
		{
			dzlog_error("OnSendBegin error=%d, m_i64Index=%lld", WSAGetLastError(), m_i64Index);
			CloseSocket();
			LeaveCriticalSection(&m_csSendLock);
			return false;
		}
	}
	LeaveCriticalSection(&m_csSendLock);
	return true;
}

//发送完成函数
bool CClientSocket::OnSendCompleted(DWORD dwSendCount)
{
	bool bSucc = true;
	EnterCriticalSection(&m_csSendLock);
	//处理数据
	if (dwSendCount > 0)
	{
		m_dwSendBuffLen -= dwSendCount;
		::MoveMemory(m_szSendBuf, &m_szSendBuf[dwSendCount], m_dwSendBuffLen*sizeof(m_szSendBuf[0]));
	}
	if(m_dwSendBuffLen > 0)
	{
		LeaveCriticalSection(&m_csSendLock);
		//继续发送数据
		bSucc = OnSendBegin();
	}
	else
	{
		LeaveCriticalSection(&m_csSendLock);
	}
	return bSucc;
}

//处理消息
void CClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	m_dwTimeOutCount = 0;
	if(nullptr == pMsgBuf || dwBufLen < sizeof(NetMsgHead))
	{
		dzlog_error("HandleMsg m_i64Index=%lld, pMsgBuf=%x, dwBufLen=%d", m_i64Index, pMsgBuf, dwBufLen);
		return;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//根据不同的消息ID做处理
	if(pMsgHead)
	{
		dzlog_debug("HandleMsg m_i64Index=%lld, dwMsgSize=%d, dwMainID=%d, dwAssID=%d, dwHandleCode=%d, dwReserve=%d", 
			m_i64Index, pMsgHead->dwMsgSize, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
		DWORD dwDataLen = dwBufLen - sizeof(NetMsgHead);
		BYTE *pDataBuf = (BYTE*)pMsgBuf + sizeof(NetMsgHead);
		SendData(pDataBuf, dwDataLen, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode);
	}
	else
	{
		dzlog_error("HandleMsg m_i64Index=%lld, pMsgHead is null!", m_i64Index);
	}
	return;
}



yServerSocket::yServerSocket()
{
	char szPath[MAX_PATH] = {0};
	GetCurrentDirectory(MAX_PATH, szPath);
	std::string strFullPath = szPath;
	strFullPath += "/conf/zlog_conf.conf";
	int iRet = dzlog_init(strFullPath.c_str(), "my_cat");
	m_bWorking = false;
	m_iWSAInitResult = -1;
	m_dwIoThreadNum = 0;
	m_dwJobThreadNum = 0;
	m_hThreadEvent = NULL;
	m_hJobEvent = NULL;
	m_hTimerEvent = NULL;
	m_hCompletionPort = NULL;
	m_hListenThread = NULL;
	m_lsSocket = INVALID_SOCKET;
	m_pClientManager = nullptr;
	dzlog_debug("constructor yServerSocket finish! iRet=%d", iRet);
}

yServerSocket::~yServerSocket()
{
	StopService();
	dzlog_debug("distructor yServerSocket finish!");
	zlog_fini();
}

//停止服务
int yServerSocket::StopService()
{
	dzlog_debug("yServerSocket StopService!");
	m_bWorking = false;
	//删除心跳定时器
	m_pClientManager->KillTimer(TIMER_ID_KEEP_ALIVE);

	if(INVALID_SOCKET != m_lsSocket)
	{
		closesocket(m_lsSocket);
		m_lsSocket = INVALID_SOCKET;
	}

	//退出监听线程
	if (NULL != m_hListenThread) 
	{
		DWORD dwCode = ::WaitForSingleObject(m_hListenThread, TIME_OUT);
		if (dwCode==WAIT_TIMEOUT) ::TerminateThread(m_hListenThread, 1);
		::CloseHandle(m_hListenThread);
		m_hListenThread = NULL;
	}

	//关闭完成端口
	if (NULL != m_hCompletionPort)
	{
		for (UINT i=0; i<m_dwIoThreadNum; i++)
		{
			::PostQueuedCompletionStatus(m_hCompletionPort,0,NULL,NULL);
			::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
			::ResetEvent(m_hThreadEvent);
		}
		m_dwIoThreadNum = 0;
		::CloseHandle(m_hCompletionPort);
		m_hCompletionPort = NULL;
	}

	//关闭所有连接客户端,必须在关闭完成端口后调用
	if(nullptr != m_pClientManager)
	{
		m_pClientManager->SetIsShutDown(true);
	}

	//关闭工作线程
	SetEvent(m_hJobEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);
	m_dwJobThreadNum = 0;
	//关闭定时器线程
	SetEvent(m_hTimerEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);

	//关闭事件
	if (NULL != m_hThreadEvent)
	{
		::CloseHandle(m_hThreadEvent);
		m_hThreadEvent = NULL;
	}
	if (NULL != m_hJobEvent)
	{
		::CloseHandle(m_hJobEvent);
		m_hJobEvent = NULL;
	}
	if (NULL != m_hTimerEvent)
	{
		::CloseHandle(m_hTimerEvent);
		m_hTimerEvent = NULL;
	}

	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
	}

	if(nullptr != m_pClientManager)
	{
		m_pClientManager->CloseAllConnection();
		delete m_pClientManager;
		m_pClientManager = nullptr;
	}
	return 0;
}

int yServerSocket::StartService(int iPort, DWORD dwIoThreadNum, DWORD dwJobThreadNum)
{
	dzlog_debug("yServerSocket StartService! iPort=%d, dwIoThreadNum=%d, dwJobThreadNum=%d", iPort, dwIoThreadNum, dwJobThreadNum);
	if(m_bWorking)
	{
		dzlog_error("StartService is working!");
		return CODE_SERVICE_WORKING;
	}
	if(iPort <= 100)
	{
		dzlog_error("StartService iPort=%d illgelal!", iPort);
		return CODE_SERVICE_PARAM_ERROR;
	}

	if(nullptr == m_pClientManager)
	{
		m_pClientManager = new CClientManager;
	}
	//获取系统信息
	SYSTEM_INFO SystemInfo;
	::GetSystemInfo(&SystemInfo);
	//默认创建cpu核心数两倍的线程数
	if (dwIoThreadNum <=0 || dwIoThreadNum >= MAX_WORD_THREAD_NUMS) 
	{
		dwIoThreadNum = SystemInfo.dwNumberOfProcessors*2;
	}

	if (dwJobThreadNum <=0 || dwJobThreadNum >= MAX_WORD_THREAD_NUMS) 
	{
		dwJobThreadNum = SystemInfo.dwNumberOfProcessors*2;
	}
	m_pClientManager->SetIsShutDown(false);

	WSADATA wsaData;
	m_iWSAInitResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(NO_ERROR != m_iWSAInitResult)
	{
		StopService();
		return CODE_SERVICE_WSA_ERROR;
	}

	m_hThreadEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hThreadEvent)
	{
		StopService();
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hJobEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hJobEvent)
	{
		StopService();
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hTimerEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hTimerEvent)
	{
		StopService();
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hCompletionPort)
	{
		StopService();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	//启动IO线程
	int iRet = StartIOWork(dwIoThreadNum);
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_IO_FAILED;
	}

	//启动工作线程
	iRet = StartJobWork(dwJobThreadNum);
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_WORK_FAILED;
	}

	//启动监听
	iRet = StartListen(iPort);
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_LISTEN_FAILED;
	}

	//自动定时器
	iRet = StartTimer();
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_TIMER_FAILED;
	}
	//启动心跳定时器
	m_pClientManager->SetTimer(TIMER_ID_KEEP_ALIVE, TIME_DEALY_KEEP_ALIVE, -1);

	m_bWorking = true;
	return CODE_SUCC;
}

//开始工作
int yServerSocket::StartIOWork(DWORD dwThreadNum)
{
	dzlog_info("yServerSocket StartIOWork dwThreadNum=%d!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_WORD_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hJobEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	m_dwIoThreadNum = 0;
	for(UINT i=0;i<dwThreadNum;i++)
	{
		threadData.iThreadIndex = i;
		hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, IOThreadProc, &threadData, 0, &uThreadID);
		if(NULL == hThreadHandle)
		{
			return -2;
		}
		++m_dwIoThreadNum;
		//逐个创建线程
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始工作
int yServerSocket::StartJobWork(DWORD dwThreadNum)
{
	dzlog_info("yServerSocket StartJobWork dwThreadNum=%d!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_WORD_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hJobEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	m_dwJobThreadNum = 0;
	for(UINT i=0;i<dwThreadNum;i++)
	{
		threadData.iThreadIndex = i;
		hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, JobThreadProc, &threadData, 0, &uThreadID);
		if(NULL == hThreadHandle)
		{
			return -2;
		}
		++m_dwJobThreadNum;
		//逐个创建线程
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始监听
int yServerSocket::StartListen(int iPort)
{
	m_lsSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);	
	if (INVALID_SOCKET == m_lsSocket)
	{
		return -1;
	}

	SOCKADDR_IN	serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_port = htons(iPort);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	if (SOCKET_ERROR == bind(m_lsSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)))
	{
		closesocket(m_lsSocket);
		return -2;
	}

	if (SOCKET_ERROR == listen(m_lsSocket, SOMAXCONN))
	{
		closesocket(m_lsSocket);
		return -3;
	}

	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hJobEvent;
	threadData.hLsSocket = m_lsSocket;
	threadData.pSocketManage = m_pClientManager;
	threadData.iThreadIndex = 0;
	m_hListenThread = (HANDLE)::_beginthreadex(nullptr, 0, ListenThreadProc, &threadData, 0, &uThreadID);
	if (NULL == m_hListenThread)
	{
		return -4;
	}
	::WaitForSingleObject(m_hThreadEvent, INFINITE);
	::ResetEvent(m_hThreadEvent);
	return 0; 
}

unsigned __stdcall yServerSocket::IOThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CClientManager* pClientManager = pThreadData->pSocketManage;
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	HANDLE hJobEvent = pThreadData->hJobEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pClientManager)
	{
		return 0;
	}
	LPOVERLAPPED lpOverlapped = nullptr;
	while (true)
	{
		DWORD dwTransferred = 0;
		CClientSocket *pClient = nullptr;
		sOverLapped *pOverLapped = nullptr;
		BOOL bIoRet = ::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, (PULONG_PTR)&pClient, &lpOverlapped, INFINITE);
		dzlog_debug("IOThreadProc iThreadIndex=%d, dwTransferred=%d, bIoRet=%d", iThreadIndex, dwTransferred, bIoRet);
		if(!bIoRet)
		{
			DWORD dwIoError = GetLastError();
			dzlog_warn("GetQueuedCompletionStatus dwIoError=%d", dwIoError);
			//INFINITE条件下不可能WAIT_TIMEOUT
			if(dwIoError != WAIT_TIMEOUT)
			{
				//(ERROR_NETNAME_DELETED == dwIoError || WSAECONNRESET == dwIoError || WSAECONNABORTED == dwIoError)
				if(nullptr != pClient)
				{
					pClientManager->CloseOneConnection(pClient, 0);
					continue;
				}
			}
		}
		if(nullptr != lpOverlapped)
		{
			pOverLapped = CONTAINING_RECORD(lpOverlapped, sOverLapped, OverLapped);
		}
		//异常
		if ((nullptr == pClient) || (nullptr == pOverLapped))
		{
			//收到退出消息
			if ((nullptr == pClient) && (nullptr == pOverLapped)) 
			{				
				::SetEvent(hThreadEvent);				
			}
			dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
			return 0;
		}

		bool bSucc = true;
		switch (pOverLapped->uOperationType)
		{
			//SOCKET 数据读取
		case SOCKET_REV:
			{
				bSucc = pClient->OnRecvBegin();
				break;
			}
		case SOCKET_REV_FINISH:
			{
				//继续接收数据
				bSucc = pClient->OnRecvBegin();
				//有新job
				if(bSucc && pClient->OnRecvCompleted(dwTransferred) > 0)
				{
					//通知工作线程
					//EnterCriticalSection(&pClientManager->m_csEvent);
					SetEvent(hJobEvent);
					//LeaveCriticalSection(&pClientManager->m_csEvent);
				}
				break;
			}
			//SOCKET 数据发送
		case SOCKET_SND:
			{
				bSucc = pClient->OnSendBegin();
				break;
			}
		case SOCKET_SND_FINISH:
			{
				bSucc = pClient->OnSendCompleted(dwTransferred);
				break;
			}
		default:
			dzlog_warn("unknow uOperationType=%d i64Index=%lld!", pOverLapped->uOperationType, pOverLapped->i64HandleID);
			break;
		}
		
		if(!bSucc)
		{
			dzlog_error("uOperationType=%d return error!", pOverLapped->uOperationType);
		}
	}
	dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
	return 0;
}

//监听线程
unsigned __stdcall yServerSocket::ListenThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}

	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	SOCKET hLsSocket = pThreadData->hLsSocket;
	CClientManager* pClientManager = pThreadData->pSocketManage;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pClientManager)
	{
		return 0;
	}

	SOCKET hSocket = NULL;
	CClientSocket* pClient = nullptr;
	while (true)
	{
		try
		{
			int	nLen = sizeof(SOCKADDR_IN);
			hSocket = ::WSAAccept(hLsSocket, NULL, &nLen, 0, 0); 
			if (INVALID_SOCKET == hSocket)
			{
				dzlog_error("WSAAccept error! WSAGetLastError=%d", WSAGetLastError());
				return 0;
			}
			else
			{
				pClient = pClientManager->ActiveOneConnection(hSocket);
				if(nullptr != pClient)
				{
					HANDLE hAcceptCompletionPort = ::CreateIoCompletionPort((HANDLE)hSocket, hCompletionPort, (ULONG_PTR)pClient, 0);
					//出错了
					if(NULL == hAcceptCompletionPort || !pClient->OnRecvBegin())
					{
						dzlog_error("CreateIoCompletionPort error=%d", WSAGetLastError());
					}

					//发送连接成功消息
					pClient->SendData(nullptr, 0, 180, 18, 8);
				}
			}
		}
		catch(...)
		{
			dzlog_error("ListenThreadProc catch an error!");
			if (nullptr != pClient) 
			{
				pClientManager->CloseOneConnection(pClient, pClient->GetIndex());
			}
		}
	}
	return 0;
}

unsigned __stdcall yServerSocket::JobThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CClientManager* pClientManager = pThreadData->pSocketManage;
	HANDLE hJobEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pClientManager)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hJobEvent, INFINITE);
		dzlog_debug("JobThreadProc get single! iThreadIndex=%d", iThreadIndex);
		if(pClientManager->GetIsShutDown())
		{
			break;
		}
		//没有任务了
		if(!pClientManager->ProcessJob())
		{
			//EnterCriticalSection(&pClientManager->m_csEvent);
			ResetEvent(hJobEvent);
			//LeaveCriticalSection(&pClientManager->m_csEvent);
		}
	}
	dzlog_info("JobThreadProc exit! iThreadIndex=%d", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//开始定时器
int yServerSocket::StartTimer()
{
	dzlog_info("yServerSocket StartTimer!");
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = NULL;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hTimerEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, TimerThreadProc, &threadData, 0, &uThreadID);
	if(NULL == hThreadHandle)
	{
		return -1;
	}
	//逐个创建线程
	::WaitForSingleObject(m_hThreadEvent, INFINITE);
	::ResetEvent(m_hThreadEvent);
	::CloseHandle(hThreadHandle);
	return 0;
}

unsigned __stdcall yServerSocket::TimerThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CClientManager* pClientManager = pThreadData->pSocketManage;
	HANDLE hTimerEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pClientManager)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hTimerEvent, 10);
		ResetEvent(hTimerEvent);
		pClientManager->OnBaseTimer();
	}
	dzlog_info("TimerThreadProc exit!");
	SetEvent(hThreadEvent);
	return 0;
}