#include "CommDefine.h"
#include "wtypes.h"

#ifndef PROXY_DEFINE_H
#define PROXY_DEFINE_H

/**************************************
������Ϣͷ����
**************************************/

struct sProxyHead
{
	sHeadComm headComm;	//ͨ��ͷ
	enSrvType iSrcType;	//��Ϣ��Դ����
	enSrvType iDstType;	//��ϢĿ�ĵ�����
	USHORT uSrcID;		//��Ϣ��ԴID(��ͬ���͵ķ�������ж����ͨ��ID����)
	USHORT uDstID;		//��ϢĿ�ĵ�ID
	enTransType iTransType;	//ת������
	__int64 i64Index;	//��Ϣ��Դ������Ϣ���صľ���˱�ʶ

	sProxyHead()
	{
		memset(this, 0, sizeof(*this));
		headComm.iHeadType = PROXY_HEAD;
	}
};

#endif