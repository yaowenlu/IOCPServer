#include "stdafx.h"
#include "yServerImpl.h"
#include "CommEvent.h"
#include "SpdlogDef.h"

#pragma comment(lib,"ws2_32.lib")


CSrvClientSocket::CSrvClientSocket()
{

};


CSrvClientSocket::~CSrvClientSocket()
{

}

//处理普通消息
bool CSrvClientSocket::HandleNormalMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (__super::HandleNormalMsg(pMsgBuf, dwBufLen))
	{
		return true;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//直接返回消息给客户端（暂时没有做业务相关处理）
	if (pMsgHead)
	{
		loggerIns()->debug("HandleNormalMsg m_i64Index={}, dwMsgSize={}, dwMainID={}, dwAssID={}, dwHandleCode={}, dwReserve={}",
			GetIndex(), pMsgHead->headComm.uTotalLen, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
		//DWORD dwDataLen = dwBufLen - sizeof(NetMsgHead);
		//BYTE *pDataBuf = (BYTE*)pMsgBuf + sizeof(NetMsgHead);
		//SendData(pDataBuf, dwDataLen, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode);
	}
	else
	{
		loggerIns()->error("HandleNormalMsg m_i64Index={}, pMsgHead is null!", GetIndex());
	}
	return true;
}

//处理代理消息
bool CSrvClientSocket::HandleProxyMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if (__super::HandleProxyMsg(pMsgBuf, dwBufLen))
	{
		return true;
	}

	//直接转发消息
	SendProxyMsg(pMsgBuf, dwBufLen);
	return true;
}

/************************************************************************/
/*                                                                      */
/************************************************************************/
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
		pClient = new CSrvClientSocket();
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
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	loggerIns()->info("ActiveOneConnection m_iClientNums={}, i64Index={}", m_iClientNums, i64Index);
	LeaveCriticalSection(&m_csActiveConnectLock);
	return pClient;
}

//处理任务
bool CSrvSocketManager::ProcessJob()
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
		//发送给所有人
		if (0 == pJob->i64Index)
		{
			std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
			for (;iterClient != m_mapClientConnect.end();)
			{
				if (nullptr != iterClient->second)
				{
					//只会给连接到自己的客户端群发消息
					if (NORMAL_CLIENT == iterClient->second->GetSrvType())
					{
						iterClient->second->HandleMsg(pJob->pJobBuff, pJob->dwBufLen);
					}
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
			//消息发起者没有断开连接
			if (iterClient != m_mapClientConnect.end() && nullptr != iterClient->second)
			{
				sHeadComm* pHead = reinterpret_cast<sHeadComm*>(pJob->pJobBuff);
				if (!pHead)
				{
					loggerIns()->warn("ProcessJob pHeadType invalid!");
					break;
				}
				//代理消息
				if (PROXY_HEAD == pHead->iHeadType)
				{
					sProxyHead* pProxyHead = reinterpret_cast<sProxyHead*>(pJob->pJobBuff);
					if (!pProxyHead)
					{
						loggerIns()->warn("ProcessJob pProxyHead invalid!");
						break;
					}
					//找到消息目的地
					if (pProxyHead->iDstType != INVALID_SRV)
					{
						std::map<unsigned __int64, CClientSocket*>::iterator iterClt = m_mapClientConnect.begin();
						//找到对应的服务
						for (;iterClt != m_mapClientConnect.end();iterClt++)
						{
							//找到目的地服务
							if (iterClt->second->GetSrvType() == pProxyHead->iDstType)
							{
								if ((pProxyHead->iTransType == trans_p2p && iterClt->second->GetSrvID() == pProxyHead->uDstID) || (pProxyHead->iTransType == trans_p2g))
								{
									//一般都是请求后的响应
									if (pProxyHead->i64Index != 0)
									{
										//指定ID
										if (pProxyHead->i64Index == iterClt->first)
										{
											iterClt->second->HandleProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
											break;
										}
									}
									//一般都是发起的请求
									else
									{
										//记录请求者，在应答时返回结果给请求者
										pProxyHead->i64Index = pJob->i64Index;
										iterClt->second->HandleProxyMsg(pJob->pJobBuff, pJob->dwBufLen);
										if (pProxyHead->iTransType == trans_p2p)
										{
											break;
										}
									}
								}
							}
						}
					}
				}
				//正常消息
				else if (MSG_HEAD == pHead->iHeadType)
				{
					iterClient->second->HandleNormalMsg(pJob->pJobBuff, pJob->dwBufLen);
				}
			}
			else
			{
				loggerIns()->warn("ProcessJob i64Index={} not exists!", pJob->i64Index);
			}
		}
	} while (false);

	LeaveCriticalSection(&m_csActiveConnectLock);
	//释放Job内存
	m_jobManager.ReleaseJobItem(pJob);
	return true;
}

