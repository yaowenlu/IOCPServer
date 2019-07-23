#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

//Job结构
struct sJobItem
{
	__int64 i64Index;//客户端唯一ID
	unsigned long dwBufLen;
	unsigned char *pJobBuff;
};

class CJobManager
{
public:
	CJobManager();
	~CJobManager();

	//申请一个新的Job
	sJobItem* NewJobItem(unsigned long dwBufLen);
	//释放一个Job
	void ReleaseJobItem(sJobItem* pJobItem);
};


#endif