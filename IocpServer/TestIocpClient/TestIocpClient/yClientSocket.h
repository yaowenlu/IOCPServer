#ifndef Y_CLIENT_SOCKET_H
#define Y_CLIENT_SOCKET_H

#include <winsock2.h>
//zlog日志库
#include "zlog.h"

#include "afxwin.h"
#include <map>
#include <list>
#include "MsgDefine.h"

#define MAX_WORD_THREAD_NUMS 256	//最大工作线程数
//缓存定义
#define SED_SIZE				150000							//缓冲区大小
#define RCV_SIZE				100000							//缓冲区大小
#define TIME_OUT				3000
#define MAX_SEND_SIZE			4000


class CClientSocket;
class CClientManager;

//开启服务错误码定义
enum 
{
	CODE_SUCC = 0,						//成功
	CODE_SERVICE_WSA_ERROR,				//WSA失败
	CODE_SERVICE_PARAM_ERROR,			//参数错误
	CODE_SERVICE_CREATE_EVENT_ERROR,	//创建完成端口错误
	CODE_SERVICE_CREATE_IOCP_ERROR,		//创建完成端口错误
	CODE_SERVICE_WORK_FAILED,			//启动工作线程失败
	CODE_SERVICE_LISTEN_FAILED,			//监听失败
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
	HANDLE hCompletionPort;//完成端口
	HANDLE hThreadEvent;//线程事件
	SOCKET hLsSocket;//监听socket			
	CClientManager*	pSocketManage;//管理指针
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
	bool CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index);
	//关闭所有连接
	bool CloseAllConnection();
	//发送数据
	int SendData(unsigned __int64 i64Index, void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode);
	//
	bool GetIsShutDown(){return m_bShutDown;}
	void SetIsShutDown(bool bShutDown){m_bShutDown = bShutDown;return;}
private:

	std::map<unsigned __int64, CClientSocket*> m_mapClientConnect;
	std::list<CClientSocket *> m_lstFreeClientConn;

	bool m_bShutDown;//是否关闭服务
	int m_iClientNums;//客户端连接数
	unsigned __int64 m_i64UniqueIndex;//唯一索引

	//锁
	CRITICAL_SECTION m_csConnectLock;
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
	bool CloseSocket();
	inline void SetSocket(SOCKET hSocket){m_hSocket = hSocket;}
	inline SOCKET GetSocket(){return m_hSocket;}
	inline void SetIndex(unsigned __int64 i64Index){m_i64Index = i64Index;}
	inline unsigned __int64 GetIndex(){return m_i64Index;}
	void SetClientManager(CClientManager *pManager){m_pManage = pManager;}

	//开始接收数据
	bool OnRecvBegin();
	//接收完成函数
	bool OnRecvCompleted(DWORD dwRecvCount);

	//发送数据函数
	int SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode);
	//开始发送
	bool OnSendBegin();
	//发送完成
	bool OnSendCompleted(DWORD dwSendCount);

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
	struct sockaddr_in	m_SocketAddr;			//地址信息

	//内部数据
private:
	DWORD			m_dwSendBuffLen;	//发送缓冲区长度
	DWORD			m_dwRecvBuffLen;	//接收缓冲区长度
	sOverLapped		m_SendOverData;		//发送数据重叠结构
	sOverLapped		m_RecvOverData;		//接收数据重叠结构
};

class yClientSocket
{
public:
	yClientSocket();
	~yClientSocket();
	static unsigned __stdcall WorkThreadProc(LPVOID pThreadParam);
public:
	//连接服务器
	int ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum);
	//断开连接
	int DisConnectServer();
	//发送数据
	int SendData(void* pData, UINT uBufLen, BYTE bMainID, BYTE bAssistantID, BYTE bHandleCode);
	//开始工作
	int StartWork(unsigned short usThreadNum);

protected:

private:
	bool m_bWorking;
	int m_iWSAInitResult;
	unsigned short m_usThreadNum;
	HANDLE m_hThreadEvent;//批量发送事件
	HANDLE m_hCompletionPort;//完成端口
	CClientManager *m_pClientManager;
};

#endif