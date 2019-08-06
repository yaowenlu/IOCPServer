#include "stdafx.h"
#include "yClientImpl.h"
#include <WS2tcpip.h>
#include "CommEvent.h"
#include "SpdlogDef.h"

#pragma comment(lib,"ws2_32.lib")


/************************************************************************/
/*                                                                      */
/************************************************************************/
CCltClientSocket::CCltClientSocket()
{
	m_i64SrvIndex = 0;
};


CCltClientSocket::~CCltClientSocket()
{

}

//��ʼ������
void CCltClientSocket::InitData()
{
	__super::InitData();
	m_i64SrvIndex = 0;
}

//������ͨ��Ϣ
bool CCltClientSocket::HandleNormalMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (nullptr == pMsgBuf || dwBufLen < sizeof(NetMsgHead))
	{
		loggerIns()->error("HandleMsg m_i64Index={}, i64SrvIndex={}, pMsgBuf={}, dwBufLen={}", GetIndex(), m_i64SrvIndex, (void*)(pMsgBuf), dwBufLen);
		return false;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//���ݲ�ͬ����ϢID������
	if (!pMsgHead)
	{
		loggerIns()->error("{} pMsgHead is null!", __FUNCTION__);
		return true;
	}

	loggerIns()->debug("{} m_i64Index={}, i64SrvIndex={}, dwMsgSize={}, dwMainID={}, dwAssID={}, dwHandleCode={}, dwReserve={}",
		__FUNCTION__, GetIndex(), GetSrvIndex(), pMsgHead->headComm.uTotalLen, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
	if (MAIN_KEEP_ALIVE == pMsgHead->dwMainID)
	{
		switch (pMsgHead->dwAssID)
		{
		//����ֱ�ӻ�Ӧ
		case ASS_SC_KEEP_ALIVE:
		{
			SendData(nullptr, 0, MAIN_KEEP_ALIVE, ASS_CS_KEEP_ALIVE, pMsgHead->dwHandleCode);
			break;
		}
		default:
			break;
		}
	}
	//�����Ϣ
	else if (MAIN_FRAME_MSG == pMsgHead->dwMainID)
	{
		switch (pMsgHead->dwAssID)
		{
			//���ӳɹ���Ϣ
		case ASS_CONNECT_SUCC:
		{
			DWORD dwDateLen = dwBufLen - sizeof(NetMsgHead);
			if (dwDateLen != sizeof(sConnectSucc))
			{
				loggerIns()->error("ASS_CONNECT_SUCC dwDateLen={} != sizeof(sConnectSucc)={}", dwDateLen, sizeof(sConnectSucc));
				break;
			}
			sConnectSucc* pConnectSucc = reinterpret_cast<sConnectSucc*>(pMsgHead + 1);
			if (nullptr != pConnectSucc)
			{
				m_i64SrvIndex = pConnectSucc->i64SrvIndex;
			}
			break;
		}
		default:
			break;
		}
	}

	return true;
}

//���������Ϣ
bool CCltClientSocket::HandleProxyMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (__super::HandleProxyMsg(pMsgBuf, dwBufLen))
	{
		return true;
	}
	sProxyHead *pHead = reinterpret_cast<sProxyHead*>(pMsgBuf);
	if (!pHead)
	{
		loggerIns()->error("{} pHead is null!", __FUNCTION__);
		return true;
	}

	loggerIns()->debug("HandleProxyMsg uTotalLen={}, iSrcType={}, uSrcID={}", pHead->headComm.uTotalLen, pHead->iSrcType, pHead->uSrcID);
	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pHead+1);
	if (!pMsgHead)
	{
		loggerIns()->error("{} pMsgHead is null!", __FUNCTION__);
		return true;
	}
	loggerIns()->debug("HandleProxyMsg uTotalLen={}, dwMainID={}, dwAssID={}, dwHandleCode={}, dwReserve={}", 
		pMsgHead->headComm.uTotalLen, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
	return true;
}


/************************************************************************/
/*                                                                      */
/************************************************************************/
CCltSocketManager::CCltSocketManager()
{

};


CCltSocketManager::~CCltSocketManager()
{

}

