#include "JobManager.h"

CJobManager::CJobManager()
{

}

CJobManager::~CJobManager()
{

}

//申请一个新的Job
sJobItem *CJobManager::NewJobItem(unsigned long dwBufLen)
{
	sJobItem *pJob = new sJobItem;
	if(!pJob)
	{
		return nullptr;
	}
	pJob->pJobBuff = new unsigned char[dwBufLen];
	if(!pJob->pJobBuff)
	{
		delete pJob;
		pJob = nullptr;
		return nullptr;
	}
	return pJob;
}

//释放一个Job
void CJobManager::ReleaseJobItem(sJobItem* pJobItem)
{
	if(pJobItem)
	{
		if(pJobItem->pJobBuff)
		{
			delete [] pJobItem->pJobBuff;
			pJobItem->pJobBuff = nullptr;
		}
		delete pJobItem;
		pJobItem = nullptr;
	}
	return;
}