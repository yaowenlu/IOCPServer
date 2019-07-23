#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <winsock2.h>
#include <list>
//zlog��־��
#include "zlog.h"

#include "MsgDefine.h"
#include "CommSocketStruct.h"
#include "JobManager.h"

//���涨��
#define SED_SIZE				40960			//���ͻ�������С
#define RCV_SIZE				40960			//���ջ�������С
#define TIME_OUT				3000			//��ʱʱ��
#define MAX_SEND_SIZE			4000			//������Ϣ��෢�͵����ݴ�С


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
	DWORD GetTimeOutCount(){return m_dwTimeOutCount;}

	//��ʼ��������
	bool OnRecvBegin();

	//������ɺ���
	bool OnRecvCompleted(DWORD dwRecvCount, std::list<sJobItem *> &lstJobs);

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
	CRITICAL_SECTION m_csStateLock;

protected:
	char				m_szSendBuf[SED_SIZE];	//�������ݻ�����
	char				m_szRecvBuf[RCV_SIZE];	//���ݽ��ջ�����
	long int			m_lBeginTime;			//����ʱ��

	//�ڲ�����
private:
	DWORD			m_dwSendBuffLen;	//���ͻ���������
	DWORD			m_dwRecvBuffLen;	//���ջ���������
	sOverLapped		m_SendOverData;		//���������ص��ṹ
	sOverLapped		m_RecvOverData;		//���������ص��ṹ
	DWORD			m_dwTimeOutCount;	//��ʱ����
	bool			m_bSending;			//�����Ƿ����ڷ���
	CJobManager		m_jobManager;
};

#endif