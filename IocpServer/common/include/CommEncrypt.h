#ifndef COMMON_ENCRYPT_H
#define COMMON_ENCRYPT_H

#include <string>
#include "openssl/aes.h"

using namespace std;

/**************************************
通用加密相关定义
**************************************/

const int aes_key_len = 32;

class CYEncrypt
{
public:
	static CYEncrypt* GetInstance();
	static void ReleaseInstance();
private:
	CYEncrypt();
	~CYEncrypt();

public:
	//Aes加/解密
	bool AesEncrypt(const char* pData, const unsigned int uLen, char* &pOutData, unsigned int &uOutLen, string strKey, bool bEncrypt);

private:
	//处理key使之合法
	void DealWithAesKey(string &strKey);

private:
	static CYEncrypt* m_pInstance;//单例
};

//获取logger
static CYEncrypt* yEncryptIns()
{
	return CYEncrypt::GetInstance();
}

#endif