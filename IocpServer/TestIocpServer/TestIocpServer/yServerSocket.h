#ifndef Y_SERVER_SOCKET_H
#define Y_SERVER_SOCKET_H

#include <winsock2.h>
//zlog日志库
#include "zlog.h"

#include "afxwin.h"
#include <map>
#include <list>
#include "MsgDefine.h"

#define MAX_WORD_THREAD_NUMS 256	//最大工作线程数
#define MAX_FREE_CLIENT_NUM	 1000	//最大空闲连接数
#define MAX_WAIT_JOB_COUNT	10000	//最大等待工作数，超出将不会再处理
//缓存定义
#define SED_SIZE				40960			//发送缓冲区大小
#define RCV_SIZE				40960			//接收缓冲区大小
#define TIME_OUT				3000			//超时时间
#define MAX_SEND_SIZE			4000			//单条消息最多发送的数据大小

#define MAX_CONNECT_TIME_OUT_COUNT		5				//超时次数，超过此次数时会强制关闭并释放连接
//定时器ID定义
#define TIMER_ID_KEEP_ALIVE				100				//心跳定时器
#define TIMER_ID_CHECK_CLOSE_SOCKET		101				//检测客户端

//定时时间间隔
#define TIME_DEALY_KEEP_ALIVE			10000			//心跳检测间隔

class CClientSocket;
class CClientManager;

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

//控制标识
enum
{
	SOCKET_SND = 1,//SOCKET 发送
	SOCKET_SND_FINISH,//SOCKET 发送完成
	SOCKET_REV,//SOCKET 接收
	SOCKET_REV_FINISH,//SOCKET接收完成
	SOCKET_CLO,//SOCKET 退出
};

//SOCKET 重叠 IO 结构
struct sOverLapped		
{
	OVERLAPPED OverLapped;//重叠结构
	WSABUF WSABuffer;//数据缓冲
	UINT uOperationType;//操作类型
	unsigned __int64 i64HandleID;//处理 ID 号码
};

//线程启动参数结构
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//完成端口
	HANDLE hThreadEvent;//线程事件
	HANDLE hJobEvent;//线程事件
	SOCKET hLsSocket;//监听socket			
	CClientManager*	pSocketManage;//管理指针
};

//工作结构
struct sJobItem
{
	__int64 i64Index;//客户端索引
	DWORD dwBufLen;
	BYTE *pJobBuff;
};

//定时器结构
struct sTimer
{
	DWORD dwTimerID;//定时器ID
	int iTimerCount;//命中次数
	DWORD dwBeginTime;//定时器起始时间
	DWORD dwDelayTime;//定时器延时时间
	DWORD dwEnterTime;//定时器命中时间
	sTimer()
	{
		memset(this, 0, sizeof(*this));
	}
};

//客户端管理类
class CClientManager
{
public:
	CClientManager();
	~CClientManager();

	//收到一个连接
	CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//关闭一个连接
	/***************************
	请不要直接调用此函数，要关闭一个连接调用对应客户端的CloseSocket函数即可，
	完成端口会自动调用此函数释放连接占用的资源
	***************************/
	bool CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index);
	//释放连接资源
	void ReleaseOneConnection(unsigned __int64 i64Index, bool bErase = true);

	//关闭所有连接
	/*******************************
	在调用此函数前必须要先关闭所有完成端口，否则可能导致程序崩溃
	因为此函数会释放客户端所属的内存，而此内存会被完成端口使用
	********************************/
	bool CloseAllConnection();

	//发送数据
	int SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	bool GetIsShutDown()
	{
		return m_bShutDown;
	}
	void SetIsShutDown(bool bShutDown)
	{
		m_bShutDown = bShutDown;
		return;
	}

	//增加任务
	bool AddJob(sJobItem *pJob);

	//处理任务
	bool ProcessJob();

	//定时器
	void OnBaseTimer();

	//设置定时器
	int SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount);

	//触发定时器
	int OnTimer(DWORD dwTimerID);

	//删除定时器
	int KillTimer(DWORD dwTimerID);

private:	
	std::map<unsigned __int64, CClientSocket*> m_mapClientConnect;
	std::list<CClientSocket *> m_lstFreeClientConn;
	std::list<sJobItem *> m_lstJobItem;
	std::map<UINT, sTimer> m_mapAllTimer;

	bool m_bShutDown;//是否关闭服务
	int m_iClientNums;//客户端连接数
	unsigned __int64 m_i64UniqueIndex;//唯一索引

	//锁
	CRITICAL_SECTION m_csConnectLock;
	CRITICAL_SECTION m_csJob;
	CRITICAL_SECTION m_csTimer;
public:
	//CRITICAL_SECTION m_csEvent;
};

//每个客户端连接信息
class CClientSocket
{
public:
	CClientSocket();
	~CClientSocket();
	//初始化数据
	void InitData();

	//关闭连接
	bool CloseSocket(bool bGraceful = true);

	inline void SetSocket(SOCKET hSocket){m_hSocket = hSocket;}
	inline SOCKET GetSocket(){return m_hSocket;}
	inline void SetIndex(unsigned __int64 i64Index){m_i64Index = i64Index;}
	inline unsigned __int64 GetIndex(){return m_i64Index;}
	void SetClientManager(CClientManager *pManager){m_pManage = pManager;}
	DWORD GetTimeOutCount(){return m_dwTimeOutCount;}

	//开始接收数据
	bool OnRecvBegin();

	//接收完成函数
	int OnRecvCompleted(DWORD dwRecvCount);

	//发送数据函数
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//开始发送
	bool OnSendBegin();

	//发送完成
	bool OnSendCompleted(DWORD dwSendCount);

	//处理消息
	void HandleMsg(void *pMsgBuf, DWORD dwBufLen);

private:
	SOCKET m_hSocket;//连接对应的socket
	unsigned __int64 m_i64Index;//用来识别唯一的客户端连接
	CRITICAL_SECTION m_csRecvLock;
	CRITICAL_SECTION m_csSendLock;

protected:
	char				m_szSendBuf[SED_SIZE];	//发送数据缓冲区
	char				m_szRecvBuf[RCV_SIZE];	//数据接收缓冲区
	long int			m_lBeginTime;			//连接时间
	CClientManager		*m_pManage;				//SOCKET 管理类指针

	//内部数据
private:
	DWORD			m_dwSendBuffLen;	//发送缓冲区长度
	DWORD			m_dwRecvBuffLen;	//接收缓冲区长度
	sOverLapped		m_SendOverData;		//发送数据重叠结构
	sOverLapped		m_RecvOverData;		//接收数据重叠结构
	DWORD			m_dwTimeOutCount;	//超时次数
};

class yServerSocket
{
public:
	yServerSocket();
	~yServerSocket();
	//开始服务
	int StartService(int iPort, DWORD dwIoThreadNum, DWORD dwJobThreadNum);

	//停止服务
	int StopService();
private:
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

private:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;
	DWORD m_dwJobThreadNum;
	HANDLE m_hThreadEvent;//线程发送事件
	HANDLE m_hJobEvent;//job发送事件
	HANDLE m_hTimerEvent;//job发送事件
	HANDLE m_hCompletionPort;//完成端口
	HANDLE m_hListenThread;//监听线程句柄
	SOCKET m_lsSocket;//监听socket
	CClientManager *m_pClientManager;
};

#endif