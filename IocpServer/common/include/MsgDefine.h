#include "CommDefine.h"

#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#define MAIN_KEEP_ALIVE	100
#define ASS_SC_KEEP_ALIVE	1
#define ASS_CS_KEEP_ALIVE	2

///�������ݰ��ṹͷ
struct NetMsgHead
{
	sHeadComm headComm;		//ͨ��ͷ
	DWORD	dwMainID;		//��Ϣ��ID
	DWORD	dwAssID;		//��Ϣ��ID
	DWORD	dwHandleCode;	//��Ϣ�������
	DWORD	dwReserve;		//�����ֶ�
	NetMsgHead()
	{
		memset(this, 0, sizeof(*this));
		headComm.iHeadType = MSG_HEAD;
	}
};

#define MAIN_FRAME_MSG		150
#define ASS_CONNECT_SUCC	10
//���ӳɹ�����������ص�������Ϣ
struct sConnectSucc
{
	unsigned __int64 i64SrvIndex;
};

//���÷�����Ϣ
#define ASS_SET_SRV_INFO	20
struct sSrvInfo
{
	enSrvType iSrvType;
	USHORT usSrvID;
};

#endif