#include "CommEncrypt.h"

#pragma comment(lib,"libcrypto.lib")
#pragma comment(lib,"libssl.lib")

CYEncrypt* CYEncrypt::m_pInstance = nullptr;

CYEncrypt * CYEncrypt::GetInstance()
{
	if (nullptr == m_pInstance)
	{
		m_pInstance = new CYEncrypt();
	}
	return m_pInstance;
}

void CYEncrypt::ReleaseInstance()
{
	if (nullptr != m_pInstance)
	{
		delete m_pInstance;
		m_pInstance = nullptr;
	}
}

CYEncrypt::CYEncrypt()
{
}

CYEncrypt::~CYEncrypt()
{
}

/*
函数功能：Aes加/解密
传入参数：
	strData:需要加/解密的数据
	strKey:加/解密的秘钥
	bEncrypt:true-加密，false-解密
返回：
	加/解密后的数据
*/
string CYEncrypt::AesEncrypt(char* pData, unsigned int uLen, string strKey, bool bEncrypt)
{
	if (!pData || 0 == uLen)
	{
		return "";
	}
	DealWithAesKey(strKey);
	AES_KEY aesKey;
	//设置秘钥出错
	if (bEncrypt)
	{
		if (AES_set_encrypt_key((unsigned char*)strKey.c_str(), aes_key_len * 8, &aesKey) < 0)
		{
			return "";
		}
	}
	else
	{
		if (AES_set_decrypt_key((unsigned char*)strKey.c_str(), aes_key_len * 8, &aesKey) < 0)
		{
			return "";
		}
	}
	//加密的初始化向量
	unsigned char iv[AES_BLOCK_SIZE];
	//iv一般设置为全0,可以设置其他，但是加/解密要一样就行
	for (int i = 0; i < AES_BLOCK_SIZE; ++i)
	{
		iv[i] = 0;
	}

	//数据是AES_BLOCK_SIZE的整数倍
	unsigned int uOutLen = uLen;
	int iMod = uOutLen%AES_BLOCK_SIZE;
	if (0 != iMod)
	{
		uOutLen = uLen - iMod + AES_BLOCK_SIZE;
	}
	char* pSzOut = new char[uOutLen + 1];
	memset(pSzOut, 0, sizeof(char)*(uOutLen + 1));
	if (bEncrypt)
	{
		AES_cbc_encrypt((unsigned char*)pData, (unsigned char*)pSzOut, uLen, &aesKey, iv, AES_ENCRYPT);
	}
	else
	{
		AES_cbc_encrypt((unsigned char*)pData, (unsigned char*)pSzOut, uLen, &aesKey, iv, AES_DECRYPT);
	}
	
	string strOut(pSzOut);
	delete[] pSzOut;
	return strOut;
}

void CYEncrypt::DealWithAesKey(string &strKey)
{
	//key长度判断
	if (strKey.length() != aes_key_len)
	{
		int iLen = strKey.length();
		//补足长度
		if (iLen < aes_key_len)
		{
			int iDiff = aes_key_len - iLen;
			char szKey[aes_key_len] = { 0 };
			for (int i = 0;i < iDiff;i++)
			{
				//a的accii码是97
				szKey[i] = 97 + i % 26;
			}
			strKey += szKey;
		}
		else
		{
			//截取长度
			strKey = strKey.substr(0, aes_key_len);
		}
	}
	return;
}
