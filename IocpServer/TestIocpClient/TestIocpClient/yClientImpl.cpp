#include "stdafx.h"
#include "yClientImpl.h"
#include <WS2tcpip.h>
#include "CommEvent.h"

#pragma comment(lib,"ws2_32.lib")
#pragma comment(lib,"zlog.lib")


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
void CCltClientSocket::HandleMsg(void *pMsgBuf, DWORD dwBufLen)
{
	__super::HandleMsg(pMsgBuf, dwBufLen);
	if(nullptr == pMsgBuf || dwBufLen < sizeof(NetMsgHead))
	{
		dzlog_error("HandleMsg m_i64Index=%lld, i64SrvIndex=%lld, pMsgBuf=%x, dwBufLen=%d", m_i64Index, m_i64SrvIndex, pMsgBuf, dwBufLen);
		return;
	}

	NetMsgHead* pMsgHead = reinterpret_cast<NetMsgHead*>(pMsgBuf);
	//根据不同的消息ID做处理
	if(pMsgHead)
	{
		dzlog_debug("HandleMsg m_i64Index=%lld, i64SrvIndex=%lld, dwMsgSize=%d, dwMainID=%d, dwAssID=%d, dwHandleCode=%d, dwReserve=%d", 
			m_i64Index, m_i64SrvIndex, pMsgHead->dwMsgSize, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode, pMsgHead->dwReserve);
		if(MAIN_KEEP_ALIVE == pMsgHead->dwMainID)
		{
			switch(pMsgHead->dwAssID)
			{
				//心跳直接回应
			case ASS_KEEP_ALIVE:
				{
					SendData(nullptr, 0, pMsgHead->dwMainID, pMsgHead->dwAssID, pMsgHead->dwHandleCode);
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
						dzlog_error("ASS_CONNECT_SUCC dwDateLen=%d != sizeof(sConnectSucc)=%d", dwDateLen, sizeof(sConnectSucc));
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
		dzlog_error("HandleMsg m_i64Index=%lld, i64SrvIndex=%lld, pMsgHead is null!", m_i64Index, m_i64SrvIndex);
	}
	return;
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
		pClient = new CCltClientSocket();
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
	pClient->SetSrvIndex(0);
	m_mapClientConnect[i64Index] = pClient;
	++m_iClientNums;
	dzlog_info("ActiveOneConnection m_iClientNums=%d, i64Index=%lld", m_iClientNums, i64Index);
	LeaveCriticalSection(&m_csConnectLock);
	return pClient;
}

//关闭指定数量的连接
bool CCltSocketManager::CloseConnection(DWORD dwNum)
{
	EnterCriticalSection(&m_csConnectLock);
	DWORD dwCloseNum = 0;
	std::map<unsigned __int64, CClientSocket*>::iterator iterClient = m_mapClientConnect.begin();
	for(;iterClient != m_mapClientConnect.end();)
	{
		dzlog_info("CloseConnection dwNum=%d i64Index=%lld, i64SrvIndex=%lld, m_iClientNums=%d", 
			dwNum, iterClient->first, iterClient->second->GetSrvIndex(), m_iClientNums);
		ReleaseOneConnection(iterClient->first, false);
		iterClient = m_mapClientConnect.erase(iterClient);
		if(++dwCloseNum >= dwNum)
		{
			break;
		}
	}
	LeaveCriticalSection(&m_csConnectLock);
	return true;
}


/************************************************************************/
/*                                                                      */
/************************************************************************/
yClientImpl::yClientImpl()
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
	m_pCltSocketManage = nullptr;
	m_strServerIp = "";
	m_usServerPort = 0;
	dzlog_debug("constructor yClientImpl finish! iRet=%d", iRet);
}

yClientImpl::~yClientImpl()
{
	DisConnectServer();
	dzlog_debug("distructor yClientImpl finish!");
	zlog_fini();
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
	dzlog_debug("DisConnectServer!");
	m_bWorking = false;
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
	if(nullptr != m_pCltSocketManage)
	{
		m_pCltSocketManage->SetIsShutDown(true);
	}

	//关闭工作线程
	SetEvent(m_hJobEvent);
	::WaitForSingleObject(m_hThreadEvent, TIME_OUT);
	::ResetEvent(m_hThreadEvent);
	m_dwJobThreadNum = 0;
	CCommEvent::GetInstance()->RemoveOneEvent(ENEVT_NEW_JOB_ADD);

	//关闭测试线程
	SetEvent(m_hTestEvent);
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
	if (NULL != m_hTestEvent)
	{
		::CloseHandle(m_hTestEvent);
		m_hTestEvent = NULL;
	}

	if(NO_ERROR == m_iWSAInitResult)
	{
		WSACleanup();
	}

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
	dzlog_debug("ConnectServer strIp=%s, usPort=%d, usConnectNum=%d", strIp.c_str(), usPort, usConnectNum);
	if(m_bWorking)
	{
		dzlog_warn("ConnectServer is working!");
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
	m_pCltSocketManage->SetIsShutDown(false);
	//启动IO线程
	int iRet = StartIOWork(dwIoThreadNum);
	if(0 != iRet)
	{
		DisConnectServer();
		return CODE_SERVICE_WORK_FAILED;
	}

	CCommEvent::GetInstance()->AddOneEvent(ENEVT_NEW_JOB_ADD, EventFunc, &m_hJobEvent);
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
	dzlog_info("ActiveConnect dwConnectNum=%d", dwConnectNum);
	if(dwConnectNum <= 0 || m_strServerIp == "" || 0 == m_usServerPort)
	{
		dzlog_error("ActiveConnect error! dwConnectNum=%d, m_strServerIp=%s, m_usServerPort=%d", dwConnectNum, m_strServerIp.c_str(), m_usServerPort);
		return -1;
	}
	//连接服务器
	for(UINT i=0;i<dwConnectNum;i++)
	{
		//客户端套接字
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
		//与指定IP地址和端口的服务端连接
		if (connect(hSocket, (sockaddr *)&serAddr, sizeof(serAddr)) == SOCKET_ERROR) 
		{
			dzlog_error("connect error=%d", WSAGetLastError());
			closesocket(hSocket);
			continue;
		}

		CClientSocket* pClient = m_pCltSocketManage->ActiveOneConnection(hSocket);
		if(nullptr != pClient)
		{
			HANDLE hCompletionPort = ::CreateIoCompletionPort((HANDLE)hSocket, m_hCompletionPort, (ULONG_PTR)pClient, 0);
			//出错了
			if(NULL == hCompletionPort || !pClient->OnRecvBegin())
			{
				dzlog_warn("CreateIoCompletionPort or OnRecvBegin error!");
				continue;
			}
		}
	}
	return 0;
}

//开始工作
int yClientImpl::StartIOWork(DWORD dwThreadNum)
{
	dzlog_info("yClientImpl StartIOWork dwThreadNum=%d!", dwThreadNum);
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
		::WaitForSingleObject(m_hThreadEvent, INFINITE);
		::ResetEvent(m_hThreadEvent);
		::CloseHandle(hThreadHandle);
	}
	return 0;
}

//开始工作
int yClientImpl::StartJobWork(DWORD dwThreadNum)
{
	dzlog_info("yClientImpl StartJobWork dwThreadNum=%d!", dwThreadNum);
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
	HANDLE hJobEvent = pThreadData->hJobEvent;
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
					dzlog_info("before close 1, m_i64Index=%lld, i64SrvIndex=%lld", pClient->GetIndex(), pClient->GetSrvIndex());
					pCltSocketManage->CloseOneConnection(pClient);
					continue;
				}
			}
		}
		if(nullptr != lpOverlapped)
		{
			if(nullptr != pClient)
			{
				dzlog_debug("m_i64Index=%lld, i64SrvIndex=%lld, lpOverlapped=%x, send=%x, recv=%x", 
					pClient->GetIndex(), pClient->GetSrvIndex(), lpOverlapped, &pClient->m_SendOverData.OverLapped, pClient->m_RecvOverData.OverLapped);
			}
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

		dzlog_debug("GetQueuedCompletionStatus m_i64Index=%lld, i64SrvIndex=%lld, dwTransferred=%d, uOperationType=%d, send OperationType=%d, recv OperationType=%d", 
			pClient->GetIndex(), pClient->GetSrvIndex(), dwTransferred, pOverLapped->uOperationType, pClient->m_SendOverData.uOperationType, pClient->m_RecvOverData.uOperationType);
		//断开连接了
		if ((0 == dwTransferred) && (SOCKET_REV_FINISH == pOverLapped->uOperationType))
		{
			dzlog_info("before close 2, m_i64Index=%lld, i64SrvIndex=%lld", pClient->GetIndex(), pClient->GetSrvIndex());
			pCltSocketManage->CloseOneConnection(pClient);
			continue;
		}

		bool bSucc = pCltSocketManage->ProcessIOMessage(pClient, pOverLapped, dwTransferred);		
		if(!bSucc)
		{
			dzlog_info("before close 3, m_i64Index=%lld, i64SrvIndex=%lld", pClient->GetIndex(), pClient->GetSrvIndex());
			pCltSocketManage->CloseOneConnection(pClient);
			dzlog_error("uOperationType=%d return error!", pOverLapped->uOperationType);
		}
	}
	dzlog_info("IOThreadProc exit! iThreadIndex=%d", iThreadIndex);
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
	HANDLE hJobEvent = pThreadData->hJobEvent;
	HANDLE hThreadEvent = pThreadData->hThreadEvent;
	int iThreadIndex = pThreadData->iThreadIndex;
	//线程数据读取完成
	SetEvent(hThreadEvent);
	if(!pCltSocketManage)
	{
		return 0;
	}

	while (true)
	{
		WaitForSingleObject(hJobEvent, INFINITE);
		dzlog_debug("JobThreadProc get single! iThreadIndex=%d", iThreadIndex);
		if(pCltSocketManage->GetIsShutDown())
		{
			break;
		}
		//没有任务了
		if(!pCltSocketManage->ProcessJob())
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

//测试用
int yClientImpl::StartTest()
{
	dzlog_info("yClientImpl StartTest!");
	HANDLE hThreadHandle = NULL;
	UINT uThreadID = 0;
	sThreadData threadData;
	threadData.hCompletionPort = NULL;
	threadData.hThreadEvent = m_hThreadEvent;
	threadData.hJobEvent = m_hTestEvent;
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
	HANDLE hTestEvent = pThreadData->hJobEvent;
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
		dzlog_debug("TestThreadProc get single! iThreadIndex=%d", iThreadIndex);
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
	dzlog_info("TestThreadProc exit! iThreadIndex=%d", iThreadIndex);
	SetEvent(hThreadEvent);
	return 0;
}

//事件回调函数
int yClientImpl::EventFunc(string strEventName, void *pFuncParam)
{
	if(ENEVT_NEW_JOB_ADD == strEventName)
	{
		SetEvent(*(HANDLE *)(pFuncParam));
	}
	return 1;
}