//�յ�һ������
CClientSocket* CCltSocketManager::ActiveOneConnection(SOCKET hSocket)
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
		pClient = new CCltClientSocket();
	}

	if(nullptr == pClient)
	{
		loggerIns()->error("new CSrvClientSocket fail");
		return nullptr;
	}
	pClient->SetManager(this);
	pClient->SetSocket(hSocket);
	EnterCriticalSection(&m_csActiveConnectLock);
	__int64 i64Index = ++m_i64UniqueIndex;
	if(i64Index <= 0)
	{
		m_i64UniqueIndex = 1;
		i64Index = 1;
	}
	pClient->SetIndex(i64Index);
	pClient->SetSrvIndex(0);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	loggerIns()->info("ActiveOneConnection m_iClientNums={}, i64Index={}", m_iClientNums, i64Index);
	LeaveCriticalSection(&m_csActiveConnectLock);
	return pClient;
}

//�ر�ָ������������
bool CCltSocketManager::CloseConnection(DWORD dwNum)
{
	DWORD dwCloseNum = 0;
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient;
	CClientSocket* pClient = nullptr;
	while (++dwCloseNum <= dwNum)
	{
		EnterCriticalSection(&m_csActiveConnectLock);
		iterClient = m_mapClientConnect.begin();
		if (iterClient != m_mapClientConnect.end())
		{
			pClient = iterClient->second;
			//�Ȱ����client�ó�������ֹ�����˷���
			m_mapClientConnect.erase(iterClient->first);
			--m_iClientNums;
			loggerIns()->info("CloseConnection dwCloseNum={}, i64Index={}, i64SrvIndex={}, m_iClientNums={}",
				dwCloseNum, pClient->GetIndex(), pClient->GetSrvIndex(), m_iClientNums);
			LeaveCriticalSection(&m_csActiveConnectLock);
		}
		else
		{
			LeaveCriticalSection(&m_csActiveConnectLock);
			break;
		}	
		CloseOneConnection(pClient);
	}
	return true;
}

