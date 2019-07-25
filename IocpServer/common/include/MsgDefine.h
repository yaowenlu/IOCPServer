#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#define MAIN_KEEP_ALIVE	100
#define ASS_SC_KEEP_ALIVE	1
#define ASS_CS_KEEP_ALIVE	2

///网络数据包结构头
struct NetMsgHead
{
	DWORD	dwMsgSize;		//数据包大小
	DWORD	dwMainID;		//消息主ID
	DWORD	dwAssID;		//消息辅ID
	DWORD	dwHandleCode;	//消息处理代码
	DWORD	dwReserve;		//保留字段
};

#define MAIN_FRAME_MSG		150
#define ASS_CONNECT_SUCC	10
//连接成功后服务器返回的数据信息
struct sConnectSucc
{
	unsigned __int64 i64SrvIndex;
};

#endif