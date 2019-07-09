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

//关闭一个连接
bool CClientManager::CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index)
{
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
	std::map<unsigned __int64, CClientSocket*>::iterator iterFind = m_mapClientConnect.find(i64Index);
	if(iterFind == m_mapClientConnect.end())
	{
		::LeaveCriticalSection(&m_csConnectLock);
		return false;
	}
	//最多保留MAX_FREE_CLIENT_NUM个空闲资源备用
	if(m_lstFreeClientConn.size() < MAX_FREE_CLIENT_NUM)
	{
		m_lstFreeClientConn.push_back(iterFind->second);
	}
	else
	{
		delete iterFind->second;
	}
	m_mapClientConnect.erase(iterFind);
	--m_iClientNums;
	pClient->CloseSocket();
	::LeaveCriticalSection(&m_csConnectLock);
	return true;
}

//关闭所有连接
bool CClientManager::CloseAllConnection()
{
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
int CClientManager::SendData(unsigned __int64 i64Index, void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	//所有客户端
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
	//指定客户端
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
		iterClient->second->HandleMsg(pJob->pJobBuff, pJob->usBufLen);
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

//关闭连接
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
			NetMessageHead* pMsgHead = reinterpret_cast<NetMessageHead *>(m_szRecvBuf);
			//非法数据包
			if(!pMsgHead || pMsgHead->uMessageSize < sizeof(NetMessageHead))
			{
				m_pManage->CloseOneConnection(this, m_i64Index);
				LeaveCriticalSection(&m_csRecvLock);
				return iNewJobCount;
			}
			//将任务交给专门的业务线程处理
			sJobItem *pJob = new sJobItem;
			pJob->i64Index = m_i64Index;
			pJob->usBufLen = *pMsgSize;
			pJob->pJobBuff = new BYTE[pJob->usBufLen];
			memcpy(pJob->pJobBuff, m_szRecvBuf, pJob->usBufLen*sizeof(BYTE));
			m_pManage->AddJob(pJob);
			++iNewJobCount;

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
int CClientSocket::SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	if (uBufLen <= MAX_SEND_SIZE)
	{
		//锁定数据
		UINT uSendSize = sizeof(NetMessageHead) + uBufLen;
		EnterCriticalSection(&m_csSendLock);
		//缓冲区满了
		if (uSendSize > (SED_SIZE - m_dwSendBuffLen))
		{
			LeaveCriticalSection(&m_csSendLock);
			return SOCKET_ERROR;
		}

		//发送数据
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
		if ((::WSASend(m_hSocket,&m_SendOverData.WSABuffer,1,&dwSendCount,0,(LPWSAOVERLAPPED)&m_SendOverData,NULL)==SOCKET_ERROR) && (::WSAGetLastError()!= WSA_IO_PENDING))
		{
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
	bool bSucc = false;
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
void CClientSocket::HandleMsg(void *pMsgBuf, unsigned short usBufLen)
{
	if(nullptr == pMsgBuf || usBufLen < sizeof(NetMessageHead))
	{
		dzlog_error("HandleMsg m_i64Index=%lld, pMsgBuf=%x, usBufLen=%d", m_i64Index, pMsgBuf, usBufLen);
		return;
	}

	NetMessageHead* pMsgHead = reinterpret_cast<NetMessageHead*>(pMsgBuf);
	//根据不同的消息ID做处理
	if(pMsgHead)
	{
		dzlog_debug("HandleMsg m_i64Index=%lld, uMessageSize=%d, bMainID=%d, bAssistantID=%d, bHandleCode=%d, bReserve=%d", 
			m_i64Index, pMsgHead->uMessageSize, pMsgHead->bMainID, pMsgHead->bAssistantID, pMsgHead->bHandleCode, pMsgHead->bReserve);
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
	m_usIoThreadNum = 0;
	m_usJobThreadNum = 0;
	m_hThreadEvent = NULL;
	m_hJobEvent = NULL;
	m_hCompletionPort = NULL;
	m_pClientManager = nullptr;
	dzlog_debug("constructor yClientSocket finish! iRet=%d", iRet);
}

yClientSocket::~yClientSocket()
{
	DisConnectServer();
	dzlog_debug("distructor yClientSocket finish!");
	zlog_fini();
}

//发送数据
int yClientSocket::SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode)
{
	return m_pClientManager->SendData(0, pData, uBufLen, bMainID, bAssistantID, bHandleCode);
}

//停止服务
int yClientSocket::DisConnectServer()
{
	dzlog_debug("DisConnectServer!");
	m_bWorking = false;
	
	//关闭完成端口
	if (NULL != m_hCompletionPort)
	{
		for (UINT i=0; i<m_usIoThreadNum; i++)
		{
			::PostQueuedCompletionStatus(m_hCompletionPort,0,NULL,NULL);
			::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
			::ResetEvent(m_hThreadEvent);
		}
		m_usIoThreadNum = 0;
		::CloseHandle(m_hCompletionPort);
		m_hCompletionPort = NULL;
	}

	//关闭所有连接客户端
	if(nullptr != m_pClientManager)
	{
		m_pClientManager->SetIsShutDown(true);
		m_pClientManager->CloseAllConnection();
		m_pClientManager = nullptr;
	}

	//关闭工作线程
	for (UINT i=0; i<m_usJobThreadNum; i++)
	{
		//触发工作任务
		SetEvent(m_hJobEvent);
		::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
		::ResetEvent(m_hThreadEvent);
	}
	m_usJobThreadNum = 0;

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

	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
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
	//获取系统信息
	SYSTEM_INFO SystemInfo;
	::GetSystemInfo(&SystemInfo);
	//默认创建cpu核心数两倍的线程数
	m_usIoThreadNum = SystemInfo.dwNumberOfProcessors*2;
	m_usJobThreadNum = SystemInfo.dwNumberOfProcessors*2;

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

	m_hJobEvent = ::CreateEvent(NULL,TRUE,false,NULL);
	if(NULL == m_hJobEvent)
	{
		return CODE_SERVICE_CREATE_EVENT_ERROR;
	}

	m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hCompletionPort)
	{
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	//连接服务器
	for(int i=0;i<usConnectNum;i++)
	{
		//客户端套接字
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
		//与指定IP地址和端口的服务端连接
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
			//出错了
			if(NULL == hCompletionPort || !pClient->OnRecvBegin())
			{

			}
			//发送连接成功消息
		}
	}
	//启动IO线程
	int iRet = StartIOWork(m_usIoThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	//启动工作线程
	iRet = StartJobWork(m_usJobThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	m_bWorking = true;
	return CODE_SUCC;
}

//开始工作
int yClientSocket::StartIOWork(unsigned short usThreadNum)
{
	dzlog_debug("yClientSocket StartIOWork usThreadNum=%d!", usThreadNum);
	if(usThreadNum <= 0 || usThreadNum >= MAX_WORD_THREAD_NUMS)
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
	for(int i=0;i<usThreadNum;i++)
	{
		threadData.iThreadIndex = i;
		hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, IOThreadProc, &threadData, 0, &uThreadID);
		if(NULL == hThreadHandle)
		{
			return -2;
		}
		//逐个创建线程
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始工作
int yClientSocket::StartJobWork(unsigned short usThreadNum)
{
	dzlog_debug("yClientSocket StartJobWork usThreadNum=%d!", usThreadNum);
	if(usThreadNum <= 0 || usThreadNum >= MAX_WORD_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hJobEvent;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hJobEvent;
	threadData.hLsSocket = NULL;
	threadData.pSocketManage = m_pClientManager;
	for(int i=0;i<usThreadNum;i++)
	{
		threadData.iThreadIndex = i;
		hThreadHandle = (HANDLE)::_beginthreadex(nullptr, 0, JobThreadProc, &threadData, 0, &uThreadID);
		if(NULL == hThreadHandle)
		{
			return -2;
		}
		//逐个创建线程
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
		::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, (PULONG_PTR)&pClient, &lpOverlapped, INFINITE);
		dzlog_debug("IOThreadProc iThreadIndex=%d, dwTransferred=%d", iThreadIndex, dwTransferred);
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
			dzlog_debug("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
			return 0;
		}

		//服务器断开连接了
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			pClientManager->CloseOneConnection(pClient, pOverLapped->i64HandleID);
			continue;
		}

		switch (pOverLapped->uOperationType)
		{
			//SOCKET 数据读取
		case SOCKET_REV:
			{
				pClient->OnRecvBegin();
				break;
			}
		case SOCKET_REV_FINISH:
			{
				//有接收到新的job
				if(pClient->OnRecvCompleted(dwTransferred)>0)
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
	dzlog_debug("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
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
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pClientManager)
	{
		return 0;
	}

	while (!pClientManager->GetIsShutDown())
	{
		WaitForSingleObject(hJobEvent, INFINITE);
		dzlog_debug("JobThreadProc get single! iThreadIndex=%d", iThreadIndex);
		//没有任务了
		if(pClientManager->GetIsShutDown() || !pClientManager->ProcessJob())
		{
			//EnterCriticalSection(&pClientManager->m_csEvent);
			ResetEvent(hJobEvent);
			//LeaveCriticalSection(&pClientManager->m_csEvent);
		}
	}
	dzlog_debug("JobThreadProc exit! iThreadIndex=%d", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}