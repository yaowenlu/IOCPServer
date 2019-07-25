#ifndef Y_SERVER_IMPL_H
#define Y_SERVER_IMPL_H

#include "ClientSocket.h"
#include "SocketManager.h"

#define MAX_THREAD_NUMS			256		//最大工作线程数

class CSrvSocketManager;
//开启服务错误码定义
enum 
{
	CODE_SUCC = 0,						//成功
	CODE_SERVICE_WORKING,				//服务已经启动了
	CODE_SERVICE_WSA_ERROR,				//WSA失败
	CODE_SERVICE_PARAM_ERROR,			//参数错误
	CODE_SERVICE_CREATE_EVENT_ERROR,	//创建完成端口错误
	CODE_SERVICE_CREATE_IOCP_ERROR,		//创建完成端口错误
	CODE_SERVICE_IO_FAILED,				//启动IO线程失败
	CODE_SERVICE_WORK_FAILED,			//启动工作线程失败
	CODE_SERVICE_LISTEN_FAILED,			//监听失败
	CODE_SERVICE_TIMER_FAILED,			//启动定时器线程失败
	CODE_SERVICE_UNKNOW_ERROR=100		//未知错误
};

//线程启动参数结构
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//完成端口
	HANDLE hThreadEvent;//线程事件
	SOCKET hLsSocket;//监听socket			
	CSrvSocketManager*	pSrvSocketManage;//管理类指针
};

class CSrvClientSocket:public CClientSocket
{
public:
	CSrvClientSocket();
	~CSrvClientSocket();

	//处理消息
	virtual bool HandleMsg(void *pMsgBuf, DWORD dwBufLen);
};

//服务端Socket管理类
class CSrvSocketManager:public CSocketManager
{
public:
	CSrvSocketManager();
	~CSrvSocketManager();

	//收到一个连接
	virtual CClientSocket* ActiveOneConnection(SOCKET hSocket);
};

class yServerImpl
{
public:
	yServerImpl();
	~yServerImpl();
	//开始服务
	int StartService(int iPort, DWORD dwIoThreadNum, DWORD dwJobThreadNum);

	//停止服务
	int StopService();

protected:
	//IO开始工作
	int StartIOWork(DWORD dwThreadNum);

	//开始任务处理
	int StartJobWork(DWORD dwThreadNum);

	//开始监听
	int StartListen(int iPort);

	//开始定时器
	int StartTimer();

	//监听线程
	static unsigned __stdcall ListenThreadProc(LPVOID pThreadParam);

	//IO线程
	static unsigned __stdcall IOThreadProc(LPVOID pThreadParam);

	//工作线程
	static unsigned __stdcall JobThreadProc(LPVOID pThreadParam);

	//定时器线程
	static unsigned __stdcall TimerThreadProc(LPVOID pThreadParam);

	//事件回调函数
	static int EventFunc(string strEventName, void *pFuncParam);

protected:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;//io线程数
	DWORD m_dwJobThreadNum;//工作线程数
	HANDLE m_hThreadEvent;//线程发送事件
	HANDLE m_hIoCompletionPort;//IO完成端口
	HANDLE m_hJobCompletionPort;//Job完成端口
	HANDLE m_hListenThread;//监听线程句柄
	SOCKET m_lsSocket;//监听socket
	CSrvSocketManager *m_pSrvSocketManager;
};

#endif