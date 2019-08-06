#include "ClientSocket.h"
#include "SpdlogDef.h"
#include "SocketManager.h"
#include "ProxyDefine.h"

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
	CloseSocket();
	DeleteCriticalSection(&m_csRecvLock);
	DeleteCriticalSection(&m_csSendLock);
	DeleteCriticalSection(&m_csStateLock);
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
	m_iSrvType = INVALID_SRV;
	m_usSrvID = -1;
	m_bIsAsClient = false;
	LeaveCriticalSection(&m_csStateLock);
	LeaveCriticalSection(&m_csRecvLock);
	LeaveCriticalSection(&m_csSendLock);
}

//�ر�����
bool CClientSocket::CloseSocket(bool bGraceful)
{
	if(INVALID_SOCKET != m_hSocket)
	{
		//���Ͷ����е����ݻ�ֱ�ӷ���
		if (!bGraceful) 
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
		m_pManager->CloseOneConnection(this);
		LeaveCriticalSection(&m_csRecvLock);
		return false;
	}
	LeaveCriticalSection(&m_csRecvLock);
	return true;
}

//������ɺ���
bool CClientSocket::OnRecvCompleted(DWORD dwRecvCount, std::list<sJobItem *> &lstJobs)
{
	lstJobs.clear();
	if(dwRecvCount <= 0)
	{
		return true;
	}
	EnterCriticalSection(&m_csRecvLock);
	//��������
	m_dwRecvBuffLen += dwRecvCount;
	//���ջ���������
	if(m_dwRecvBuffLen > RCV_SIZE)
	{
		loggerIns()->error("OnRecvCompleted m_dwRecvBuffLen={} is bigger than RCV_SIZE={}, discard recv data!", m_dwRecvBuffLen, dwRecvCount);
		m_dwRecvBuffLen -= dwRecvCount;
		LeaveCriticalSection(&m_csRecvLock);
		return true;
	}

	//��ȡ������Ϣ�Ĵ�С
	DWORD* pMsgSize = nullptr;
	while(m_dwRecvBuffLen > sizeof(DWORD))
	{
		pMsgSize = reinterpret_cast<DWORD *>(m_szRecvBuf);
		loggerIns()->debug("OnRecvCompleted m_i64Index={}, i64SrvIndex={}, dwRecvCount={}, m_dwRecvBuffLen={}, MsgSize={}", 
			m_i64Index, GetSrvIndex(), dwRecvCount, m_dwRecvBuffLen, nullptr==pMsgSize?0:*pMsgSize);
		//���յ�����������Ϣ
		if(nullptr != pMsgSize && m_dwRecvBuffLen >= *pMsgSize)
		{
			DWORD dwMsgSize = *pMsgSize;
			bool bSucc = true;
			if (dwMsgSize > sizeof(DWORD))
			{
				enHeadType* pHeadType = reinterpret_cast<enHeadType*>(&(m_szRecvBuf[sizeof(DWORD)]));
				if (nullptr != pHeadType)
				{
					enHeadType curType = *pHeadType;
					if (MSG_HEAD == curType)
					{
						loggerIns()->info("recv a normal msg finish, dwMsgSize={}", dwMsgSize);
						if (dwMsgSize < sizeof(NetMsgHead))
						{
							loggerIns()->error("dwMsgSize={} is illegal!", dwMsgSize);
							bSucc = false;
						}
					}
					else if (PROXY_HEAD == curType)
					{
						loggerIns()->info("recv a proxy msg finish, dwMsgSize={}", dwMsgSize);
						if (dwMsgSize < sizeof(NetMsgHead) + sizeof(sProxyHead))
						{
							loggerIns()->error("dwMsgSize={} is illegal!", dwMsgSize);
							bSucc = false;
						}
					}
					else
					{
						loggerIns()->error("recv a unsupport msg! curType={},dwMsgSize={}", curType, dwMsgSize);
						bSucc = false;
					}
				}
				else
				{
					loggerIns()->error("pHeadType is null!");
					bSucc = false;
				}
			}
			else
			{
				loggerIns()->error("dwMsgSize={} is illegal!", dwMsgSize);
				bSucc = false;
			}
			
			
			//�Ƿ����ݰ�
			if(!bSucc)
			{
				m_pManager->CloseOneConnection(this);
				LeaveCriticalSection(&m_csRecvLock);
				return false;
			}
			//�����񽻸�ר�ŵ�ҵ���̴߳���
			sJobItem *pJob = m_jobManager.NewJobItem(dwMsgSize);
			if(!pJob)
			{
				loggerIns()->error("OnRecvCompleted NewJobItem fail!");
				m_pManager->CloseOneConnection(this);
				LeaveCriticalSection(&m_csRecvLock);
				return false;
			}
			pJob->i64Index = m_i64Index;
			pJob->dwBufLen = dwMsgSize;
			memcpy(pJob->pJobBuff, m_szRecvBuf, pJob->dwBufLen);
			lstJobs.push_back(pJob);

			//ɾ��������Ļ�������
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

//�������ݺ���
int CClientSocket::SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	if (dwDataLen <= MAX_SEND_SIZE)
	{
		//��������
		UINT uSendSize = sizeof(NetMsgHead) + dwDataLen;
		EnterCriticalSection(&m_csSendLock);
		//����������
		if (uSendSize > (SED_SIZE - m_dwSendBuffLen))
		{
			loggerIns()->error("SendData uSendSize={} is bigger than left buflen={}, discard SendData m_i64Index={}, i64SrvIndex={}, dwMainID={}, dwAssID={}, dwHandleCode{}", 
				uSendSize, SED_SIZE - m_dwSendBuffLen, m_i64Index, GetSrvIndex(), dwMainID, dwAssID, dwHandleCode);
			LeaveCriticalSection(&m_csSendLock);
			return -1;
		}

		//��������
		NetMsgHead* pNetHead=(NetMsgHead*)(m_szSendBuf + m_dwSendBuffLen);
		pNetHead->headComm.uTotalLen = uSendSize;
		pNetHead->headComm.iHeadType = MSG_HEAD;
		pNetHead->dwMainID = dwMainID;
		pNetHead->dwAssID = dwAssID;
		pNetHead->dwHandleCode = dwHandleCode;		
		pNetHead->dwReserve = 0;
		if (pData!=NULL) 
			CopyMemory(pNetHead+1, pData, dwDataLen);
		m_dwSendBuffLen += uSendSize;
		LeaveCriticalSection(&m_csSendLock);
		int iFinishSize = OnSendBegin()?dwDataLen:0;
		return iFinishSize;
	}
	else
	{
		loggerIns()->warn("SendData aborted dwDataLen={}", dwDataLen);
	}
	return 0;
}

//��ʼ����
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
			m_pManager->CloseOneConnection(this);
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

//������ɺ���
bool CClientSocket::OnSendCompleted(DWORD dwSendCount)
{
	bool bSucc = true;
	EnterCriticalSection(&m_csSendLock);
	loggerIns()->info("OnSendCompleted m_i64Index={}, i64SrvIndex={}, m_dwSendBuffLen={}, dwSendCount={}", 
		m_i64Index, GetSrvIndex(), m_dwSendBuffLen, dwSendCount);
	EnterCriticalSection(&m_csStateLock);
	m_bSending = false;
	//��������
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
		//������������
		bSucc = OnSendBegin();
	}
	else
	{
		LeaveCriticalSection(&m_csSendLock);
	}
	return bSucc;
}

//������Ϣ
bool CClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	//����������Ϣ
	if (nullptr != pMsgBuf && dwBufLen > sizeof(sHeadComm))
	{
		sHeadComm *pHeadComm = reinterpret_cast<sHeadComm*>(pMsgBuf);
		if (!pHeadComm)
		{
			return true;
		}
		if (MSG_HEAD == pHeadComm->iHeadType)
		{
			return HandleNormalMsg(pMsgBuf, dwBufLen);
		}
		else if(PROXY_HEAD == pHeadComm->iHeadType)
		{
			return HandleProxyMsg(pMsgBuf, dwBufLen);
		}
		else
		{
			loggerIns()->error("HandleMsg fail! iHeadType={}", pHeadComm->iHeadType);
		}
	}
	else
	{
		loggerIns()->error("HandleMsg fail! pMsgBuf={}, dwBufLen={}", pMsgBuf, dwBufLen);
	}

	return true;
}