//���ʹ�������
int CCltSocketManager::SendProxyMsg(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	EnterCriticalSection(&m_csActiveConnectLock);
	sProxyHead proxyHead;
	proxyHead.headComm.iHeadType = PROXY_HEAD;
	proxyHead.iDstType = GAME_SRV;
	proxyHead.uDstID = 1;
	proxyHead.iTransType = trans_p2p;

	//���пͻ���
	if (0 == i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for (;iterClient != m_mapClientConnect.end();)
		{
			if (nullptr != iterClient->second)
			{
				proxyHead.iSrcType = iterClient->second->GetSrvType();
				proxyHead.uSrcID = iterClient->second->GetSrvID();
				iterClient->second->SendProxyMsg(proxyHead, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
				++iterClient;
			}
			else
			{
				iterClient = m_mapClientConnect.erase(iterClient);
				--m_iClientNums;
			}
		}
	}
	//ָ���ͻ���
	else
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(i64Index);
		if (iterClient != m_mapClientConnect.end())
		{
			if (nullptr != iterClient->second)
			{
				proxyHead.iSrcType = iterClient->second->GetSrvType();
				proxyHead.uSrcID = iterClient->second->GetSrvID();
				iterClient->second->SendProxyMsg(proxyHead, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
			}
		}
	}
	LeaveCriticalSection(&m_csActiveConnectLock);
	return 0;
}

//��������
bool CCltSocketManager::ProcessJob()
{
	sJobItem* pJob = nullptr;
	EnterCriticalSection(&m_csJobLock);
	loggerIns()->debug("ProcessJob m_lstJobItem.size()={}", m_lstJobItem.size());
	if (m_lstJobItem.size() > 0)
	{
		pJob = *(m_lstJobItem.begin());
		m_lstJobItem.pop_front();
	}
	LeaveCriticalSection(&m_csJobLock);
	if (nullptr == pJob)
	{
		return false;
	}
	loggerIns()->debug("ProcessJob i64Index={}, dwBufLen={}", pJob->i64Index, pJob->dwBufLen);
	EnterCriticalSection(&m_csActiveConnectLock);
	do
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.find(pJob->i64Index);
		//��Ϣ������û�жϿ�����
		if (iterClient != m_mapClientConnect.end() && nullptr != iterClient->second)
		{
			sHeadComm* pHead = reinterpret_cast<sHeadComm*>(pJob->pJobBuff);
			if (!pHead)
			{
				loggerIns()->warn("ProcessJob pHeadType invalid!");
				break;
			}
			//������Ϣ
			if (PROXY_HEAD == pHead->iHeadType)
			{
				iterClient->second->HandleProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
			}
			//������Ϣ
			else if (MSG_HEAD == pHead->iHeadType)
			{
				iterClient->second->HandleNormalMsg(pJob->pJobBuff, pJob->dwBufLen);
			}
		}
		else
		{
			loggerIns()->warn("ProcessJob i64Index={} not exists!", pJob->i64Index);
		}
	} while (false);

	LeaveCriticalSection(&m_csActiveConnectLock);
	//�ͷ�Job�ڴ�
	m_jobManager.ReleaseJobItem(pJob);
	return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
yClientImpl::yClientImpl()
{
	m_bWorking = false;
	m_iWSAInitResult = -1;
	m_dwIoThreadNum = 0;
	m_dwJobThreadNum = 0;
	m_hThreadEvent = NULL;
	m_hTestEvent = NULL;
	m_hIoCompletionPort = NULL;
	m_hJobCompletionPort = NULL;
	m_pCltSocketManage = nullptr;
	CCommEvent::GetInstance();
	loggerIns()->debug("constructor yClientImpl finish!");
}

yClientImpl::~yClientImpl()
{
	DisConnectServer();
	CCommEvent::GetInstance()->ReleaseInstance();
	loggerIns()->debug("distructor yClientImpl finish!");
}

//��������
int yClientImpl::SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	if(m_bWorking && m_pCltSocketManage)
	{
		if (m_connectInfo.bUseProxy)
		{
			return m_pCltSocketManage->SendProxyMsg(0, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
		}
		else
		{
			return m_pCltSocketManage->SendData(0, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
		}
	}
	return -1;
}

//ֹͣ����
int yClientImpl::DisConnectServer()
{
	loggerIns()->debug("DisConnectServer!");
	loggerIns()->flush();
	m_bWorking = false;
	//�ر�Io��ɶ˿�
	if (NULL != m_hIoCompletionPort)
	{
		for (UINT i=0; i<m_dwIoThreadNum; i++)
		{
			::PostQueuedCompletionStatus(m_hIoCompletionPort,0,NULL,NULL);
			::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
			::ResetEvent(m_hThreadEvent);
		}
		m_dwIoThreadNum = 0;
		::CloseHandle(m_hIoCompletionPort);
		m_hIoCompletionPort = NULL;
	}

	//�Ƴ��¼�
	CCommEvent::GetInstance()->RemoveOneEvent(EVENT_NEW_JOB_ADD);
	//�ر�Job��ɶ˿�
	if (NULL != m_hJobCompletionPort)
	{
		//�رչ����߳�
		for (UINT i = 0; i < m_dwJobThreadNum; i++)
		{
			::PostQueuedCompletionStatus(m_hJobCompletionPort, 0, NULL, NULL);
			::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
			::ResetEvent(m_hThreadEvent);
		}
		m_dwJobThreadNum = 0;
		::CloseHandle(m_hJobCompletionPort);
		m_hJobCompletionPort = NULL;
	}
	
	//�رղ����߳�
	if (nullptr != m_pCltSocketManage)
	{
		m_pCltSocketManage->SetIsShutDown(true);
		SetEvent(m_hTestEvent);
		::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
		::ResetEvent(m_hThreadEvent);
	}	

	//�ر��¼�
	if (NULL != m_hThreadEvent)
	{
		::CloseHandle(m_hThreadEvent);
		m_hThreadEvent = NULL;
	}
	if (NULL != m_hTestEvent)
	{
		::CloseHandle(m_hTestEvent);
		m_hTestEvent = NULL;
	}

	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
	}

	//�ر��������ӿͻ���,�����ڹر���ɶ˿ں����
	if(nullptr != m_pCltSocketManage)
	{
		m_pCltSocketManage->CloseAllConnection();
		delete m_pCltSocketManage;
		m_pCltSocketManage = nullptr;
	}
	return 0;
}

int yClientImpl::ConnectServer(sConnectInfo connectInfo)
{
	m_connectInfo = connectInfo;
	loggerIns()->debug("ConnectServer bUseProxy={}, szServerIp={}, iServerPort={}, iConnectNum={}", 
		connectInfo.bUseProxy, connectInfo.szServerIp, connectInfo.iServerPort, connectInfo.iConnectNum);
	if(m_bWorking)
	{
		loggerIns()->warn("ConnectServer is working!");
		return 0;
	}

	if(nullptr == m_pCltSocketManage)
	{
		m_pCltSocketManage = new CCltSocketManager();
	}
	//��ȡϵͳ��Ϣ
	SYSTEM_INFO SystemInfo;
	::GetSystemInfo(&SystemInfo);
	//Ĭ�ϴ���cpu�������������߳���
	DWORD dwIoThreadNum = SystemInfo.dwNumberOfProcessors*2;
	DWORD dwJobThreadNum = SystemInfo.dwNumberOfProcessors*2;

	WSADATA wsaData;
	m_iWSAInitResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(NO_ERROR != m_iWSAInitResult)
	{
		DisConnectServer();
		return CODE_SERVICE_WSA_ERROR;
	}

	m_hThreadEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hThreadEvent)
	{
		DisConnectServer();
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hTestEvent= ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hTestEvent)
	{
		DisConnectServer();
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hIoCompletionPort)
	{
		DisConnectServer();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	m_hJobCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_hJobCompletionPort)
	{
		DisConnectServer();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}
	CCommEvent::GetInstance()->AddOneEvent(EVENT_NEW_JOB_ADD, EventFunc, &m_hJobCompletionPort);

	ActiveConnect(m_connectInfo.iConnectNum);
	m_pCltSocketManage->SetIsShutDown(false);
	//����IO�߳�
	int iRet = StartIOWork(dwIoThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	//���������߳�
	iRet = StartJobWork(dwJobThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	//���������߳�
	iRet = StartTest();
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	m_bWorking = true;
	return CODE_SUCC;
}

//����ָ�������Ŀͻ�������
int yClientImpl::ActiveConnect(DWORD dwConnectNum)
{
	loggerIns()->info("ActiveConnect dwConnectNum={}", dwConnectNum);
	if(dwConnectNum <= 0 ||  0 == strcmp(m_connectInfo.szServerIp, "") || m_connectInfo.iServerPort <= 0)
	{
		loggerIns()->error("ActiveConnect error! dwConnectNum={}, m_strServerIp=%s, m_usServerPort={}", dwConnectNum, m_connectInfo.szServerIp, m_connectInfo.iServerPort);
		return -1;
	}
	//���ӷ�����
	for(UINT i=0;i<dwConnectNum;i++)
	{
		//�ͻ����׽���
		SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
		if (hSocket == INVALID_SOCKET)
		{
			loggerIns()->warn("socket error={}", WSAGetLastError());
			continue;
		}

		sockaddr_in serAddr;
		serAddr.sin_family = AF_INET;
		serAddr.sin_port = htons(m_connectInfo.iServerPort);
		inet_pton(AF_INET, m_connectInfo.szServerIp, (void*)&serAddr.sin_addr.S_un.S_addr);
		//��ָ��IP��ַ�Ͷ˿ڵķ��������
		if (connect(hSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) 
		{
			loggerIns()->error("connect error={}", WSAGetLastError());
			closesocket(hSocket);
			continue;
		}

		CClientSocket* pClient = m_pCltSocketManage->ActiveOneConnection(hSocket);
		if(nullptr != pClient)
		{
			HANDLE hCompletionPort = ::CreateIoCompletionPort((HANDLE)hSocket, m_hIoCompletionPort, (ULONG_PTR)pClient, 0);
			//������
			if(NULL == hCompletionPort || !pClient->OnRecvBegin())
			{
				loggerIns()->warn("CreateIoCompletionPort or OnRecvBegin error!");
				continue;
			}

			//���ͷ�����Ϣ
			sSrvInfo srvInfo;
			srvInfo.iSrvType = NORMAL_CLIENT;
			srvInfo.usSrvID = 0;//���пͻ��˲������֣�����˻��Լ�����socketʶ��
			pClient->SetSrvType(srvInfo.iSrvType);
			pClient->SetSrvID(srvInfo.usSrvID);
			pClient->SetIsAsClinet(true);
			pClient->SendData(&srvInfo, sizeof(sSrvInfo), MAIN_FRAME_MSG, ASS_SET_SRV_INFO, 0);
		}
	}
	return 0;
}

//��ʼ����
int yClientImpl::StartIOWork(DWORD dwThreadNum)
{
	loggerIns()->info("yClientImpl StartIOWork dwThreadNum={}!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hIoCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.pCltSocketManage = m_pCltSocketManage;
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
		//��������߳�
		WaitForSingleObject(m_hThreadEvent, INFINITE);
		ResetEvent(m_hThreadEvent);
		CloseHandle(hThreadHandle);
	}
	return 0;
}

//��ʼ����
int yClientImpl::StartJobWork(DWORD dwThreadNum)
{
	loggerIns()->info("yClientImpl StartJobWork dwThreadNum={}!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hJobCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.pCltSocketManage = m_pCltSocketManage;
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
		//��������߳�
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

unsigned __stdcall yClientImpl::IOThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CCltSocketManager* pCltSocketManage = pThreadData->pCltSocketManage;
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//�߳����ݶ�ȡ���
	SetEvent(hThreadEvent);
	if(!pCltSocketManage)
	{
		return 0;
	}

	DWORD dwTransferred = 0;
	CClientSocket *pClient = nullptr;
	sOverLapped *pOverLapped = nullptr;
	LPOVERLAPPED lpOverlapped = nullptr;
	while (true)
	{
		dwTransferred = 0;
		pClient = nullptr;
		pOverLapped = nullptr;
		BOOL bIoRet = ::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, (PULONG_PTR)&pClient, &lpOverlapped, INFINITE);
		loggerIns()->debug("IOThreadProc iThreadIndex={}, dwTransferred={}, bIoRet={}", iThreadIndex, dwTransferred, bIoRet);
		if(!bIoRet)
		{
			DWORD dwIoError = GetLastError();
			loggerIns()->warn("GetQueuedCompletionStatus dwIoError={}", dwIoError);
			//INFINITE�����²�����WAIT_TIMEOUT
			if(dwIoError != WAIT_TIMEOUT)
			{
				//(ERROR_NETNAME_DELETED == dwIoError || WSAECONNRESET == dwIoError || WSAECONNABORTED == dwIoError)
				if(nullptr != pClient)
				{
					loggerIns()->info("before close 1, m_i64Index={}, i64SrvIndex={}", pClient->GetIndex(), pClient->GetSrvIndex());
					pCltSocketManage->CloseOneConnection(pClient);
					pCltSocketManage->ReleaseOneConnection(pClient);
					continue;
				}
			}
		}
		if(nullptr != lpOverlapped)
		{
			if(nullptr != pClient)
			{
				loggerIns()->debug("m_i64Index={}, i64SrvIndex={}, lpOverlapped={}, send={}, recv={}", 
					pClient->GetIndex(), pClient->GetSrvIndex(), (void*)(lpOverlapped), (void*)(&pClient->m_SendOverData.OverLapped), (void*)(&pClient->m_RecvOverData.OverLapped));
			}
			pOverLapped = CONTAINING_RECORD(lpOverlapped, sOverLapped, OverLapped);
		}
		//�쳣
		if ((nullptr == pClient) || (nullptr == pOverLapped))
		{
			//�յ��˳���Ϣ
			if ((nullptr == pClient) && (nullptr == pOverLapped)) 
			{				
				SetEvent(hThreadEvent);				
			}
			loggerIns()->info("IOThreadProc exit! iThreadIndex={}", iThreadIndex);
			return 0;
		}

		loggerIns()->debug("GetQueuedCompletionStatus m_i64Index={}, i64SrvIndex={}, dwTransferred={}, uOperationType={}, send OperationType={}, recv OperationType={}", 
			pClient->GetIndex(), pClient->GetSrvIndex(), dwTransferred, pOverLapped->uOperationType, pClient->m_SendOverData.uOperationType, pClient->m_RecvOverData.uOperationType);
		//�Ͽ�������
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			loggerIns()->info("before close 2, m_i64Index={}, i64SrvIndex={}", pClient->GetIndex(), pClient->GetSrvIndex());
			pCltSocketManage->CloseOneConnection(pClient);
			pCltSocketManage->ReleaseOneConnection(pClient);
			continue;
		}

		pCltSocketManage->ProcessIOMessage(pClient, pOverLapped, dwTransferred);		
	}
	loggerIns()->info("IOThreadProc exit! iThreadIndex={}", iThreadIndex);
	return 0;
}

unsigned __stdcall yClientImpl::JobThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CCltSocketManager* pCltSocketManage = pThreadData->pCltSocketManage;
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//�߳����ݶ�ȡ���
	SetEvent(hThreadEvent);
	if(!pCltSocketManage)
	{
		return 0;
	}
	
	DWORD dwTransferred = 0;
	ULONG_PTR dwCompleteKey = 0;
	LPOVERLAPPED lpOverlapped = nullptr;
	while (true)
	{
		dwTransferred = 0;
		lpOverlapped = nullptr;
		BOOL bIoRet = ::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, &dwCompleteKey, &lpOverlapped, INFINITE);
		loggerIns()->debug("JobThreadProc get single! iThreadIndex={}, bIoRet={}, dwTransferred={}", iThreadIndex, bIoRet, dwTransferred);
		if (!bIoRet || 0 == dwTransferred)
		{
			loggerIns()->info("JobThreadProc exit! iThreadIndex={}, bIoRet={}, dwTransferred={}", iThreadIndex, bIoRet, dwTransferred);
			loggerIns()->flush();
			//�����˳���
			if (0 == dwTransferred)
			{
				SetEvent(hThreadEvent);
			}
			return 0;
		}

		//��������
		while (pCltSocketManage->ProcessJob())
		{

		}
	}
	return 0;
}

