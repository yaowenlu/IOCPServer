#ifndef Y_SERVER_IMPL_H
#define Y_SERVER_IMPL_H

#include "ClientSocket.h"
#include "SocketManager.h"

#define MAX_THREAD_NUMS			256		//������߳���

class CSrvSocketManager;
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
	CODE_SERVICE_TIMER_FAILED,			//������ʱ���߳�ʧ��
	CODE_SERVICE_UNKNOW_ERROR=100		//δ֪����
};

//�߳����������ṹ
struct sThreadData	
{
	int iThreadIndex;
	HANDLE hCompletionPort;//��ɶ˿�
	HANDLE hThreadEvent;//�߳��¼�
	HANDLE hJobEvent;//�߳��¼�
	SOCKET hLsSocket;//����socket			
	CSrvSocketManager*	pSrvSocketManage;//������ָ��
};

//�����Socket������
class CSrvSocketManager:public CSocketManager
{
public:
	CSrvSocketManager(){};
	~CSrvSocketManager(){};
};

class yServerImpl
{
public:
	yServerImpl();
	~yServerImpl();
	//��ʼ����
	int StartService(int iPort, DWORD dwIoThreadNum, DWORD dwJobThreadNum);

	//ֹͣ����
	int StopService();

protected:
	//IO��ʼ����
	int StartIOWork(DWORD dwThreadNum);

	//��ʼ������
	int StartJobWork(DWORD dwThreadNum);

	//��ʼ����
	int StartListen(int iPort);

	//��ʼ��ʱ��
	int StartTimer();

	//�����߳�
	static unsigned __stdcall ListenThreadProc(LPVOID pThreadParam);

	//IO�߳�
	static unsigned __stdcall IOThreadProc(LPVOID pThreadParam);

	//�����߳�
	static unsigned __stdcall JobThreadProc(LPVOID pThreadParam);

	//��ʱ���߳�
	static unsigned __stdcall TimerThreadProc(LPVOID pThreadParam);

	//�¼��ص�����
	static int EventFunc(string strEventName, void *pFuncParam);

protected:
	bool m_bWorking;
	int m_iWSAInitResult;
	DWORD m_dwIoThreadNum;//io�߳���
	DWORD m_dwJobThreadNum;//�����߳���
	HANDLE m_hThreadEvent;//�̷߳����¼�
	HANDLE m_hJobEvent;//job�����¼�
	HANDLE m_hTimerEvent;//job�����¼�
	HANDLE m_hCompletionPort;//��ɶ˿�
	HANDLE m_hListenThread;//�����߳̾��
	SOCKET m_lsSocket;//����socket
	CSrvSocketManager *m_pSrvSocketManager;
};

#endif