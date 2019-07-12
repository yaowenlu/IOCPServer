#include "stdafx.h"
#include "yClientSocket.h"
#include <WS2tcpip.h>

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
}

CClientManager::~CClientManager()
{
	::DeleteCriticalSection(&m_csConnectLock);
	::DeleteCriticalSection(&m_csJob);
	//::DeleteCriticalSection(&m_csEvent);
}

//�յ�һ������
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
		return nullptr;
	}
	pClient->SetSocket(hSocket);
	__int64 i64Index = ++m_i64UniqueIndex;
	pClient->SetIndex(i64Index);
	pClient->SetClientManager(this);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	dzlog_info("ActiveOneConnection m_iClientNums=%d,m_i64UniqueIndex=%lld", m_iClientNums, m_i64UniqueIndex);
	::LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

//�ر�ָ������������
bool CClientManager::CloseConnection(DWORD dwNum)
{
	EnterCriticalSection(&m_csConnectLock);
	DWORD dwCloseNum = 0;
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end();iterClient++)
	{
		dzlog_info("CloseConnection dwNum=%d i64Index=%lld, m_iClientNums=%d", dwNum, iterClient->first, m_iClientNums);
		iterClient->second->CloseSocket();
		if(++dwCloseNum >= dwNum)
		{
			break;
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//�ر�һ������
/***************************
�벻Ҫֱ�ӵ��ô˺�����Ҫ�ر�һ�����ӵ��ö�Ӧ�ͻ��˵�CloseSocket�������ɣ�
��ɶ˿ڻ��Զ����ô˺����ͷ�����ռ�õ���Դ
***************************/
bool CClientManager::CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index)
{
	dzlog_info("CloseOneConnection begin i64Index=%lld", i64Index);
	//Ч������
	if (0 == i64Index && nullptr != pClient) 
	{
		i64Index = pClient->GetIndex();
	}
	if ((nullptr == pClient) || (0 == pClient->GetIndex()) || (i64Index != pClient->GetIndex())) 
	{
		return false;
	}

	::EnterCriticalSection(&m_csConnectLock);
	std::map<unsigned __int64, CClientSocket*>::iterator iterFind = m_mapClientConnect.find(i64Index);
	if(iterFind == m_mapClientConnect.end())
	{
		::LeaveCriticalSection(&m_csConnectLock);
		return false;
	}
	
	//��ౣ��MAX_FREE_CLIENT_NUM��������Դ����
	if(m_lstFreeClientConn.size() < MAX_FREE_CLIENT_NUM)
	{
		//iterFind->second��pClient��ͬһ��ָ��
		iterFind->second->CloseSocket();
		m_lstFreeClientConn.push_back(iterFind->second);
	}
	else
	{
		delete iterFind->second;//������ʱ����Զ��ر�����
		iterFind->second = nullptr;
	}
	m_mapClientConnect.erase(iterFind);
	--m_iClientNums;
	dzlog_info("CloseOneConnection end i64Index=%lld, m_iClientNums=%d", i64Index, m_iClientNums);
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//�ر���������
/*******************************
�ڵ��ô˺���ǰ����Ҫ�ȹر�������ɶ˿ڣ�������ܵ��³������
��Ϊ�˺������ͷſͻ����������ڴ棬�����ڴ�ᱻ��ɶ˿�ʹ��
********************************/
bool CClientManager::CloseAllConnection()
{
	dzlog_info("CClientManager CloseAllConnection! m_iClientNums=%d", m_iClientNums);
	::EnterCriticalSection(&m_csConnectLock);
	//�ͷ��������ӵĿͻ���
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end(); iterClient++)
	{
		//�ͷ���Դ
		if(nullptr != iterClient->second)
		{
			delete iterClient->second;//������ʱ����Զ��ر�����
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

	//�ͷ�����
	std::list<sJobItem *>::iterator lstJob = m_lstJobItem.begin();
	for(;lstJob != m_lstJobItem.end(); lstJob++)
	{
		//�ͷ���Դ
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

//��������
int CClientManager::SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	EnterCriticalSection(&m_csConnectLock);
	//���пͻ���
	if(0 == i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end(); iterClient++)
		{
			if(nullptr != iterClient->second)
			{
				iterClient->second->SendData(pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
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
		}
		LeaveCriticalSection(&m_csJob);
	}
	return bSucc;
}

//��������
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
	//�ͷ�������ڴ�
	delete[] pJob->pJobBuff;
	pJob->pJobBuff = nullptr;
	delete pJob;
	pJob = nullptr;
	LeaveCriticalSection(&m_csConnectLock);
	return true;
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
}

//�ر�����
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

//������ɺ���
int CClientSocket::OnRecvCompleted(DWORD dwRecvCount)
{
	if(dwRecvCount <= 0)
	{
		return 0;
	}
	EnterCriticalSection(&m_csRecvLock);
	//��������
	m_dwRecvBuffLen += dwRecvCount;
	int iNewJobCount = 0;
	//��ȡ������Ϣ�Ĵ�С
	DWORD* pMsgSize = nullptr;
	while(m_dwRecvBuffLen > sizeof(DWORD))
	{
		pMsgSize = reinterpret_cast<DWORD *>(m_szRecvBuf);
		//���յ�����������Ϣ
		if(nullptr != pMsgSize && m_dwRecvBuffLen >= *pMsgSize)
		{
			NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead *>(m_szRecvBuf);
			//�Ƿ����ݰ�
			if(!pMsgHead || pMsgHead->dwMsgSize < sizeof(NetMsgHead))
			{
				dzlog_error("msg null or size illegal!");
				CloseSocket();
				LeaveCriticalSection(&m_csRecvLock);
				return iNewJobCount;
			}
			//�����񽻸�ר�ŵ�ҵ���̴߳���
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

			//ɾ��������Ļ�������
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
			LeaveCriticalSection(&m_csSendLock);
			return SOCKET_ERROR;
		}

		//��������
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
		LeaveCriticalSection(&m_csSendLock);
		return iFinishSize;
	}
	else
	{
		dzlog_warn("SendData aborted dwDataLen=%d", dwDataLen);
	}
	return 0;
}

//��ʼ����
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

//������ɺ���
bool CClientSocket::OnSendCompleted(DWORD dwSendCount)
{
	bool bSucc = false;
	EnterCriticalSection(&m_csSendLock);
	//��������
	if (dwSendCount > 0)
	{
		m_dwSendBuffLen -= dwSendCount;
		::MoveMemory(m_szSendBuf, &m_szSendBuf[dwSendCount], m_dwSendBuffLen*sizeof(m_szSendBuf[0]));
	}
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
void CClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if(nullptr == pMsgBuf || dwBufLen < sizeof(NetMsgHead))
	{
		dzlog_error("HandleMsg m_i64Index=%lld, pMsgBuf=%x, dwBufLen=%d", m_i64Index, pMsgBuf, dwBufLen);
		return;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//���ݲ�ͬ����ϢID������
	if(pMsgHead)
	{
		dzlog_debug("HandleMsg m_i64Index=%lld, dwMsgSize=%d, dwMainID=%d, dwAssID=%d, dwHandleCode=%d, dwReserve=%d", 
			m_i64Index, pMsgHead->dwMsgSize, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
	}
	else
	{
		dzlog_error("HandleMsg m_i64Index=%lld, pMsgHead is null!", m_i64Index);
	}
	return;
}




yClientSocket::yClientSocket()
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
	m_hTestEvent = NULL;
	m_hCompletionPort = NULL;
	m_pClientManager = nullptr;
	m_strServerIp = "";
	m_usServerPort = 0;
	dzlog_debug("constructor yClientSocket finish! iRet=%d", iRet);
}

yClientSocket::~yClientSocket()
{
	DisConnectServer();
	dzlog_debug("distructor yClientSocket finish!");
	zlog_fini();
}

//��������
int yClientSocket::SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	if(m_bWorking && m_pClientManager)
	{
		return m_pClientManager->SendData(0, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
	}
	return -1;
}

//ֹͣ����
int yClientSocket::DisConnectServer()
{
	dzlog_debug("DisConnectServer!");
	m_bWorking = false;
	//�ر���ɶ˿�
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

	//�ر��������ӿͻ���,�����ڹر���ɶ˿ں����
	if(nullptr != m_pClientManager)
	{
		m_pClientManager->SetIsShutDown(true);
	}

	//�رչ����߳�
	SetEvent(m_hJobEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);
	m_dwJobThreadNum = 0;

	//�رղ����߳�
	SetEvent(m_hTestEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);

	//�ر��¼�
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
	if (NULL != m_hTestEvent)
	{
		::CloseHandle(m_hTestEvent);
		m_hTestEvent = NULL;
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

int yClientSocket::ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum)
{
	dzlog_debug("ConnectServer strIp=%s, usPort=%d, usConnectNum=%d", strIp.c_str(), usPort, usConnectNum);
	if(m_bWorking)
	{
		dzlog_warn("ConnectServer is working!");
		return 0;
	}

	if(nullptr == m_pClientManager)
	{
		m_pClientManager = new CClientManager;
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

	m_hJobEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hJobEvent)
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

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hCompletionPort)
	{
		DisConnectServer();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	m_strServerIp = strIp;
	m_usServerPort = usPort;
	ActiveConnect(usConnectNum);
	m_pClientManager->SetIsShutDown(false);
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
int yClientSocket::ActiveConnect(DWORD dwConnectNum)
{
	dzlog_info("ActiveConnect dwConnectNum=%d", dwConnectNum);
	if(dwConnectNum <= 0 || m_strServerIp == "" || 0 == m_usServerPort)
	{
		dzlog_error("ActiveConnect error! dwConnectNum=%d, m_strServerIp=%s, m_usServerPort=%d", dwConnectNum, m_strServerIp.c_str(), m_usServerPort);
		return -1;
	}
	//���ӷ�����
	for(UINT i=0;i<dwConnectNum;i++)
	{
		//�ͻ����׽���
		SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
		if (hSocket == INVALID_SOCKET)
		{
			dzlog_warn("socket error=%d", WSAGetLastError());
			continue;
		}

		sockaddr_in serAddr;
		serAddr.sin_family = AF_INET;
		serAddr.sin_port = htons(m_usServerPort);
		inet_pton(AF_INET, m_strServerIp.c_str(), (void*)&serAddr.sin_addr.S_un.S_addr);
		//��ָ��IP��ַ�Ͷ˿ڵķ��������
		if (connect(hSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) 
		{
			dzlog_error("connect error=%d", WSAGetLastError());
			closesocket(hSocket);
			continue;
		}

		CClientSocket* pClient = m_pClientManager->ActiveOneConnection(hSocket);
		if(nullptr != pClient)
		{
			HANDLE hCompletionPort = ::CreateIoCompletionPort((HANDLE)hSocket, m_hCompletionPort, (ULONG_PTR)pClient, 0);
			//������
			if(NULL == hCompletionPort || !pClient->OnRecvBegin())
			{
				dzlog_warn("CreateIoCompletionPort or OnRecvBegin error!");
				continue;
			}
		}
	}
	return 0;
}

//��ʼ����
int yClientSocket::StartIOWork(DWORD dwThreadNum)
{
	dzlog_info("yClientSocket StartIOWork dwThreadNum=%d!", dwThreadNum);
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
		//��������߳�
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//��ʼ����
int yClientSocket::StartJobWork(DWORD dwThreadNum)
{
	dzlog_info("yClientSocket StartJobWork dwThreadNum=%d!", dwThreadNum);
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
		//��������߳�
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

unsigned __stdcall yClientSocket::IOThreadProc(LPVOID pParam)
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
	//�߳����ݶ�ȡ���
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
			//INFINITE�����²�����WAIT_TIMEOUT
			if(dwIoError != WAIT_TIMEOUT)
			{
				if(nullptr != pClient && ERROR_NETNAME_DELETED == dwIoError)
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
		//�쳣
		if ((nullptr == pClient) || (nullptr == pOverLapped))
		{
			//�յ��˳���Ϣ
			if ((nullptr == pClient) && (nullptr == pOverLapped)) 
			{				
				::SetEvent(hThreadEvent);				
			}
			dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
			return 0;
		}

		//�������Ͽ�������
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			pClientManager->CloseOneConnection(pClient, pOverLapped->i64HandleID);
			continue;
		}

		switch (pOverLapped->uOperationType)
		{
			//SOCKET ���ݶ�ȡ
		case SOCKET_REV:
			{
				pClient->OnRecvBegin();
				break;
			}
		case SOCKET_REV_FINISH:
			{
				//������������
				//�н��յ��µ�job
				if(pClient->OnRecvBegin() && (pClient->OnRecvCompleted(dwTransferred) > 0))
				{
					//֪ͨ�����߳�
					//EnterCriticalSection(&pClientManager->m_csEvent);
					SetEvent(hJobEvent);
					//LeaveCriticalSection(&pClientManager->m_csEvent);
				}
				break;
			}
			//SOCKET ���ݷ���
		case SOCKET_SND:
			{
				pClient->OnSendBegin();
				break;
			}
		case SOCKET_SND_FINISH:
			{
				pClient->OnSendCompleted(dwTransferred);
				break;
			}
		default:
			break;
		}
	}
	dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
	return 0;
}

unsigned __stdcall yClientSocket::JobThreadProc(LPVOID pParam)
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
	//�߳����ݶ�ȡ���
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
		//û��������
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

unsigned __stdcall yClientSocket::TestThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CClientManager* pClientManager = pThreadData->pSocketManage;
	HANDLE hTestEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	yClientSocket* pClientSocket = pThreadData->pClientSocket;
	//�߳����ݶ�ȡ���
	SetEvent(hThreadEvent);
	if(!pClientManager || !pClientSocket)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hTestEvent, 5000);
		ResetEvent(hTestEvent);
		dzlog_debug("TestThreadProc get single! iThreadIndex=%d", iThreadIndex);
		//û��������
		if(pClientManager->GetIsShutDown())
		{
			break;
		}
		BYTE byData[MAX_SEND_SIZE] = {0};
		std::string strData = "kaj8928tskdtjw90vuyv923m209vun0238uy5n2chxq8923hx98thnvutwehnc2934th298nchces������Ϣ�ƿ��ƿ��⳥��ƽ�����ն��¾�iorjgoi jiojo l;kdslkgj8wu9wi -0";
		memcpy(byData, strData.c_str(), strData.length());
		pClientManager->SendData(0, byData, sizeof(byData), 150, 15, 1);
		int iRandConnect = rand()%11 + 10;
		pClientSocket->ActiveConnect(iRandConnect);
		pClientManager->CloseConnection(iRandConnect);
	}
	dzlog_info("TestThreadProc exit! iThreadIndex=%d", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//������
int yClientSocket::StartTest()
{
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hTestEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	threadData.iThreadIndex = 0;
	threadData.pClientSocket = this;
	hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, TestThreadProc, &threadData, 0, &uThreadID);
	if(NULL == hThreadHandle)
	{
		return -2;
	}
	//��������߳�
	::WaitForSingleObject(m_hThreadEvent, INFINITE);
	::ResetEvent(m_hThreadEvent);
	::CloseHandle(hThreadHandle);
	return 0;
}