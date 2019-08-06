#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include <map>
#include <list>
#include "ClientSocket.h"
using namespace std;

#define MAX_FREE_CLIENT_NUM				1000		//最大空闲连接数
#define MAX_WAIT_JOB_COUNT				10000		//最大等待JOB数，超出将不会再处理

#define MAX_CONNECT_TIME_OUT_COUNT		5			//超时次数，超过此次数时会强制关闭并释放连接
//定时器ID定义
#define TIMER_ID_KEEP_ALIVE				100			//心跳定时器

//定时时间间隔
#define TIME_DEALY_KEEP_ALIVE			10000		//心跳检测间隔


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
class CSocketManager
{
public:
	CSocketManager();
	virtual ~CSocketManager();

	//处理IO消息
	virtual bool ProcessIOMessage(CClientSocket *pClient, sOverLapped *pOverLapped, DWORD dwIOSize);

	//收到一个连接
	virtual CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//关闭一个连接
	virtual bool CloseOneConnection(CClientSocket *pClient);

	//释放连接资源
	virtual void ReleaseOneConnection(CClientSocket *pClient);

	//关闭所有连接
	virtual bool CloseAllConnection();

	//发送数据
	virtual int SendData(unsigned __int64 i64Index, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

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
	virtual bool AddOneJob(sJobItem *pJob);

	//定时器响应
	virtual void OnBaseTimer();

	//设置定时器
	virtual int SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount);

	//触发定时器
	virtual int OnTimer(DWORD dwTimerID);

	//删除定时器
	virtual int KillTimer(DWORD dwTimerID);

	//增加心跳Job
	virtual void AddHeartJob();

protected:
	//处理任务
	virtual bool ProcessJob() = 0;

protected:
	std::map<unsigned __int64, CClientSocket*> m_mapClientConnect;
	std::list<CClientSocket *> m_lstFreeClientConn;
	std::list<sJobItem *> m_lstJobItem;
	std::map<UINT, sTimer> m_mapAllTimer;

	bool m_bShutDown;//是否关闭服务
	int m_iClientNums;//客户端连接数
	unsigned __int64 m_i64UniqueIndex;//唯一索引
	CJobManager m_jobManager;

	//锁资源
	CRITICAL_SECTION m_csActiveConnectLock;
	CRITICAL_SECTION m_csFreeConnectLock;
	CRITICAL_SECTION m_csJobLock;
	CRITICAL_SECTION m_csTimerLock;
};

#endif