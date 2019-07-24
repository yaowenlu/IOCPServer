#ifndef Y_CLIENT_IMPL_H
#define Y_CLIENT_IMPL_H

#include "ClientSocket.h"
#include "SocketManager.h"

#define MAX_WORD_THREAD_NUMS 256	//������߳���

class CCltSocketManager;
class yClientImpl;

//������������붨��
enum 
{
	CODE_SUCC = 0,						//�ɹ�
	CODE_SERVICE_WORKING,				//�����Ѿ�������
	CODE_SERVICE_WSA_ERROR,				//WSAʧ��
	CODE_SERVICE_PARAM_ERROR,			//��������
	CODE_SERVICE_CREATE_EVENT_ERROR,	//������ɶ˿ڴ���
	CODE_SERVICE_CREATE_IOCP_ERROR,		//������ɶ˿ڴ���
	CODE_SERVICE_IO_FAILED,				//����IO�߳�ʧ��
	CODE_SERVICE_WORK_FAILED,			//���������߳�ʧ��
	CODE_SERVICE_LISTEN_FAILED,			//����ʧ��
	CODE_SERVICE_UNKNOW_ERROR=100		//δ֪����
};

//�߳����������ṹ
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//��ɶ˿�
	HANDLE hThreadEvent;//�߳��¼�
	HANDLE hJobEvent;//�߳��¼�			
	CCltSocketManager* pCltSocketManage;//������ָ��
	yClientImpl* pClientSocket;
};

class CCltClientSocket:public CClientSocket
{
public:
	CCltClientSocket();
	~CCltClientSocket();

	//������Ϣ
	virtual void HandleMsg(void *pMsgBuf, DWORD dwBufLen);
	//���÷��������
	void SetSrvIndex(unsigned __int64 i64SrvIndex){m_i64SrvIndex = i64SrvIndex;}
	//��ȡ���������
	unsigned __int64 GetSrvIndex(){return m_i64SrvIndex;}
private:
	unsigned __int64 m_i64SrvIndex;
};

//�ͻ��˹�����
class CCltSocketManager:public CSocketManager
{
public:
	CCltSocketManager();
	~CCltSocketManager();

	//�յ�һ������
	virtual CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//�ر�ָ������������
	bool CloseConnection(DWORD dwNum);
};

class yClientImpl
{
public:
	yClientImpl();
	~yClientImpl();

	//���ӷ�����
	int ConnectServer(std::string strIp, unsigned short usPort, unsigned short usConnectNum);

	//�Ͽ�����
	int DisConnectServer();

	//��������
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);
	
	//��ȡ�����¼����
	HANDLE GetJobEvent(){return m_hJobEvent;}

private:
	//����ָ�������Ŀͻ�������
	int ActiveConnect(DWORD dwConnectNum);

	//IO��ʼ����
	int StartIOWork(DWORD dwThreadNum);

	//��ʼ������
	int StartJobWork(DWORD dwThreadNum);

	//������
	int StartTest();

	//IO�߳�
	static unsigned __stdcall IOThreadProc(LPVOID pThreadParam);

	//�����߳�
	static unsigned __stdcall JobThreadProc(LPVOID pThreadParam);

	//�������߳�
	static unsigned __stdcall TestThreadProc(LPVOID pThreadParam);

	//�¼��ص�����
	static int EventFunc(string strEventName, void *pFuncParam);

private:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;
	DWORD m_dwJobThreadNum;
	HANDLE m_hThreadEvent;//���������¼�
	HANDLE m_hJobEvent;//���������¼�
	HANDLE m_hTestEvent;//���������¼�
	HANDLE m_hCompletionPort;//��ɶ˿�
	CCltSocketManager *m_pCltSocketManage;

	std::string m_strServerIp;//������ip
	unsigned short m_usServerPort;//�������˿�
};

#endif