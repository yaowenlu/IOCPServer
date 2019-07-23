#ifndef SOCKET_MANAGER_H
#define SOCKET_MANAGER_H

#include "afxwin.h"
#include <map>
#include <list>
#include "ClientSocket.h"
using namespace std;

#define MAX_FREE_CLIENT_NUM				1000		//������������
#define MAX_WAIT_JOB_COUNT				10000		//���ȴ�JOB���������������ٴ���

#define MAX_CONNECT_TIME_OUT_COUNT		5			//��ʱ�����������˴���ʱ��ǿ�ƹرղ��ͷ�����
//��ʱ��ID����
#define TIMER_ID_KEEP_ALIVE				100			//������ʱ��

//��ʱʱ����
#define TIME_DEALY_KEEP_ALIVE			10000		//���������


//��ʱ���ṹ
struct sTimer
{
	DWORD dwTimerID;//��ʱ��ID
	int iTimerCount;//���д���
	DWORD dwBeginTime;//��ʱ����ʼʱ��
	DWORD dwDelayTime;//��ʱ����ʱʱ��
	DWORD dwEnterTime;//��ʱ������ʱ��
	sTimer()
	{
		memset(this, 0, sizeof(*this));
	}
};

//�ͻ��˹�����
class CSocketManager
{
public:
	CSocketManager();
	virtual ~CSocketManager();

	//����IO��Ϣ
	virtual bool ProcessIOMessage(CClientSocket *pClient, sOverLapped *pOverLapped, DWORD dwIOSize);

	//�յ�һ������
	virtual CClientSocket* ActiveOneConnection(SOCKET hSocket);

	//�ر�һ������
	/***************************
	�벻Ҫֱ�ӵ��ô˺�����Ҫ�ر�һ�����ӵ��ö�Ӧ�ͻ��˵�CloseSocket�������ɣ�
	��ɶ˿ڻ��Զ����ô˺����ͷ�����ռ�õ���Դ
	***************************/
	virtual bool CloseOneConnection(CClientSocket *pClient, unsigned __int64 i64Index = 0);
	//�ͷ�������Դ
	void ReleaseOneConnection(unsigned __int64 i64Index, bool bErase = true);

	//�ر���������
	/*******************************
	�ڵ��ô˺���ǰ����Ҫ�ȹر�������ɶ˿ڣ�������ܵ��³������
	��Ϊ�˺������ͷſͻ����������ڴ棬�����ڴ�ᱻ��ɶ˿�ʹ��
	********************************/
	virtual bool CloseAllConnection();

	//��������
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

	//��������
	virtual bool AddJob(sJobItem *pJob);

	//��������
	virtual bool ProcessJob();

	//��ʱ����Ӧ
	virtual void OnBaseTimer();

	//���ö�ʱ��
	virtual int SetTimer(DWORD dwTimerID, DWORD dwDelayTime, int iTimerCount);

	//������ʱ��
	virtual int OnTimer(DWORD dwTimerID);

	//ɾ����ʱ��
	virtual int KillTimer(DWORD dwTimerID);

	//��������Job
	virtual void AddHeartJob();

protected:
	std::map<unsigned __int64, CClientSocket*> m_mapClientConnect;
	std::list<CClientSocket *> m_lstFreeClientConn;
	std::list<sJobItem *> m_lstJobItem;
	std::map<UINT, sTimer> m_mapAllTimer;

	bool m_bShutDown;//�Ƿ�رշ���
	int m_iClientNums;//�ͻ���������
	unsigned __int64 m_i64UniqueIndex;//Ψһ����
	CJobManager m_jobManager;

	//����Դ
	CRITICAL_SECTION m_csConnectLock;
	CRITICAL_SECTION m_csJobLock;
	CRITICAL_SECTION m_csTimerLock;
};

#endif