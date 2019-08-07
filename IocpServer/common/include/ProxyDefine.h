#include "CommDefine.h"
#include "wtypes.h"

#ifndef PROXY_DEFINE_H
#define PROXY_DEFINE_H

/**************************************
代理消息头定义
**************************************/

struct sProxyHead
{
	sHeadComm headComm;	//通用头
	enSrvType iSrcType;	//消息来源类型
	enSrvType iDstType;	//消息目的地类型
	USHORT uSrcID;		//消息来源ID(相同类型的服务可能有多个，通过ID区分)
	USHORT uDstID;		//消息目的地ID
	enTransType iTransType;	//转发类型
	__int64 i64Index;	//消息来源或者消息返回的具体端标识

	sProxyHead()
	{
		memset(this, 0, sizeof(*this));
		headComm.iHeadType = PROXY_HEAD;
	}
};

#endif