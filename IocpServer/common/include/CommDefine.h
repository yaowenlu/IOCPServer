
#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

/**************************************
ͨ�ö���
**************************************/

//ͷ����
enum enHeadType
{
	INVALID_HEAD = -1,	//��Ч��Ϣͷ
	MSG_HEAD = 0,		//������Ϣͷ
	PROXY_HEAD = 1,		//������Ϣͷ
};

//ת������
enum enTransType
{
	trans_p2p = 0,	//��Ե�
	trans_p2g = 1,	//�㵽��
};

//�������Ͷ���
enum enSrvType
{
	INVALID_SRV = -1,	//��Ч����
	NORMAL_CLIENT = 0,	//�����ͻ���
	PROXY_SRV = 1,		//����˴���
	PROXY_CLIENT = 2,	//�ͻ��˴���
	GAME_SRV = 3,		//��Ϸ������
	LOGIN_SRV = 4,		//��¼������
};

//ͨ��ͷ����
struct sHeadComm
{
	unsigned int uTotalLen;	//�ܳ���
	enHeadType iHeadType;	//ͷ����
};

//�������Զ���Ϣ
struct sServerInfo
{
	int iListenPort;//�����˿�
	int iJobThreadNum;//�����߳���
	int iIoThreadNum;//IO�߳���
	int iSrvType;//��������
	int iSrvID;//����ID
};

//�����������Ϣ
struct sProxyInfo
{
	bool bUseProxy;
	char szProxyIp[32];
	int iProxyPort;
};


#endif