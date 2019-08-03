#include "wtypes.h"

#ifndef PROXY_DEFINE_H
#define PROXY_DEFINE_H

/**************************************
������Ϣͷ����
**************************************/


//ת������
enum enTransType
{
	trans_p2p = 0,	//��Ե�
	trans_p2g = 1,	//�㵽��
};

//�������Ͷ���
enum enSrvType
{
	PROXY_SRV = 0,		//����˴���
	PROXY_CLIENT = 1,	//�ͻ��˴���
	GAME_SRV = 2,		//��Ϸ������
	LOGIN_SRV = 3,		//��¼������
};

struct sProxyHead
{
	DWORD dwTotalLen;	//��Ϣ�ܳ��ȣ�����sProxyHead��С��
	enSrvType iSrcType;	//��Ϣ��Դ����
	enSrvType iDstType;	//��ϢĿ�ĵ�����
	USHORT uSrcID;		//��Ϣ��ԴID(��ͬ���͵ķ�������ж����ͨ��ID����)
	USHORT uDstID;		//��ϢĿ�ĵ�ID
	enTransType iTransType;	//ת������
	__int64 i64Index;	//��Ϣ��Դ������Ϣ���صľ���˱�ʶ
};

#endif