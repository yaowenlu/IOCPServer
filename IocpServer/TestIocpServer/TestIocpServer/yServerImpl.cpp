#include "stdafx.h"
#include "yServerImpl.h"
#include "CommEvent.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"zlog.lib")


CSrvClientSocket::CSrvClientSocket()
{

};


CSrvClientSocket::~CSrvClientSocket()
{

}

//处理消息
void CSrvClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	__super::HandleMsg(pMsgBuf, dwBufLen);
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

CSrvSocketManager::CSrvSocketManager()
{

};


CSrvSocketManager::~CSrvSocketManager()
{

}

//收到一个连接
CClientSocket* CSrvSocketManager::ActiveOneConnection(SOCKET hSocket)
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
		pClient = new CSrvClientSocket();
	}

	if(nullptr == pClient)
	{
		LeaveCriticalSection(&m_csConnectLock);
		dzlog_error("new CSrvClientSocket fail");
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
	dzlog_info("ActiveOneConnection m_iClientNums=%d, i64Index=%lld", m_iClientNums, i64Index);
	LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

yServerImpl::yServerImpl()
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
	m_pSrvSocketManager = nullptr;
	dzlog_debug("constructor yServerImpl finish! iRet=%d", iRet);
}

yServerImpl::~yServerImpl()
{
	StopService();
	dzlog_debug("distructor yServerImpl finish!");
	zlog_fini();
}

//停止服务
int yServerImpl::StopService()
{
	dzlog_debug("yServerImpl StopService!");
	m_bWorking = false;
	//删除心跳定时器
	if(nullptr != m_pSrvSocketManager)
	{
		m_pSrvSocketManager->KillTimer(TIMER_ID_KEEP_ALIVE);
	}

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
	if(nullptr != m_pSrvSocketManager)
	{
		m_pSrvSocketManager->SetIsShutDown(true);
	}

	//关闭工作线程
	SetEvent(m_hJobEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);
	m_dwJobThreadNum = 0;
	CCommEvent::GetInstance()->RemoveOneEvent(ENEVT_NEW_JOB_ADD);

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

	if(nullptr != m_pSrvSocketManager)
	{
		m_pSrvSocketManager->CloseAllConnection();
		delete m_pSrvSocketManager;
		m_pSrvSocketManager = nullptr;
	}
	return 0;
}

int yServerImpl::StartService(int iPort, DWORD dwIoThreadNum, DWORD dwJobThreadNum)
{
	dzlog_debug("yServerImpl StartService! iPort=%d, dwIoThreadNum=%d, dwJobThreadNum=%d", iPort, dwIoThreadNum, dwJobThreadNum);
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

	if(nullptr == m_pSrvSocketManager)
	{
		m_pSrvSocketManager = new CSrvSocketManager();
	}
	//获取系统信息
	SYSTEM_INFO SystemInfo;
	::GetSystemInfo(&SystemInfo);
	//默认创建cpu核心数两倍的线程数
	if (dwIoThreadNum <=0 || dwIoThreadNum >= MAX_THREAD_NUMS) 
	{
		dwIoThreadNum = SystemInfo.dwNumberOfProcessors*2;
	}

	if (dwJobThreadNum <=0 || dwJobThreadNum >= MAX_THREAD_NUMS) 
	{
		dwJobThreadNum = SystemInfo.dwNumberOfProcessors*2;
	}
	m_pSrvSocketManager->SetIsShutDown(false);

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

	CCommEvent::GetInstance()->AddOneEvent(ENEVT_NEW_JOB_ADD, EventFunc, &m_hJobEvent);
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

	//启动定时器
	iRet = StartTimer();
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_TIMER_FAILED;
	}
	//启动心跳定时器
	m_pSrvSocketManager->SetTimer(TIMER_ID_KEEP_ALIVE, TIME_DEALY_KEEP_ALIVE, -1);

	m_bWorking = true;
	return CODE_SUCC;
}