yServerImpl::yServerImpl()
{		
	m_bWorking = false;
	m_iWSAInitResult = -1;
	m_dwIoThreadNum = 0;
	m_dwJobThreadNum = 0;
	m_hThreadEvent = NULL;
	m_hIoCompletionPort = NULL;
	m_hJobCompletionPort = NULL;
	m_hListenThread = NULL;
	m_lsSocket = INVALID_SOCKET;
	m_pSrvSocketManager = nullptr;
	CCommEvent::GetInstance();
	loggerIns()->debug("constructor yServerImpl finish!");
}

yServerImpl::~yServerImpl()
{
	StopService();
	CCommEvent::GetInstance()->ReleaseInstance();
	loggerIns()->debug("distructor yServerImpl finish!");
}

//停止服务
int yServerImpl::StopService()
{
	loggerIns()->debug("yServerImpl StopService!");
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

	//关闭Io完成端口
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

	//移除事件
	CCommEvent::GetInstance()->RemoveOneEvent(EVENT_NEW_JOB_ADD);
	//关闭Job完成端口
	if (NULL != m_hJobCompletionPort)
	{
		//关闭工作线程
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

	//等待定时器线程关闭
	if (nullptr != m_pSrvSocketManager)
	{
		m_pSrvSocketManager->SetIsShutDown(true);
		::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
		::ResetEvent(m_hThreadEvent);
	}

	//关闭事件
	if (NULL != m_hThreadEvent)
	{
		::CloseHandle(m_hThreadEvent);
		m_hThreadEvent = NULL;
	}

	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
	}

	//关闭所有连接客户端,必须在关闭完成端口后调用
	if(nullptr != m_pSrvSocketManager)
	{
		m_pSrvSocketManager->CloseAllConnection();
		delete m_pSrvSocketManager;
		m_pSrvSocketManager = nullptr;
	}
	return 0;
}