//������ͨ��Ϣ
bool CClientSocket::HandleNormalMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (dwBufLen < sizeof(NetMsgHead))
	{
		loggerIns()->error("HandleNormalMsg fail! dwBufLen={}", dwBufLen);
		return true;
	}
	NetMsgHead *pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	if (!pMsgHead)
	{
		loggerIns()->error("HandleNormalMsg pMsgHead is null!");
		return true;
	}

	//�������
	if (MAIN_KEEP_ALIVE == pMsgHead->dwMainID)
	{
		//�����Ϣ�������Լ�Ҫ����ȥ�ģ�Ҳ�������յ��Ĵ�����������������
		if (ASS_SC_KEEP_ALIVE == pMsgHead->dwAssID)
		{
			//�Լ�����Ϊ�ͻ���˵�����յ����������
			if (GetIsAsClinet())
			{
				//����ֱ�ӻ�Ӧ
				SendData(nullptr, 0, MAIN_KEEP_ALIVE, ASS_CS_KEEP_ALIVE, pMsgHead->dwHandleCode);
			}
			else
			{
				//���������������пͻ���
				InterlockedIncrement(&m_dwTimeOutCount);
				if (m_dwTimeOutCount > MAX_CONNECT_TIME_OUT_COUNT && m_pManager)
				{
					loggerIns()->warn("client is not alive already, m_i64Index={}, m_dwTimeOutCount={}", GetIndex(), m_dwTimeOutCount);
					m_pManager->CloseOneConnection(this);
					return true;
				}
				//����������
				SendData(nullptr, 0, MAIN_KEEP_ALIVE, ASS_SC_KEEP_ALIVE, 0);
			}
		}
		//�յ�������Ϣ
		else if (ASS_CS_KEEP_ALIVE == pMsgHead->dwAssID)
		{
			InterlockedExchange(&m_dwTimeOutCount, 0);
		}
		return true;
	}
	//���������
	else
	{
		InterlockedExchange(&m_dwTimeOutCount, 0);
		//���·�����Ϣ
		if (ASS_SET_SRV_INFO == pMsgHead->dwAssID && MAIN_FRAME_MSG == pMsgHead->dwMainID)
		{
			sSrvInfo* pSrvInfo = reinterpret_cast<sSrvInfo*>(pMsgHead + 1);
			if (nullptr != pSrvInfo)
			{
				SetSrvType(pSrvInfo->iSrvType);
				SetSrvID(pSrvInfo->usSrvID);
			}
			return true;
		}
	}
	return false;
}

