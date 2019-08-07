#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <winsock2.h>
#include <list>

#include "MsgDefine.h"
#include "CommSocketStruct.h"
#include "JobManager.h"
#include "ProxyDefine.h"

//缓存定义
#define SED_SIZE				40960			//发送缓冲区大小
#define RCV_SIZE				40960			//接收缓冲区大小
#define TIME_OUT				3000			//超时时间
#define MAX_SEND_SIZE			4000			//单条消息最多发送的数据大小
#define MAX_SEND_BUFF_COUNT		10000			//最多缓存多少条待发送数据

struct sSendBuff
{
	char* pBuff;
	USHORT usBuffLen;
};

class CSocketManager;
//每个客户端连接信息
class CClientSocket
{
public:
	CClientSocket();
	virtual ~CClientSocket();

	//初始化数据
	virtual void InitData();

	//释放发送内存
	void ReleaseSendBuf();

	//增加发送缓存
	bool AddSendBuf(sProxyHead proxyHead, NetMsgHead msgHead, void* pBuff, DWORD dwLen);

	//增加发送缓存
	bool AddSendBuf(NetMsgHead msgHead, void* pBuff, DWORD dwLen);

	//增加发送缓存
	bool AddSendBuf(void* pBuff, DWORD dwLen);

	//关闭连接
	bool CloseSocket(bool bGraceful = false);

	//设置管理类
	void SetManager(CSocketManager* pManager){ m_pManager = pManager; }

	inline void SetSocket(SOCKET hSocket){m_hSocket = hSocket;}
	inline SOCKET GetSocket(){return m_hSocket;}

	inline void SetIndex(unsigned __int64 i64Index){m_i64Index = i64Index;}
	inline unsigned __int64 GetIndex(){return m_i64Index;}

	DWORD GetTimeOutCount(){return m_dwTimeOutCount;}
	//代理服务相关
	void SetSrvType(enSrvType iSrvType) { m_iSrvType = iSrvType; }
	enSrvType GetSrvType() { return m_iSrvType; }

	void SetSrvID(USHORT usSrvID) {	m_usSrvID = usSrvID;}
	USHORT GetSrvID() {	return m_usSrvID;}

	void SetIsAsClinet(bool bAsClient) { m_bIsAsClient = bAsClient; }
	bool GetIsAsClinet() { return m_bIsAsClient; }

	//开始接收数据
	bool OnRecvBegin();

	//接收完成函数
	bool OnRecvCompleted(DWORD dwRecvCount, std::list<sJobItem *> &lstJobs);

	//发送数据函数
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//发送代理数据
	int SendProxyMsg(void *pMsgBuf, DWORD dwBufLen);

	//发送代理数据
	int SendProxyMsg(sProxyHead proxyHead, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//开始发送
	bool OnSendBegin();

	//发送完成
	bool OnSendCompleted(DWORD dwSendCount);

	//处理消息
	bool HandleMsg(void *pMsgBuf, DWORD dwBufLen);

	//处理普通消息
	virtual bool HandleNormalMsg(void *pMsgBuf, DWORD dwBufLen);

	//处理代理消息
	virtual bool HandleProxyMsg(void *pMsgBuf, DWORD dwBufLen);

	//设置服务端索引
	virtual void SetSrvIndex(unsigned __int64 i64SrvIndex){ return; }

	//获取服务端索引
	virtual unsigned __int64 GetSrvIndex(){return 0;}

private:
	CSocketManager* m_pManager;
	SOCKET m_hSocket;//连接对应的socket
	unsigned __int64 m_i64Index;//用来识别唯一的连接
	CRITICAL_SECTION m_csRecvLock;
	CRITICAL_SECTION m_csSendLock;
	CRITICAL_SECTION m_csStateLock;

	char			m_szSendBuf[SED_SIZE];	//发送数据缓冲区
	char			m_szRecvBuf[RCV_SIZE];	//数据接收缓冲区
	std::list<sSendBuff> m_lstSendBuff;		//待发送缓冲数据

	DWORD			m_dwTimeOutCount;	//超时次数
	bool			m_bSending;			//数据是否正在发送
	CJobManager		m_jobManager;
public:
	DWORD			m_dwSendBuffLen;	//发送缓冲区长度
	DWORD			m_dwRecvBuffLen;	//接收缓冲区长度
	sOverLapped		m_SendOverData;		//发送数据重叠结构
	sOverLapped		m_RecvOverData;		//接收数据重叠结构
private:
	enSrvType		m_iSrvType;		//自己的类型
	USHORT			m_usSrvID;		//自己的ID
	bool			m_bIsAsClient;	//标识此socket是不是作为客户端连接到其他服务器
};

#endif