#ifndef JOB_MANAGER_H
#define JOB_MANAGER_H

//Job�ṹ
struct sJobItem
{
	__int64 i64Index;//�ͻ���ΨһID
	unsigned long dwBufLen;
	unsigned char *pJobBuff;
};

class CJobManager
{
public:
	CJobManager();
	~CJobManager();

	//����һ���µ�Job
	sJobItem* NewJobItem(unsigned long dwBufLen);
	//�ͷ�һ��Job
	void ReleaseJobItem(sJobItem* pJobItem);
};


#endif