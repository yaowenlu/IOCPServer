#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#define MAIN_KEEP_ALIVE	100
#define ASS_KEEP_ALIVE	1

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