//���������Ϣ
bool CClientSocket::HandleProxyMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (dwBufLen < sizeof(sProxyHead) + sizeof(NetMsgHead))
	{
		loggerIns()->error("HandleProxyMsg fail! dwBufLen={}", dwBufLen);
		return true;
	}
	sProxyHead *pHead = reinterpret_cast<sProxyHead*>(pMsgBuf);
	if (!pHead)
	{
		loggerIns()->error("HandleProxyMsg pHead is null!");
		return true;
	}

	return false;
}

//���ʹ�������
int CClientSocket::SendProxyMsg(void *pMsgBuf, DWORD dwBufLen)
{
	EnterCriticalSection(&m_csSendLock);
	//����������
	if (dwBufLen > (SED_SIZE - m_dwSendBuffLen))
	{
		loggerIns()->error("SendProxyMsg dwBufLen={} is bigger than left buflen={}, discard SendProxyMsg m_i64Index={}, i64SrvIndex={}",
			dwBufLen, SED_SIZE - m_dwSendBuffLen, m_i64Index, GetSrvIndex());
		LeaveCriticalSection(&m_csSendLock);
		return -1;
	}

	//��������
	CopyMemory(m_szSendBuf + m_dwSendBuffLen, pMsgBuf, dwBufLen);
	m_dwSendBuffLen += dwBufLen;
	LeaveCriticalSection(&m_csSendLock);
	OnSendBegin();
	return 0;
}

//���ʹ�������
int CClientSocket::SendProxyMsg(sProxyHead proxyHead, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	//��������
	UINT uSendSize = sizeof(sProxyHead) + sizeof(NetMsgHead) + dwDataLen;
	EnterCriticalSection(&m_csSendLock);
	//����������
	if (uSendSize > (SED_SIZE - m_dwSendBuffLen))
	{
		loggerIns()->error("SendProxyMsg uSendSize={} is bigger than left buflen={}, discard SendData m_i64Index={}, i64SrvIndex={}, dwMainID={}, dwAssID={}, dwHandleCode{}",
			uSendSize, SED_SIZE - m_dwSendBuffLen, m_i64Index, GetSrvIndex(), dwMainID, dwAssID, dwHandleCode);
		LeaveCriticalSection(&m_csSendLock);
		return -1;
	}

	//��������
	sProxyHead* pHead = (sProxyHead*)(m_szSendBuf + m_dwSendBuffLen);
	*pHead = proxyHead;
	pHead->headComm.uTotalLen = uSendSize;
	NetMsgHead* pMsgHead = (NetMsgHead*)(m_szSendBuf + m_dwSendBuffLen + sizeof(sProxyHead));
	pMsgHead->headComm.uTotalLen = uSendSize - sizeof(sProxyHead);
	pMsgHead->headComm.iHeadType = MSG_HEAD;
	pMsgHead->dwMainID = dwMainID;
	pMsgHead->dwAssID = dwAssID;
	pMsgHead->dwHandleCode = dwHandleCode;
	pMsgHead->dwReserve = 0;
	if(pData)
		CopyMemory(pMsgHead + 1, pData, dwDataLen);
	m_dwSendBuffLen += uSendSize;
	LeaveCriticalSection(&m_csSendLock);
	OnSendBegin();
	return 0;
}