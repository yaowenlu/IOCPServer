#include "wtypes.h"

#ifndef PROXY_DEFINE_H
#define PROXY_DEFINE_H

/**************************************
代理消息头定义
**************************************/


//转发类型
enum enTransType
{
	trans_p2p = 0,	//点对点
	trans_p2g = 1,	//点到组
};

//服务类型定义
enum enSrvType
{
	PROXY_SRV = 0,		//服务端代理
	PROXY_CLIENT = 1,	//客户端代理
	GAME_SRV = 2,		//游戏服务器
	LOGIN_SRV = 3,		//登录服务器
};

struct sProxyHead
{
	DWORD dwTotalLen;	//消息总长度（包含sProxyHead大小）
	enSrvType iSrcType;	//消息来源类型
	enSrvType iDstType;	//消息目的地类型
	USHORT uSrcID;		//消息来源ID(相同类型的服务可能有多个，通过ID区分)
	USHORT uDstID;		//消息目的地ID
	enTransType iTransType;	//转发类型
	__int64 i64Index;	//消息来源或者消息返回的具体端标识
};

#endif