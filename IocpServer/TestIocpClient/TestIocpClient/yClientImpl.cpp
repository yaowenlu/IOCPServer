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

//处理消息
bool CCltClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	if(nullptr == pMsgBuf || dwBufLen < sizeof(NetMsgHead))
	{
		loggerIns()->error("HandleMsg m_i64Index={}, i64SrvIndex={}, pMsgBuf={}, dwBufLen={}", GetIndex(), m_i64SrvIndex, (void*)(pMsgBuf), dwBufLen);
		return false;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//根据不同的消息ID做处理
	if(pMsgHead)
	{
		loggerIns()->debug("HandleMsg m_i64Index={}, i64SrvIndex={}, dwMsgSize={}, dwMainID={}, dwAssID={}, dwHandleCode={}, dwReserve={}",
			GetIndex(), GetSrvIndex(), pMsgHead->dwMsgSize, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
		if(MAIN_KEEP_ALIVE == pMsgHead->dwMainID)
		{
			switch(pMsgHead->dwAssID)
			{
				//心跳直接回应
			case ASS_SC_KEEP_ALIVE:
				{
					SendData(nullptr, 0, MAIN_KEEP_ALIVE, ASS_CS_KEEP_ALIVE, pMsgHead->dwHandleCode);
					break;
				}
			default:
				break;
			}
		}
		//框架消息
		else if(MAIN_FRAME_MSG == pMsgHead->dwMainID)
		{
			switch(pMsgHead->dwAssID)
			{
				//连接成功消息
			case ASS_CONNECT_SUCC:
				{
					DWORD dwDateLen = dwBufLen - sizeof(NetMsgHead);
					if(dwDateLen != sizeof(sConnectSucc))
					{
						loggerIns()->error("ASS_CONNECT_SUCC dwDateLen={} != sizeof(sConnectSucc)={}", dwDateLen, sizeof(sConnectSucc));
						break;
					}
					sConnectSucc* pConnectSucc = reinterpret_cast<sConnectSucc*>(pMsgHead+1);
					if(nullptr != pConnectSucc)
					{
						m_i64SrvIndex = pConnectSucc->i64SrvIndex;
					}
					break;
				}
			default:
				break;
			}
		}
	}
	else
	{
		loggerIns()->error("HandleMsg m_i64Index={}, i64SrvIndex={}, pMsgHead is null!", GetIndex(), m_i64SrvIndex);
	}
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

//收到一个连接
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

//关闭指定数量的连接
bool CCltSocketManager::CloseConnection(DWORD dwNum)
{
	
	DWORD dwCloseNum = 0;
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient;
	while (++dwCloseNum <= dwNum)
	{
		EnterCriticalSection(&m_csActiveConnectLock);
		iterClient = m_mapClientConnect.begin();
		if (iterClient != m_mapClientConnect.end())
		{
			//先把这个client拿出来，防止其他人访问
			m_mapClientConnect.erase(iterClient->first);
			--m_iClientNums;
			loggerIns()->info("CloseConnection dwCloseNum={}, i64Index={}, i64SrvIndex={}, m_iClientNums={}",
				dwCloseNum, iterClient->first, iterClient->second->GetSrvIndex(), m_iClientNums);
			LeaveCriticalSection(&m_csActiveConnectLock);
		}
		else
		{
			LeaveCriticalSection(&m_csActiveConnectLock);
			break;
		}	
		ReleaseOneConnection(iterClient->second);
	}
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
	m_strServerIp = "";
	m_usServerPort = 0;
	loggerIns()->debug("constructor yClientImpl finish!");
}

yClientImpl::~yClientImpl()
{
	DisConnectServer();
	loggerIns()->debug("distructor yClientImpl finish!");
}

//发送数据
int yClientImpl::SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode)
{
	if(m_bWorking && m_pCltSocketManage)
	{
		return m_pCltSocketManage->SendData(0, pData, dwDataLen, dwMainID, dwAssID, dwHandleCode);
	}
	return -1;
}

//停止服务
int yClientImpl::DisConnectServer()
{
	loggerIns()->debug("DisConnectServer!");
	m_bWorking = false;
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
	
	//关闭测试线程
	if (nullptr != m_pCltSocketManage)
	{
		m_pCltSocketManage->SetIsShutDown(true);
		SetEvent(m_hTestEvent);
		::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
		::ResetEvent(m_hThreadEvent);
	}	

	//关闭事件
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

	//关闭所有连接客户端,必须在关闭完成端口后调用
	if(nullptr != m_pCltSocketManage)
	{
		m_pCltSocketManage->CloseAllConnection();
		delete m_pCltSocketManage;
		m_pCltSocketManage = nullptr;
	}
	return 0;
}

int yClientImpl::ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum)
{
	loggerIns()->debug("ConnectServer strIp=%s, usPort={}, usConnectNum={}", strIp.c_str(), usPort, usConnectNum);
	if(m_bWorking)
	{
		loggerIns()->warn("ConnectServer is working!");
		return 0;
	}

	if(nullptr == m_pCltSocketManage)
	{
		m_pCltSocketManage = new CCltSocketManager();
	}
	//获取系统信息
	SYSTEM_INFO SystemInfo;
	::GetSystemInfo(&SystemInfo);
	//默认创建cpu核心数两倍的线程数
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

	m_strServerIp = strIp;
	m_usServerPort = usPort;
	ActiveConnect(usConnectNum);
	m_pCltSocketManage->SetIsShutDown(false);
	//启动IO线程
	int iRet = StartIOWork(dwIoThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	CCommEvent::GetInstance()->AddOneEvent(EVENT_NEW_JOB_ADD, EventFunc, &m_hJobCompletionPort);
	//启动工作线程
	iRet = StartJobWork(dwJobThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	//启动测试线程
	iRet = StartTest();
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	m_bWorking = true;
	return CODE_SUCC;
}

//激活指定数量的客户端连接
int yClientImpl::ActiveConnect(DWORD dwConnectNum)
{
	loggerIns()->info("ActiveConnect dwConnectNum={}", dwConnectNum);
	if(dwConnectNum <= 0 || m_strServerIp == "" || 0 == m_usServerPort)
	{
		loggerIns()->error("ActiveConnect error! dwConnectNum={}, m_strServerIp=%s, m_usServerPort={}", dwConnectNum, m_strServerIp.c_str(), m_usServerPort);
		return -1;
	}
	//连接服务器
	for(UINT i=0;i<dwConnectNum;i++)
	{
		//客户端套接字
		SOCKET hSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
		if (hSocket == INVALID_SOCKET)
		{
			loggerIns()->warn("socket error={}", WSAGetLastError());
			continue;
		}

		sockaddr_in serAddr;
		serAddr.sin_family = AF_INET;
		serAddr.sin_port = htons(m_usServerPort);
		inet_pton(AF_INET, m_strServerIp.c_str(), (void*)&serAddr.sin_addr.S_un.S_addr);
		//与指定IP地址和端口的服务端连接
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
			//出错了
			if(NULL == hCompletionPort || !pClient->OnRecvBegin())
			{
				loggerIns()->warn("CreateIoCompletionPort or OnRecvBegin error!");
				continue;
			}
		}
	}
	return 0;
}

//开始工作
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
		//逐个创建线程
		WaitForSingleObject(m_hThreadEvent, INFINITE);
		ResetEvent(m_hThreadEvent);
		CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始工作
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
		//逐个创建线程
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
	//线程数据读取完成
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
			//INFINITE条件下不可能WAIT_TIMEOUT
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

		loggerIns()->debug("GetQueuedCompletionStatus m_i64Index={}, i64SrvIndex={}, dwTransferred={}, uOperationType={}, send OperationType={}, recv OperationType={}", 
			pClient->GetIndex(), pClient->GetSrvIndex(), dwTransferred, pOverLapped->uOperationType, pClient->m_SendOverData.uOperationType, pClient->m_RecvOverData.uOperationType);
		//断开连接了
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
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pCltSocketManage)
	{
		return 0;
	}

	DWORD dwTransferred = 0;
	DWORD dwCompleteKey = 0;
	LPOVERLAPPED *pOverLapped = nullptr;
	while (true)
	{
		dwTransferred = 0;
		pOverLapped = nullptr;
		BOOL bIoRet = ::GetQueuedCompletionStatus(hCompletionPort, &dwTransferred, (PULONG_PTR)&dwCompleteKey, pOverLapped, INFINITE);
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
		while (pCltSocketManage->ProcessJob())
		{

		}
	}
	return 0;
}

//测试用
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
	//逐个创建线程
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
	//线程数据读取完成
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
		//没有任务了
		if(pCltSocketManage->GetIsShutDown())
		{
			break;
		}
		BYTE byData[MAX_SEND_SIZE] = {0};
		std::string strData = "kaj8928tskdtjw90vuyv923m209vun0238uy5n2chxq8923hx98thnvutwehnc2934th298nchces测试消息破口破口赔偿金平均分苏东坡就iorjgoi jiojo l;kdslkgj8wu9wi -0";
		memcpy(byData, strData.c_str(), strData.length());
		pCltSocketManage->SendData(0, byData, sizeof(byData), 150, 15, 1);
		int iRandConnect = rand()%11 + 10;
		pClientSocket->ActiveConnect(iRandConnect);
		pCltSocketManage->CloseConnection(iRandConnect);
	}
	loggerIns()->info("TestThreadProc exit! iThreadIndex={}", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//事件回调函数
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