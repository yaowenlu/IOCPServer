#ifndef COMMON_ENCRYPT_H
#define COMMON_ENCRYPT_H

#include <string>
#include "openssl/aes.h"

using namespace std;

/**************************************
ͨ�ü�����ض���
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
	//Aes��/����
	bool AesEncrypt(const char* pData, const unsigned int uLen, char* &pOutData, unsigned int &uOutLen, string strKey, bool bEncrypt);

private:
	//����keyʹ֮�Ϸ�
	void DealWithAesKey(string &strKey);

private:
	static CYEncrypt* m_pInstance;//����
};

//��ȡlogger
static CYEncrypt* yEncryptIns()
{
	return CYEncrypt::GetInstance();
}

#endif