#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#include "stdafx.h"

///�������ݰ��ṹͷ
struct NetMessageHead
{
	DWORD	uMessageSize;		///���ݰ���С
	DWORD	bMainID;			///����������
	DWORD	bAssistantID;		///������������ ID
	DWORD	bHandleCode;		///���ݰ��������
	DWORD	bReserve;			///�����ֶ�
};

#endif