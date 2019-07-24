#include "ClientSocket.h"
#include "SpdlogDef.h"

/*********************************************************
*********************************************************
*********************************************************
*********************************************************/
CClientSocket::CClientSocket()
{
	InitializeCriticalSection(&m_csRecvLock);
	InitializeCriticalSection(&m_csSendLock);
	InitializeCriticalSection(&m_csStateLock);
	InitData();
}

CClientSocket::~CClientSocket()
{
	DeleteCriticalSection(&m_csRecvLock);
	DeleteCriticalSection(&m_csSendLock);
	DeleteCriticalSection(&m_csStateLock);
	CloseSocket();
}

void CClientSocket::InitData()
{
	EnterCriticalSection(&m_csSendLock);
	EnterCriticalSection(&m_csRecvLock);
	EnterCriticalSection(&m_csStateLock);
	m_hSocket = INVALID_SOCKET;
	m_i64Index = 0;
	m_lBeginTime=0;
	m_dwSendBuffLen=0;
	m_dwRecvBuffLen=0;
	memset(&m_SendOverData,0,sizeof(m_SendOverData));
	memset(&m_RecvOverData,0,sizeof(m_RecvOverData));
	m_SendOverData.uOperationType=SOCKET_SND;
	m_RecvOverData.uOperationType=SOCKET_REV;
	m_dwTimeOutCount = 0;
	m_bSending = false;
	LeaveCriticalSection(&m_csStateLock);
	LeaveCriticalSection(&m_csRecvLock);
	LeaveCriticalSection(&m_csSendLock);
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
	m_RecvOverData.i64Index = m_i64Index;
	m_RecvOverData.WSABuffer.buf = m_szRecvBuf + m_dwRecvBuffLen;
	m_RecvOverData.WSABuffer.len = RCV_SIZE - m_dwRecvBuffLen;
	m_RecvOverData.uOperationType = SOCKET_REV_FINISH;
	loggerIns()->debug("recv m_i64Index={}, i64SrvIndex={}, OverLapped={}", m_i64Index, GetSrvIndex(), (void *)(&m_RecvOverData.OverLapped));
	int iRet = WSARecv(m_hSocket, &m_RecvOverData.WSABuffer, 1, &dwRecvCount, &dwFlags, &m_RecvOverData.OverLapped, NULL);
	if (SOCKET_ERROR == iRet && WSAGetLastError() != WSA_IO_PENDING)
	{
		loggerIns()->error("OnRecvBegin error={}, m_i64Index={}, i64SrvIndex={}", WSAGetLastError(), m_i64Index, GetSrvIndex());
		CloseSocket();
		LeaveCriticalSection(&m_csRecvLock);
		return false;
	}
	LeaveCriticalSection(&m_csRecvLock);
	return true;
}

//接收完成函数
bool CClientSocket::OnRecvCompleted(DWORD dwRecvCount, std::list<sJobItem *> &lstJobs)
{
	lstJobs.clear();
	if(dwRecvCount <= 0)
	{
		return true;
	}
	EnterCriticalSection(&m_csRecvLock);
	//处理数据
	m_dwRecvBuffLen += dwRecvCount;
	//接收缓冲区满了
	if(m_dwRecvBuffLen > RCV_SIZE)
	{
		loggerIns()->error("OnRecvCompleted m_dwRecvBuffLen={} is bigger than RCV_SIZE={}, discard recv data!", m_dwRecvBuffLen, dwRecvCount);
		m_dwRecvBuffLen -= dwRecvCount;
		LeaveCriticalSection(&m_csRecvLock);
		return true;
	}

	//获取本次消息的大小
	DWORD* pMsgSize = nullptr;
	while(m_dwRecvBuffLen > sizeof(DWORD))
	{
		pMsgSize = reinterpret_cast<DWORD *>(m_szRecvBuf);
		loggerIns()->debug("OnRecvCompleted m_i64Index={}, i64SrvIndex={}, dwRecvCount={}, m_dwRecvBuffLen={}, MsgSize={}", 
			m_i64Index, GetSrvIndex(), dwRecvCount, m_dwRecvBuffLen, nullptr==pMsgSize?0:*pMsgSize);
		//接收到了完整的消息
		if(nullptr != pMsgSize && m_dwRecvBuffLen >= *pMsgSize)
		{
			NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead *>(m_szRecvBuf);
			DWORD dwMsgSize = pMsgHead->dwMsgSize;
			//非法数据包
			if(!pMsgHead || pMsgHead->dwMsgSize < sizeof(NetMsgHead))
			{
				loggerIns()->error("msg null or size illegal! pMsgHead={}", (void *)(pMsgHead));
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return false;
			}
			//将任务交给专门的业务线程处理
			sJobItem *pJob = m_jobManager.NewJobItem(dwMsgSize);
			if(!pJob)
			{
				loggerIns()->error("NewJobItem fail!");
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return false;
			}
			pJob->i64Index = m_i64Index;
			pJob->dwBufLen = dwMsgSize;
			memcpy(pJob->pJobBuff, m_szRecvBuf, pJob->dwBufLen);
			lstJobs.push_back(pJob);

			//删除处理过的缓存数据
			m_dwRecvBuffLen -= dwMsgSize;
			if(m_dwRecvBuffLen > 0)
			{
				MoveMemory(m_szRecvBuf, m_szRecvBuf + dwMsgSize, m_dwRecvBuffLen);
			}
		}
		else
		{
			break;
		}
	}	
	LeaveCriticalSection(&m_csRecvLock);
	return true;
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
			loggerIns()->error("SendData uSendSize={} is bigger than left buflen={}, discard SendData m_i64Index={}, i64SrvIndex={}, dwMainID={}, dwAssID={}, dwHandleCode{}", 
				uSendSize, SED_SIZE - m_dwSendBuffLen, m_i64Index, GetSrvIndex(), dwMainID, dwAssID, dwHandleCode);
			LeaveCriticalSection(&m_csSendLock);
			return -1;
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
		LeaveCriticalSection(&m_csSendLock);
		int iFinishSize = OnSendBegin()?dwDataLen:0;
		if(MAIN_KEEP_ALIVE == dwMainID && ASS_KEEP_ALIVE == dwAssID)
		{
			InterlockedIncrement(&m_dwTimeOutCount);
		}
		return iFinishSize;
	}
	else
	{
		loggerIns()->warn("SendData aborted dwDataLen={}", dwDataLen);
	}
	return 0;
}

