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
�������ܣ�Aes��/����
���������
	pData:��Ҫ��/���ܵ�����
	uLen:��/���ܵ����ݳ���
	pOutData:��ż�/���ܺ������
	uOutLen:��/���ܺ�����ݳ���
	strKey:��/���ܵ���Կ
	bEncrypt:true-���ܣ�false-����
���أ�
	�Ƿ�ɹ�
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
	//������Կ����
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
	//���ܵĳ�ʼ������
	unsigned char iv[AES_BLOCK_SIZE];
	//ivһ������Ϊȫ0,�����������������Ǽ�/����Ҫһ������
	for (int i = 0; i < AES_BLOCK_SIZE; ++i)
	{
		iv[i] = 0;
	}

	//������AES_BLOCK_SIZE��������
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
	//key�����ж�
	if (strKey.length() != aes_key_len)
	{
		unsigned int uLen = static_cast<unsigned int>(strKey.length());
		//���㳤��
		if (uLen < aes_key_len)
		{
			int iDiff = aes_key_len - uLen;
			char szKey[aes_key_len] = { 0 };
			for (int i = 0;i < iDiff;i++)
			{
				//a��accii����97
				szKey[i] = 97 + i % 26;
			}
			strKey += szKey;
		}
		else
		{
			//��ȡ����
			strKey = strKey.substr(0, aes_key_len);
		}
	}
	return;
}
