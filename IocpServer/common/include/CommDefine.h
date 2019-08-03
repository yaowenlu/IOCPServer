
#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

/**************************************
通用定义
**************************************/


//头类型
enum enHeadType
{
	INVALID_HEAD = -1,	//无效消息头
	MSG_HEAD = 0,		//正常消息头
	PROXY_HEAD = 1,		//代理消息头
};

//转发类型
enum enTransType
{
	trans_p2p = 0,	//点对点
	trans_p2g = 1,	//点到组
};

//服务类型定义
enum enSrvType
{
	INVALID_SRV = -1,	//无效服务
	NORMAL_CLIENT = 0,	//正常客户端
	PROXY_SRV = 1,		//服务端代理
	PROXY_CLIENT = 2,	//客户端代理
	GAME_SRV = 3,		//游戏服务器
	LOGIN_SRV = 4,		//登录服务器
};


struct sServerInfo
{
	int iListenPort;//监听端口
	int iJobThreadNum;//工作线程数
	int iIoThreadNum;//IO线程数
	int iSrvType;//服务类型
	int iSrvID;//服务ID
};

#endif