//开始发送
bool CClientSocket::OnSendBegin()
{
	EnterCriticalSection(&m_csSendLock);
	EnterCriticalSection(&m_csStateLock);
	loggerIns()->debug("OnSendBegin m_i64Index={}, i64SrvIndex={}, m_bSending={}, m_dwSendBuffLen={}", m_i64Index, GetSrvIndex(), m_bSending, m_dwSendBuffLen);
	if (!m_bSending && m_dwSendBuffLen > 0)
	{
		m_bSending = true;
		DWORD dwSendCount = 0;
		m_SendOverData.i64Index = m_i64Index;
		m_SendOverData.WSABuffer.buf = m_szSendBuf;
		m_SendOverData.WSABuffer.len = m_dwSendBuffLen;
		m_SendOverData.uOperationType = SOCKET_SND_FINISH;
		loggerIns()->debug("send m_i64Index={}, i64SrvIndex={}, OverLapped={}", m_i64Index, GetSrvIndex(), (void *)(&m_SendOverData.OverLapped));
		int iRet = WSASend(m_hSocket,&m_SendOverData.WSABuffer,1,&dwSendCount,0,&m_SendOverData.OverLapped,NULL);
		if (SOCKET_ERROR == iRet && WSAGetLastError() != WSA_IO_PENDING)
		{
			loggerIns()->error("OnSendBegin error={}, m_i64Index={}, i64SrvIndex={}", WSAGetLastError(), m_i64Index, GetSrvIndex());
			CloseSocket();
			m_bSending = false;
			LeaveCriticalSection(&m_csStateLock);
			LeaveCriticalSection(&m_csSendLock);
			return false;
		}
	}
	LeaveCriticalSection(&m_csStateLock);
	LeaveCriticalSection(&m_csSendLock);
	return true;
}

//发送完成函数
bool CClientSocket::OnSendCompleted(DWORD dwSendCount)
{
	bool bSucc = true;
	EnterCriticalSection(&m_csSendLock);
	loggerIns()->info("OnSendCompleted m_i64Index={}, i64SrvIndex={}, m_dwSendBuffLen={}, dwSendCount={}", 
		m_i64Index, GetSrvIndex(), m_dwSendBuffLen, dwSendCount);
	EnterCriticalSection(&m_csStateLock);
	m_bSending = false;
	//处理数据
	if (dwSendCount > 0)
	{
		m_dwSendBuffLen -= dwSendCount;
		if(m_dwSendBuffLen > 0)
		{
			MoveMemory(m_szSendBuf, &m_szSendBuf[dwSendCount], m_dwSendBuffLen*sizeof(m_szSendBuf[0]));
		}
	}
	LeaveCriticalSection(&m_csStateLock);
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
	InterlockedExchange(&m_dwTimeOutCount, 0);
	return;
}