//������
int yClientImpl::StartTest()
{
	loggerIns()->info("yClientImpl StartTest!");
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = NULL;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hTestEvent = m_hTestEvent;
	threadData.pCltSocketManage = m_pCltSocketManage;
	threadData.iThreadIndex = 0;
	threadData.pClientSocket = this;
	hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, TestThreadProc, &threadData, 0, &uThreadID);
	if(NULL == hThreadHandle)
	{
		return -1;
	}
	//��������߳�
	::WaitForSingleObject(m_hThreadEvent, INFINITE);
	::ResetEvent(m_hThreadEvent);
	::CloseHandle(hThreadHandle);
	return 0;
}

unsigned __stdcall yClientImpl::TestThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CCltSocketManager* pCltSocketManage = pThreadData->pCltSocketManage;
	HANDLE hTestEvent = pThreadData->hTestEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	yClientImpl* pClientSocket = pThreadData->pClientSocket;
	//�߳����ݶ�ȡ���
	SetEvent(hThreadEvent);
	if(!pCltSocketManage || !pClientSocket)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hTestEvent, 5000);
		ResetEvent(hTestEvent);
		loggerIns()->debug("TestThreadProc get single! iThreadIndex={}", iThreadIndex);
		//û��������
		if(pCltSocketManage->GetIsShutDown())
		{
			break;
		}
		BYTE byData[MAX_SEND_SIZE] = {0};
		std::string strData = "kaj8928tskdtjw90vuyv923m209vun0238uy5n2chxq8923hx98thnvutwehnc2934th298nchces������Ϣ�ƿ��ƿ��⳥��ƽ�����ն��¾�iorjgoi jiojo l;kdslkgj8wu9wi -0";
		memcpy(byData, strData.c_str(), strData.length());
		if (pClientSocket->m_connectInfo.bUseProxy)
		{
			pCltSocketManage->SendProxyMsg(0, byData, sizeof(byData), 150, 15, 1);
		}
		else
		{
			pCltSocketManage->SendData(0, byData, sizeof(byData), 150, 15, 1);
		}
		int iRandConnect = rand()%11 + 10;
		pClientSocket->ActiveConnect(iRandConnect);
		pCltSocketManage->CloseConnection(iRandConnect);
	}
	loggerIns()->info("TestThreadProc exit! iThreadIndex={}", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//�¼��ص�����
int yClientImpl::EventFunc(string strEventName, void *pFuncParam)
{
	if (EVENT_NEW_JOB_ADD == strEventName)
	{
		HANDLE* pCompletionPort = reinterpret_cast<HANDLE*>(pFuncParam);
		if (nullptr != pCompletionPort)
		{
			::PostQueuedCompletionStatus(*pCompletionPort, 1, NULL, NULL);
		}
		else
		{
			loggerIns()->warn("EventFunc strEventName={} pCompletionPort is null!", strEventName);
		}
	}
	return 1;
}