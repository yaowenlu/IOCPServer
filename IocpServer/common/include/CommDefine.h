
#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

#include <string>

/**************************************
ͨ�ö���
**************************************/

//ip��������
#define MAX_IP_LEN 64

//��Ϣ�Ƿ���м���
const bool g_encrypt = true;
const std::string g_aes_key = "oii923489jsdf78w2uhiws89tw9834";

//ͷ����
enum enHeadType
{
	INVALID_HEAD = 0,	//��Ч��Ϣͷ
	MSG_HEAD = 1,		//������Ϣͷ
	PROXY_HEAD = 2,		//������Ϣͷ
};

//ת������
enum enTransType
{
	invalid_trans = 0,	//��Ч����
	trans_p2p = 1,		//��Ե�
	trans_p2g = 2,		//�㵽��
};

//�������Ͷ���
enum enSrvType
{
	INVALID_SRV = 0,	//��Ч����
	NORMAL_CLIENT = 1,	//�����ͻ���
	PROXY_CLIENT = 2,	//�ͻ��˴���
	PROXY_SRV = 3,		//����˴���
	GAME_SRV = 4,		//��Ϸ������
	LOGIN_SRV = 5,		//��¼������
};

//ͨ��ͷ����
struct sHeadComm
{
	unsigned int uTotalLen;	//�ܳ���
	enHeadType iHeadType;	//ͷ����
};

//������������Ϣ
struct sServerInfo
{
	int iListenPort;//�����˿�
	int iJobThreadNum;//�����߳���
	int iIoThreadNum;//IO�߳���
	int iSrvType;//��������
	int iSrvID;//����ID
	sServerInfo()
	{
		memset(this, 0, sizeof(*this));
	}
};

//��������
#define MAX_PROXY_COUNT	255

//�����������Ϣ
struct sProxyInfo
{
	bool bUseProxy;//�Ƿ�ʹ�ô���
	char szProxyIp[MAX_PROXY_COUNT][MAX_IP_LEN];//���������Ip
	int iProxyPort[MAX_PROXY_COUNT];//����������˿�
	sProxyInfo()
	{
		memset(this, 0, sizeof(*this));
	}
};


#endif