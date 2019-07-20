#ifndef Y_CLIENT_SOCKET_H
#define Y_CLIENT_SOCKET_H

#include <string>
#include <winsock2.h>
//zlog��־��
#include "zlog.h"

#include "afxwin.h"
#include <map>
#include <list>
#include "MsgDefine.h"

#define MAX_WORD_THREAD_NUMS 256	//������߳���
#define MAX_FREE_CLIENT_NUM	 1000	//������������
#define MAX_WAIT_JOB_COUNT	10000	//���ȴ��������������������ٴ���
//���涨��
#define SED_SIZE				40960			//���ͻ�������С
#define RCV_SIZE				40960			//���ջ�������С
#define TIME_OUT				3000			//��ʱʱ��
#define MAX_SEND_SIZE			4000			//������Ϣ��෢�͵����ݴ�С


class CClientSocket;
class CClientManager;
class yClientSocket;

//������������붨��
enum 
{
	CODE_SUCC = 0,						//�ɹ�
	CODE_SERVICE_WSA_ERROR,				//WSAʧ��
	CODE_SERVICE_PARAM_ERROR,			//��������
	CODE_SERVICE_CREATE_EVENT_ERROR,	//������ɶ˿ڴ���
	CODE_SERVICE_CREATE_IOCP_ERROR,		//������ɶ˿ڴ���
	CODE_SERVICE_WORK_FAILED,			//���������߳�ʧ��
	CODE_SERVICE_LISTEN_FAILED,			//����ʧ��
	CODE_SERVICE_UNKNOW_ERROR=100		//δ֪����
};

//���Ʊ�ʶ
enum
{
	SOCKET_SND = 1,//SOCKET ����
	SOCKET_SND_FINISH,//SOCKET �������
	SOCKET_REV,//SOCKET ����
	SOCKET_REV_FINISH,//SOCKET�������
	SOCKET_CLO,//SOCKET �˳�
};

//SOCKET �ص� IO �ṹ
struct sOverLapped		
{
	OVERLAPPED OverLapped;//�ص��ṹ
	WSABUF WSABuffer;//���ݻ���
	UINT uOperationType;//��������
	unsigned __int64 i64HandleID;//���� ID ����
};

//�߳����������ṹ
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//��ɶ˿�
	HANDLE hThreadEvent;//�߳��¼�
	HANDLE hJobEvent;//�߳��¼�
	SOCKET hLsSocket;//����socket			
	CClientManager*	pSocketManage;//����ָ��
	yClientSocket* pClientSocket;
};

//�����ṹ
struct sJobItem
{
	__int64 i64Index;//�ͻ�������
	DWORD dwBufLen;
	BYTE *pJobBuff;
};

//�ͻ��˹�����
class CClientManager
{
public:
	CClientManager();
	~CClientManager();

	//�յ�һ������
	CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//�ر�һ������
	/***************************
	�벻Ҫֱ�ӵ��ô˺�����Ҫ�ر�һ�����ӵ��ö�Ӧ�ͻ��˵�CloseSocket�������ɣ�
	��ɶ˿ڻ��Զ����ô˺����ͷ�����ռ�õ���Դ
	***************************/
	bool CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index);
	//�ͷ�������Դ
	void ReleaseOneConnection(unsigned __int64 i64Index);

	//�ر�ָ������������
	bool CloseConnection(DWORD dwNum);

	//�ر���������
	/*******************************
	�ڵ��ô˺���ǰ����Ҫ�ȹر�������ɶ˿ڣ�������ܵ��³������
	��Ϊ�˺������ͷſͻ����������ڴ棬�����ڴ�ᱻ��ɶ˿�ʹ��
	********************************/
	bool CloseAllConnection();

	//��������
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

	//��������
	bool AddJob(sJobItem *pJob);

	//��������
	bool ProcessJob();

private:
	std::map<unsigned __int64, CClientSocket*> m_mapClientConnect;
	std::list<CClientSocket *> m_lstFreeClientConn;
	std::list<sJobItem *> m_lstJobItem;

	bool m_bShutDown;//�Ƿ�رշ���
	int m_iClientNums;//�ͻ���������
	unsigned __int64 m_i64UniqueIndex;//Ψһ����

	//��
	CRITICAL_SECTION m_csConnectLock;
	CRITICAL_SECTION m_csJob;
public:
	//CRITICAL_SECTION m_csEvent;
};

//ÿ���ͻ���������Ϣ
class CClientSocket
{
public:
	CClientSocket();
	~CClientSocket();
	//��ʼ������
	void InitData();

	//�ر�����
	bool CloseSocket(bool bGraceful = true);

	inline void SetSocket(SOCKET hSocket){m_hSocket = hSocket;}
	inline SOCKET GetSocket(){return m_hSocket;}
	inline void SetIndex(unsigned __int64 i64Index){m_i64Index = i64Index;}
	inline unsigned __int64 GetIndex(){return m_i64Index;}
	void SetClientManager(CClientManager *pManager){m_pManage = pManager;}

	//��ʼ��������
	bool OnRecvBegin();

	//������ɺ���
	int OnRecvCompleted(DWORD dwRecvCount);

	//�������ݺ���
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//��ʼ����
	bool OnSendBegin();

	//�������
	bool OnSendCompleted(DWORD dwSendCount);

	//������Ϣ
	void HandleMsg(void *pMsgBuf, DWORD dwBufLen);

private:
	SOCKET m_hSocket;//���Ӷ�Ӧ��socket
	unsigned __int64 m_i64Index;//����ʶ��Ψһ�Ŀͻ�������
	CRITICAL_SECTION m_csRecvLock;
	CRITICAL_SECTION m_csSendLock;

protected:
	char				m_szSendBuf[SED_SIZE];	//�������ݻ�����
	char				m_szRecvBuf[RCV_SIZE];	//���ݽ��ջ�����
	long int			m_lBeginTime;			//����ʱ��
	CClientManager		*m_pManage;				//SOCKET ������ָ��

	//�ڲ�����
private:
	DWORD			m_dwSendBuffLen;	//���ͻ���������
	DWORD			m_dwRecvBuffLen;	//���ջ���������
	sOverLapped		m_SendOverData;		//���������ص��ṹ
	sOverLapped		m_RecvOverData;		//���������ص��ṹ
};

class yClientSocket
{
public:
	yClientSocket();
	~yClientSocket();

	//���ӷ�����
	int ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum);

	//�Ͽ�����
	int DisConnectServer();

	//��������
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

private:
	//����ָ�������Ŀͻ�������
	int ActiveConnect(DWORD dwConnectNum);

	//IO��ʼ����
	int StartIOWork(DWORD usThreadNum);

	//��ʼ������
	int StartJobWork(DWORD usThreadNum);

	//������
	int StartTest();

	//IO�߳�
	static unsigned __stdcall IOThreadProc(LPVOID pThreadParam);

	//�����߳�
	static unsigned __stdcall JobThreadProc(LPVOID pThreadParam);

	//�������߳�
	static unsigned __stdcall TestThreadProc(LPVOID pThreadParam);

private:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;
	DWORD m_dwJobThreadNum;
	HANDLE m_hThreadEvent;//���������¼�
	HANDLE m_hJobEvent;//���������¼�
	HANDLE m_hTestEvent;//���������¼�
	HANDLE m_hCompletionPort;//��ɶ˿�
	CClientManager *m_pClientManager;

	std::string m_strServerIp;//������ip
	unsigned short m_usServerPort;//�������˿�
};

#endif