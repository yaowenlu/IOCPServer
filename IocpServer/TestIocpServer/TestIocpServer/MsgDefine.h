#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#include "stdafx.h"

///�������ݰ��ṹͷ
struct NetMsgHead
{
	DWORD	dwMsgSize;		//���ݰ���С
	DWORD	dwMainID;		//��Ϣ��ID
	DWORD	dwAssID;		//��Ϣ��ID
	DWORD	dwHandleCode;	//��Ϣ�������
	DWORD	dwReserve;		//�����ֶ�
};

#endif