int yServerImpl::StartService(sServerInfo serverInfo)
{
	m_serverInfo = serverInfo;
	loggerIns()->debug("yServerImpl StartService! iListenPort={},iIoThreadNum={},iJobThreadNum={},iSrvType={},iSrvID={}", 
		serverInfo.iListenPort, serverInfo.iIoThreadNum, serverInfo.iJobThreadNum, serverInfo.iSrvType, serverInfo.iSrvID);
	if(m_bWorking)
	{
		loggerIns()->error("StartService is working!");
		return CODE_SERVICE_WORKING;
	}
	if(serverInfo.iListenPort <= 100)
	{
		loggerIns()->error("StartService iPort={} illgelal!", serverInfo.iListenPort);
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
	if (serverInfo.iIoThreadNum <=0 || serverInfo.iIoThreadNum >= MAX_THREAD_NUMS)
	{
		serverInfo.iIoThreadNum = SystemInfo.dwNumberOfProcessors*2;
	}

	if (serverInfo.iJobThreadNum <=0 || serverInfo.iJobThreadNum >= MAX_THREAD_NUMS)
	{
		serverInfo.iJobThreadNum = SystemInfo.dwNumberOfProcessors*2;
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

	m_hIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0 );
	if(NULL == m_hIoCompletionPort)
	{
		StopService();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	m_hJobCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (NULL == m_hJobCompletionPort)
	{
		StopService();
		return CODE_SERVICE_CREATE_IOCP_ERROR;
	}

	//启动IO线程
	int iRet = StartIOWork(serverInfo.iIoThreadNum);
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_IO_FAILED;
	}

	CCommEvent::GetInstance()->AddOneEvent(EVENT_NEW_JOB_ADD, EventFunc, &m_hJobCompletionPort);
	//启动工作线程
	iRet = StartJobWork(serverInfo.iJobThreadNum);
	if(0 != iRet)
	{
		StopService();
		return CODE_SERVICE_WORK_FAILED;
	}

	//启动监听
	iRet = StartListen(serverInfo.iListenPort);
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
	loggerIns()->info("yServerImpl StartIOWork dwThreadNum={}!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hIoCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
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
	loggerIns()->info("yServerImpl StartJobWork dwThreadNum={}!", dwThreadNum);
	if(dwThreadNum <= 0 || dwThreadNum >= MAX_THREAD_NUMS)
	{
		return -1;
	}

	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = m_hJobCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
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
	m_lsSocket = ::WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);	
	if (INVALID_SOCKET == m_lsSocket)
	{
		return -1;
	}

	SOCKADDR_IN	serverAddr;
	ZeroMemory(&serverAddr, sizeof(serverAddr));
	serverAddr.sin_port = htons(iPort);
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	if (SOCKET_ERROR == ::bind(m_lsSocket, (LPSOCKADDR)&serverAddr, sizeof(serverAddr)))
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
	threadData.hCompletionPort = m_hIoCompletionPort;
	threadData.hThreadEvent = m_hThreadEvent;
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
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
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
			//INFINITE条件下不可能WAIT_TIMEOUT
			if(dwIoError != WAIT_TIMEOUT)
			{
				//(ERROR_NETNAME_DELETED == dwIoError || WSAECONNRESET == dwIoError || WSAECONNABORTED == dwIoError)
				if(nullptr != pClient)
				{
					loggerIns()->info("before close 1, m_i64Index={}", pClient->GetIndex());
					pSrvSocketManage->CloseOneConnection(pClient);
					pSrvSocketManage->ReleaseOneConnection(pClient);
					continue;
				}
			}
		}
		if(nullptr != lpOverlapped)
		{
			if(nullptr != pClient)
			{
				loggerIns()->debug("m_i64Index={}, lpOverlapped={}, send={}, recv={}", 
					pClient->GetIndex(), (void*)(lpOverlapped), (void*)(&pClient->m_SendOverData.OverLapped), (void*)(&pClient->m_RecvOverData.OverLapped));
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
			loggerIns()->info("IOThreadProc exit! iThreadIndex={}", iThreadIndex);
			return 0;
		}

		loggerIns()->debug("GetQueuedCompletionStatus m_i64Index={}, dwTransferred={}, uOperationType={}, send OperationType={}, recv OperationType={}", 
			pClient->GetIndex(), dwTransferred, pOverLapped->uOperationType, pClient->m_SendOverData.uOperationType, pClient->m_RecvOverData.uOperationType);
		//断开连接了
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			loggerIns()->info("before close 2, m_i64Index={}", pClient->GetIndex());
			pSrvSocketManage->CloseOneConnection(pClient);
			pSrvSocketManage->ReleaseOneConnection(pClient);
			continue;
		}

		pSrvSocketManage->ProcessIOMessage(pClient, pOverLapped, dwTransferred);
	}
	loggerIns()->info("IOThreadProc exit! iThreadIndex={}", iThreadIndex);
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
				loggerIns()->error("WSAAccept error! WSAGetLastError={}", WSAGetLastError());
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
						loggerIns()->error("CreateIoCompletionPort error={}", WSAGetLastError());
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
			loggerIns()->error("ListenThreadProc catch an error!");
			if (nullptr != pClient) 
			{
				pSrvSocketManage->CloseOneConnection(pClient);
				pSrvSocketManage->ReleaseOneConnection(pClient);
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
	HANDLE hCompletionPort = pThreadData->hCompletionPort;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pSrvSocketManage)
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
			//主动退出的
			if (0 == dwTransferred)
			{
				SetEvent(hThreadEvent);
			}
			return 0;
		}
		
		//处理任务
		while (pSrvSocketManage->ProcessJob())
		{

		}
	}
	return 0;
}

//开始定时器
int yServerImpl::StartTimer()
{
	loggerIns()->info("yServerImpl StartTimer!");
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = NULL;
	threadData.hThreadEvent = m_hThreadEvent;
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

//定时器线程
unsigned __stdcall yServerImpl::TimerThreadProc(LPVOID pParam)
{
	sThreadData* pThreadData = reinterpret_cast<sThreadData*>(pParam);
	if(nullptr == pThreadData)
	{
		return 0;
	}
	CSrvSocketManager* pSrvSocketManage = pThreadData->pSrvSocketManage;
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
		if (pSrvSocketManage->GetIsShutDown())
		{
			break;
		}
		pSrvSocketManage->OnBaseTimer();
		::Sleep(10);
	}
	loggerIns()->info("TimerThreadProc exit!");
	SetEvent(hThreadEvent);
	return 0;
}

//事件回调函数
int yServerImpl::EventFunc(string strEventName, void *pFuncParam)
{
	if(EVENT_NEW_JOB_ADD == strEventName)
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