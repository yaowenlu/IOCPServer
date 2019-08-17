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
	pData:需要加/解密的数据
	uLen:加/解密的数据长度
	pOutData:存放加/解密后的数据
	uOutLen:加/解密后的数据长度
	strKey:加/解密的秘钥
	bEncrypt:true-加密，false-解密
返回：
	是否成功
*/
bool CYEncrypt::AesEncrypt(const char* pData, const unsigned int uLen, char* &pOutData, unsigned int &uOutLen, string strKey, bool bEncrypt)
{
	pOutData = nullptr;
	uOutLen = 0;
	if (!pData || 0 == uLen)
	{
		return false;
	}
	DealWithAesKey(strKey);
	AES_KEY aesKey;
	//设置秘钥出错
	if (bEncrypt)
	{
		if (AES_set_encrypt_key((unsigned char*)strKey.c_str(), aes_key_len * 8, &aesKey) < 0)
		{
			return false;
		}
	}
	else
	{
		if (AES_set_decrypt_key((unsigned char*)strKey.c_str(), aes_key_len * 8, &aesKey) < 0)
		{
			return false;
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
	uOutLen = uLen;
	int iMod = uOutLen%AES_BLOCK_SIZE;
	if (0 != iMod)
	{
		uOutLen = uLen - iMod + AES_BLOCK_SIZE;
	}
	pOutData = new char[uOutLen];
	if (!pOutData)
	{
		return false;
	}
	memset(pOutData, 0, sizeof(char)*(uOutLen));
	if (bEncrypt)
	{
		AES_cbc_encrypt((unsigned char*)pData, (unsigned char*)pOutData, uLen, &aesKey, iv, AES_ENCRYPT);
	}
	else
	{
		AES_cbc_encrypt((unsigned char*)pData, (unsigned char*)pOutData, uLen, &aesKey, iv, AES_DECRYPT);
	}
	
	return true;
}

void CYEncrypt::DealWithAesKey(string &strKey)
{
	//key长度判断
	if (strKey.length() != aes_key_len)
	{
		unsigned int uLen = static_cast<unsigned int>(strKey.length());
		//补足长度
		if (uLen < aes_key_len)
		{
			int iDiff = aes_key_len - uLen;
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
