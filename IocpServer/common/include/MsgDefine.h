#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#define MAIN_KEEP_ALIVE	100
#define ASS_KEEP_ALIVE	1

///网络数据包结构头
struct NetMsgHead
{
	DWORD	dwMsgSize;		//数据包大小
	DWORD	dwMainID;		//消息主ID
	DWORD	dwAssID;		//消息辅ID
	DWORD	dwHandleCode;	//消息处理代码
	DWORD	dwReserve;		//保留字段
};

#endif