//开始工作
int yServerImpl::StartIOWork(DWORD dwThreadNum)
{
	dzlog_info("yServerImpl StartIOWork dwThreadNum=%d!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
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
	threadData.pSrvSocketManage = m_pSrvSocketManager;
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
		WaitForSingleObject(m_hThreadEvent, INFINITE);
		ResetEvent(m_hThreadEvent);
		CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始工作
int yServerImpl::StartJobWork(DWORD dwThreadNum)
{
	dzlog_info("yServerImpl StartJobWork dwThreadNum=%d!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
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
	threadData.pSrvSocketManage = m_pSrvSocketManager;
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
int yServerImpl::StartListen(int iPort)
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
	threadData.pSrvSocketManage = m_pSrvSocketManager;
	threadData.iThreadIndex = 0;
	m_hListenThread = (HANDLE)::_beginthreadex(nullptr, 0, ListenThreadProc, &threadData, 0, &uThreadID);
	if (NULL == m_hListenThread)
	{
		return -4;
	}
	WaitForSingleObject(m_hThreadEvent, INFINITE);
	ResetEvent(m_hThreadEvent);
	return 0; 
}

unsigned __stdcall yServerImpl::IOThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CSrvSocketManager* pSrvSocketManage = pThreadData->pSrvSocketManage;
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	HANDLE hJobEvent = pThreadData->hJobEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
	{
		return 0;
	}
	LPOVERLAPPED lpOverlapped = nullptr;
	DWORD dwTransferred = 0;
	CClientSocket *pClient = nullptr;
	sOverLapped *pOverLapped = nullptr;
	while (true)
	{
		dwTransferred = 0;
		pClient = nullptr;
		pOverLapped = nullptr;
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
					dzlog_info("before close 1, m_i64Index=%lld", pClient->GetIndex());
					pSrvSocketManage->CloseOneConnection(pClient);
					continue;
				}
			}
		}
		if(nullptr != lpOverlapped)
		{
			if(nullptr != pClient)
			{
				dzlog_debug("m_i64Index=%lld, lpOverlapped=%x, send=%x, recv=%x", 
					pClient->GetIndex(), lpOverlapped, &pClient->m_SendOverData.OverLapped, &pClient->m_RecvOverData.OverLapped);
			}
			pOverLapped = CONTAINING_RECORD(lpOverlapped, sOverLapped, OverLapped);
		}
		//异常
		if ((nullptr == pClient) || (nullptr == pOverLapped))
		{
			//收到退出消息
			if ((nullptr == pClient) && (nullptr == pOverLapped)) 
			{				
				SetEvent(hThreadEvent);				
			}
			dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
			return 0;
		}

		dzlog_debug("GetQueuedCompletionStatus m_i64Index=%lld, dwTransferred=%d, uOperationType=%d, send OperationType=%d, recv OperationType=%d", 
			pClient->GetIndex(), dwTransferred, pOverLapped->uOperationType, pClient->m_SendOverData.uOperationType, pClient->m_RecvOverData.uOperationType);
		//断开连接了
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			dzlog_info("before close 2, m_i64Index=%lld", pClient->GetIndex());
			pSrvSocketManage->CloseOneConnection(pClient, pOverLapped->i64Index);
			continue;
		}

		bool bSucc = pSrvSocketManage->ProcessIOMessage(pClient, pOverLapped, dwTransferred);
		if(!bSucc)
		{
			dzlog_info("before close 3, m_i64Index=%lld", pClient->GetIndex());
			pSrvSocketManage->CloseOneConnection(pClient);
			dzlog_error("uOperationType=%d return error!", pOverLapped->uOperationType);
		}
	}
	dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
	return 0;
}

//监听线程
unsigned __stdcall yServerImpl::ListenThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}

	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	SOCKET hLsSocket = pThreadData->hLsSocket;
	CSrvSocketManager* pSrvSocketManage = pThreadData->pSrvSocketManage;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
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
				pClient = pSrvSocketManage->ActiveOneConnection(hSocket);
				if(nullptr != pClient)
				{
					HANDLE hAcceptCompletionPort = ::CreateIoCompletionPort((HANDLE)hSocket, hCompletionPort, (ULONG_PTR)pClient, 0);
					//出错了
					if(NULL == hAcceptCompletionPort || !pClient->OnRecvBegin())
					{
						dzlog_error("CreateIoCompletionPort error=%d", WSAGetLastError());
					}

					//发送连接成功消息
					sConnectSucc sConnect;
					sConnect.i64SrvIndex = pClient->GetIndex();
					pClient->SendData(&sConnect, sizeof(sConnectSucc), MAIN_FRAME_MSG, ASS_CONNECT_SUCC, 0);
				}
			}
		}
		catch(...)
		{
			dzlog_error("ListenThreadProc catch an error!");
			if (nullptr != pClient) 
			{
				pSrvSocketManage->CloseOneConnection(pClient, pClient->GetIndex());
			}
		}
	}
	return 0;
}

unsigned __stdcall yServerImpl::JobThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CSrvSocketManager* pSrvSocketManage = pThreadData->pSrvSocketManage;
	HANDLE hJobEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hJobEvent, INFINITE);
		dzlog_debug("JobThreadProc get single! iThreadIndex=%d", iThreadIndex);
		if(pSrvSocketManage->GetIsShutDown())
		{
			break;
		}
		//没有任务了
		if(!pSrvSocketManage->ProcessJob())
		{
			ResetEvent(hJobEvent);
		}
	}
	dzlog_info("JobThreadProc exit! iThreadIndex=%d", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//开始定时器
int yServerImpl::StartTimer()
{
	dzlog_info("yServerImpl StartTimer!");
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = NULL;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hTimerEvent;
	threadData.hLsSocket = NULL;
	threadData.pSrvSocketManage = m_pSrvSocketManager;
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

unsigned __stdcall yServerImpl::TimerThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CSrvSocketManager* pSrvSocketManage = pThreadData->pSrvSocketManage;
	HANDLE hTimerEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hTimerEvent, 10);
		ResetEvent(hTimerEvent);
		pSrvSocketManage->OnBaseTimer();
	}
	dzlog_info("TimerThreadProc exit!");
	SetEvent(hThreadEvent);
	return 0;
}

//事件回调函数
int yServerImpl::EventFunc(string strEventName, void *pFuncParam)
{
	if(ENEVT_NEW_JOB_ADD == strEventName)
	{
		SetEvent(*(HANDLE *)(pFuncParam));
	}
	return 1;
}