
#ifndef COMMON_DEFINE_H
#define COMMON_DEFINE_H

#include <string>

/**************************************
通用定义
**************************************/

//ip长度限制
#define MAX_IP_LEN 64

//消息是否进行加密
const bool g_encrypt = true;
const std::string g_aes_key = "oii923489jsdf78w2uhiws89tw9834";

//头类型
enum enHeadType
{
	INVALID_HEAD = 0,	//无效消息头
	MSG_HEAD = 1,		//正常消息头
	PROXY_HEAD = 2,		//代理消息头
};

//转发类型
enum enTransType
{
	invalid_trans = 0,	//无效类型
	trans_p2p = 1,		//点对点
	trans_p2g = 2,		//点到组
};

//服务类型定义
enum enSrvType
{
	INVALID_SRV = 0,	//无效服务
	NORMAL_CLIENT = 1,	//正常客户端
	PROXY_CLIENT = 2,	//客户端代理
	PROXY_SRV = 3,		//服务端代理
	GAME_SRV = 4,		//游戏服务器
	LOGIN_SRV = 5,		//登录服务器
};

//通用头定义
struct sHeadComm
{
	unsigned int uTotalLen;	//总长度
	enHeadType iHeadType;	//头类型
};

//服务器启动信息
struct sServerInfo
{
	int iListenPort;//监听端口
	int iJobThreadNum;//工作线程数
	int iIoThreadNum;//IO线程数
	int iSrvType;//服务类型
	int iSrvID;//服务ID
	sServerInfo()
	{
		memset(this, 0, sizeof(*this));
	}
};

//最大代理数
#define MAX_PROXY_COUNT	255

//代理服务器信息
struct sProxyInfo
{
	bool bUseProxy;//是否使用代理
	char szProxyIp[MAX_PROXY_COUNT][MAX_IP_LEN];//代理服务器Ip
	int iProxyPort[MAX_PROXY_COUNT];//代理服务器端口
	sProxyInfo()
	{
		memset(this, 0, sizeof(*this));
	}
};


#endif