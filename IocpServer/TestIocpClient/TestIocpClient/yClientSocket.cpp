#include "stdafx.h"
#include "yClientSocket.h"
#include <WS2tcpip.h>

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"zlog.lib")

CClientManager::CClientManager()
{
	m_mapClientConnect.clear();
	m_lstFreeClientConn.clear();
	m_bShutDown = false;
	m_iClientNums = 0;
	m_i64UniqueIndex = 0;

	::InitializeCriticalSection(&m_csConnectLock);
}

CClientManager::~CClientManager()
{
	::DeleteCriticalSection(&m_csConnectLock);
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
	::LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

//�ر�һ������
bool CClientManager::CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index)
{
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
	m_lstFreeClientConn.push_back(iterFind->second);
	m_mapClientConnect.erase(iterFind);
	--m_iClientNums;
	pClient->CloseSocket();
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//�ر���������
bool CClientManager::CloseAllConnection()
{
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
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//��������
int CClientManager::SendData(unsigned __int64 i64Index, void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	//���пͻ���
	EnterCriticalSection(&m_csConnectLock);
	if(0 == i64Index)
	{
		std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
		for(;iterClient != m_mapClientConnect.end(); iterClient++)
		{
			if(nullptr != iterClient->second)
			{
				iterClient->second->SendData(pData, uBufLen, bMainID, bAssistantID, bHandleCode);
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
				iterClient->second->SendData(pData, uBufLen, bMainID, bAssistantID, bHandleCode);
			}
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
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
	memset(&m_SocketAddr,0,sizeof(m_SocketAddr));
	memset(&m_SendOverData,0,sizeof(m_SendOverData));
	memset(&m_RecvOverData,0,sizeof(m_RecvOverData));
	m_SendOverData.uOperationType=SOCKET_SND;
	m_RecvOverData.uOperationType=SOCKET_REV;
}

//�ر�����
bool CClientSocket::CloseSocket()
{
	if(INVALID_SOCKET != m_hSocket)
	{
		closesocket(m_hSocket);
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
	if ((::WSARecv(m_hSocket, &m_RecvOverData.WSABuffer, 1, &dwRecvCount, &dwFlags, &m_RecvOverData.OverLapped, NULL)) && (::WSAGetLastError() != WSA_IO_PENDING))
	{
		m_pManage->CloseOneConnection(this, m_i64Index);
		LeaveCriticalSection(&m_csRecvLock);
		return false;
	}
	LeaveCriticalSection(&m_csRecvLock);
	return true;
}

//������ɺ���
bool CClientSocket::OnRecvCompleted(DWORD dwRecvCount)
{
	if(dwRecvCount <= 0)
	{
		return false;
	}
	EnterCriticalSection(&m_csRecvLock);
	//��������
	m_dwRecvBuffLen += dwRecvCount;
	//��ȡ������Ϣ�Ĵ�С
	DWORD* pMsgSize = nullptr;
	while(m_dwRecvBuffLen > sizeof(DWORD))
	{
		pMsgSize = reinterpret_cast<DWORD *>(m_szRecvBuf);
		//���յ�����������Ϣ
		if(nullptr != pMsgSize && m_dwRecvBuffLen >= *pMsgSize)
		{
			NetMessageHead* pMsgHead = reinterpret_cast<NetMessageHead *>(m_szRecvBuf);
			//�Ƿ����ݰ�
			if(!pMsgHead || pMsgHead->uMessageSize < sizeof(NetMessageHead))
			{
				m_pManage->CloseOneConnection(this, m_i64Index);
				LeaveCriticalSection(&m_csRecvLock);
				return false;
			}
			//��Ϣ���ݴ�С
			DWORD dwMsgContentSize = *pMsgSize - sizeof(NetMessageHead);
			//֪ͨӦ�ó�������Ϣ

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
	return true;
}

//�������ݺ���
int CClientSocket::SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	if (uBufLen <= MAX_SEND_SIZE)
	{
		//��������
		UINT uSendSize = sizeof(NetMessageHead) + uBufLen;
		EnterCriticalSection(&m_csSendLock);
		//����������
		if (uSendSize > (SED_SIZE - m_dwSendBuffLen))
		{
			LeaveCriticalSection(&m_csSendLock);
			return SOCKET_ERROR;
		}

		//��������
		NetMessageHead* pNetHead=(NetMessageHead*)(m_szSendBuf + m_dwSendBuffLen);
		pNetHead->uMessageSize = uSendSize;
		pNetHead->bMainID = bMainID;
		pNetHead->bAssistantID = bAssistantID;
		pNetHead->bHandleCode = bHandleCode;		
		pNetHead->bReserve = 0;
		if (pData!=NULL) 
			CopyMemory(pNetHead+1, pData, uBufLen);
		m_dwSendBuffLen += uSendSize;
		int iFinishSize = OnSendBegin()?uBufLen:0;
		LeaveCriticalSection(&m_csSendLock);
		return iFinishSize;
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
		if ((::WSASend(m_hSocket,&m_SendOverData.WSABuffer,1,&dwSendCount,0,(LPWSAOVERLAPPED)&m_SendOverData,NULL)==SOCKET_ERROR) && (::WSAGetLastError()!= WSA_IO_PENDING))
		{
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





yClientSocket::yClientSocket()
{
	char szPath[MAX_PATH] = {0};
	GetCurrentDirectory(MAX_PATH, szPath);
	std::string strFullPath = szPath;
	strFullPath += "/conf/test_default.conf";
	int iRet = dzlog_init(strFullPath.c_str(), "my_cat");
	m_bWorking = false;
	m_iWSAInitResult = -1;
	m_usThreadNum = 0;
	m_hThreadEvent = NULL;
	m_hCompletionPort = NULL;
	m_pClientManager = nullptr;
	dzlog_debug("create yClientSocket");
}

yClientSocket::~yClientSocket()
{
	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
	}

	if(nullptr != m_pClientManager)
	{
		delete m_pClientManager;
		m_pClientManager = nullptr;
	}
	dzlog_debug("release yClientSocket");
}

//ֹͣ����
int yClientSocket::DisConnectServer()
{
	m_bWorking = false;
	//�ر���ɶ˿�
	if (NULL != m_hCompletionPort)
	{
		for (UINT i=0; i<m_usThreadNum; i++)
		{
			::PostQueuedCompletionStatus(m_hCompletionPort,0,NULL,NULL);
			::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
			::ResetEvent(m_hThreadEvent);
		}
		m_usThreadNum = 0;
		::CloseHandle(m_hCompletionPort);
		m_hCompletionPort = NULL;
	}

	//�ر��¼�
	if (NULL != m_hThreadEvent)
	{
		::CloseHandle(m_hThreadEvent);
		m_hThreadEvent = NULL;
	}

	//�ر��������ӿͻ���
	if(nullptr != m_pClientManager)
	{
		m_pClientManager->SetIsShutDown(true);
		m_pClientManager->CloseAllConnection();
	}
	return 0;
}

int yClientSocket::ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum)
{
	dzlog_debug("ConnectServer strIp=%s, usPort=%d, usConnectNum=%d", strIp.c_str(), usPort, usConnectNum);
	if(m_bWorking)
	{
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
	m_usThreadNum = SystemInfo.dwNumberOfProcessors*2;;

	WSADATA wsaData;
	m_iWSAInitResult = WSAStartup(MAKEWORD(2,2), &wsaData);
	if(NO_ERROR != m_iWSAInitResult)
	{
		return CODE_SERVICE_WSA_ERROR;
	}

	m_hThreadEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hThreadEvent)
	{
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hCompletionPort)
	{
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	//���ӷ�����
	for(int i=0;i<usConnectNum;i++)
	{
		//�ͻ����׽���
		SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
		if (hSocket == INVALID_SOCKET)
		{
			printf("invalid socket!");
			continue;
		}

		sockaddr_in serAddr;
		serAddr.sin_family = AF_INET;
		serAddr.sin_port = htons(usPort);
		inet_pton(AF_INET, strIp.c_str(), (void*)&serAddr.sin_addr.S_un.S_addr);
		//��ָ��IP��ַ�Ͷ˿ڵķ��������
		if (connect(hSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) 
		{
			printf("connect error !");
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

			}
			//�������ӳɹ���Ϣ
		}
	}

	int iRet = StartWork(m_usThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	m_bWorking = true;
	return CODE_SUCC;
}

//��ʼ����
int yClientSocket::StartWork(unsigned short usThreadNum)
{
	if(usThreadNum <= 0 || usThreadNum >= MAX_WORD_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	for(int i=0;i<usThreadNum;i++)
	{
		hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, WorkThreadProc, &threadData, 0, &uThreadID);
		if(NULL == hThreadHandle)
		{
			return -2;
		}
		//��������߳�
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//��������
int yClientSocket::SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	return m_pClientManager->SendData(0, pData, uBufLen, bMainID, bAssistantID, bHandleCode);
}

unsigned __stdcall yClientSocket::WorkThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CClientManager* pClientManager = pThreadData->pSocketManage;
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
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
		::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, (PULONG_PTR)&pClient, &lpOverlapped, INFINITE);
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
			_endthreadex(0);
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
				pClient->OnRecvCompleted(dwTransferred);
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
	return 0;
}