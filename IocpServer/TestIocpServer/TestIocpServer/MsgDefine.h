#ifndef MSG_DEFINE_H
#define MSG_DEFINE_H

#include "stdafx.h"

///网络数据包结构头
struct NetMessageHead
{
	DWORD	uMessageSize;		///数据包大小
	DWORD	bMainID;			///处理主类型
	DWORD	bAssistantID;		///辅助处理类型 ID
	DWORD	bHandleCode;		///数据包处理代码
	DWORD	bReserve;			///保留字段
};

#endif