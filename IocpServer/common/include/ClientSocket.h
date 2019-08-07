#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

#include <winsock2.h>
#include <list>

#include "MsgDefine.h"
#include "CommSocketStruct.h"
#include "JobManager.h"
#include "ProxyDefine.h"

//���涨��
#define SED_SIZE				40960			//���ͻ�������С
#define RCV_SIZE				40960			//���ջ�������С
#define TIME_OUT				3000			//��ʱʱ��
#define MAX_SEND_SIZE			4000			//������Ϣ��෢�͵����ݴ�С
#define MAX_SEND_BUFF_COUNT		10000			//��໺�����������������

struct sSendBuff
{
	char* pBuff;
	USHORT usBuffLen;
};

class CSocketManager;
//ÿ���ͻ���������Ϣ
class CClientSocket
{
public:
	CClientSocket();
	virtual ~CClientSocket();

	//��ʼ������
	virtual void InitData();

	//�ͷŷ����ڴ�
	void ReleaseSendBuf();

	//���ӷ��ͻ���
	bool AddSendBuf(sProxyHead proxyHead, NetMsgHead msgHead, void* pBuff, DWORD dwLen);

	//���ӷ��ͻ���
	bool AddSendBuf(NetMsgHead msgHead, void* pBuff, DWORD dwLen);

	//���ӷ��ͻ���
	bool AddSendBuf(void* pBuff, DWORD dwLen);

	//�ر�����
	bool CloseSocket(bool bGraceful = false);

	//���ù�����
	void SetManager(CSocketManager* pManager){ m_pManager = pManager; }

	inline void SetSocket(SOCKET hSocket){m_hSocket = hSocket;}
	inline SOCKET GetSocket(){return m_hSocket;}

	inline void SetIndex(unsigned __int64 i64Index){m_i64Index = i64Index;}
	inline unsigned __int64 GetIndex(){return m_i64Index;}

	DWORD GetTimeOutCount(){return m_dwTimeOutCount;}
	//����������
	void SetSrvType(enSrvType iSrvType) { m_iSrvType = iSrvType; }
	enSrvType GetSrvType() { return m_iSrvType; }

	void SetSrvID(USHORT usSrvID) {	m_usSrvID = usSrvID;}
	USHORT GetSrvID() {	return m_usSrvID;}

	void SetIsAsClinet(bool bAsClient) { m_bIsAsClient = bAsClient; }
	bool GetIsAsClinet() { return m_bIsAsClient; }

	//��ʼ��������
	bool OnRecvBegin();

	//������ɺ���
	bool OnRecvCompleted(DWORD dwRecvCount, std::list<sJobItem *> &lstJobs);

	//�������ݺ���
	int SendData(void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//���ʹ�������
	int SendProxyMsg(void *pMsgBuf, DWORD dwBufLen);

	//���ʹ�������
	int SendProxyMsg(sProxyHead proxyHead, void* pData, DWORD dwDataLen, DWORD dwMainID, DWORD dwAssID, DWORD dwHandleCode);

	//��ʼ����
	bool OnSendBegin();

	//�������
	bool OnSendCompleted(DWORD dwSendCount);

	//������Ϣ
	bool HandleMsg(void *pMsgBuf, DWORD dwBufLen);

	//������ͨ��Ϣ
	virtual bool HandleNormalMsg(void *pMsgBuf, DWORD dwBufLen);

	//���������Ϣ
	virtual bool HandleProxyMsg(void *pMsgBuf, DWORD dwBufLen);

	//���÷��������
	virtual void SetSrvIndex(unsigned __int64 i64SrvIndex){ return; }

	//��ȡ���������
	virtual unsigned __int64 GetSrvIndex(){return 0;}

private:
	CSocketManager* m_pManager;
	SOCKET m_hSocket;//���Ӷ�Ӧ��socket
	unsigned __int64 m_i64Index;//����ʶ��Ψһ������
	CRITICAL_SECTION m_csRecvLock;
	CRITICAL_SECTION m_csSendLock;
	CRITICAL_SECTION m_csStateLock;

	char			m_szSendBuf[SED_SIZE];	//�������ݻ�����
	char			m_szRecvBuf[RCV_SIZE];	//���ݽ��ջ�����
	std::list<sSendBuff> m_lstSendBuff;		//�����ͻ�������

	DWORD			m_dwTimeOutCount;	//��ʱ����
	bool			m_bSending;			//�����Ƿ����ڷ���
	CJobManager		m_jobManager;
public:
	DWORD			m_dwSendBuffLen;	//���ͻ���������
	DWORD			m_dwRecvBuffLen;	//���ջ���������
	sOverLapped		m_SendOverData;		//���������ص��ṹ
	sOverLapped		m_RecvOverData;		//���������ص��ṹ
private:
	enSrvType		m_iSrvType;		//�Լ�������
	USHORT			m_usSrvID;		//�Լ���ID
	bool			m_bIsAsClient;	//��ʶ��socket�ǲ�����Ϊ�ͻ������ӵ�����������
};

#endif