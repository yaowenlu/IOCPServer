#ifndef Y_CLIENT_IMPL_H
#define Y_CLIENT_IMPL_H

#include "ClientSocket.h"
#include "SocketManager.h"

#define MAX_WORD_THREAD_NUMS 256	//最大工作线程数

class CCltSocketManager;
class yClientImpl;

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
	CODE_SERVICE_UNKNOW_ERROR=100		//未知错误
};

//线程启动参数结构
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//完成端口
	HANDLE hThreadEvent;//线程事件
	HANDLE hJobEvent;//线程事件			
	CCltSocketManager* pCltSocketManage;//管理类指针
	yClientImpl* pClientSocket;
};

class CCltClientSocket:public CClientSocket
{
public:
	CCltClientSocket();
	~CCltClientSocket();

	//处理消息
	virtual void HandleMsg(void *pMsgBuf, DWORD dwBufLen);
	//设置服务端索引
	void SetSrvIndex(unsigned __int64 i64SrvIndex){m_i64SrvIndex = i64SrvIndex;}
	//获取服务端索引
	unsigned __int64 GetSrvIndex(){return m_i64SrvIndex;}
private:
	unsigned __int64 m_i64SrvIndex;
};

//客户端管理类
class CCltSocketManager:public CSocketManager
{
public:
	CCltSocketManager();
	~CCltSocketManager();

	//收到一个连接
	virtual CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//关闭指定数量的连接
	bool CloseConnection(DWORD dwNum);
};

class yClientImpl
{
public:
	yClientImpl();
	~yClientImpl();

	//连接服务器
	int ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum);

	//断开连接
	int DisConnectServer();

	//发送数据
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);
	
	//获取工作事件句柄
	HANDLE GetJobEvent(){return m_hJobEvent;}

private:
	//激活指定数量的客户端连接
	int ActiveConnect(DWORD dwConnectNum);

	//IO开始工作
	int StartIOWork(DWORD dwThreadNum);

	//开始任务处理
	int StartJobWork(DWORD dwThreadNum);

	//测试用
	int StartTest();

	//IO线程
	static unsigned __stdcall IOThreadProc(LPVOID pThreadParam);

	//工作线程
	static unsigned __stdcall JobThreadProc(LPVOID pThreadParam);

	//测试用线程
	static unsigned __stdcall TestThreadProc(LPVOID pThreadParam);

	//事件回调函数
	static int EventFunc(string strEventName, void *pFuncParam);

private:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;
	DWORD m_dwJobThreadNum;
	HANDLE m_hThreadEvent;//批量发送事件
	HANDLE m_hJobEvent;//批量发送事件
	HANDLE m_hTestEvent;//批量发送事件
	HANDLE m_hCompletionPort;//完成端口
	CCltSocketManager *m_pCltSocketManage;

	std::string m_strServerIp;//服务器ip
	unsigned short m_usServerPort;//服务器端口
};

#endif