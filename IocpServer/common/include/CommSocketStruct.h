#ifndef COMMON_STRUCT_H
#define COMMON_STRUCT_H

//控制标识
enum
{
	SOCKET_SND = 1,//SOCKET 发送
	SOCKET_SND_FINISH,//SOCKET 发送完成
	SOCKET_REV,//SOCKET 接收
	SOCKET_REV_FINISH,//SOCKET接收完成
	SOCKET_CLOSE,//SOCKET 退出
};

//SOCKET 重叠 IO 结构
struct sOverLapped		
{
	OVERLAPPED OverLapped;//重叠结构
	WSABUF WSABuffer;//数据缓冲
	UINT uOperationType;//操作类型
	unsigned __int64 i64Index;//客户端唯一ID
};

#endif