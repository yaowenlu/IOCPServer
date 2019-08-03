#include "wtypes.h"
#include "minwinbase.h"
#include <string>
#include <map>
#include "EventDefine.h"

using namespace std;

struct sEventInfo
{
	void *pEventFunc;
	void *pFuncParam;
};

//�ص�����ָ��
typedef int(*func_callback)(string, void *);

class CCommEvent
{
public:
	static CCommEvent* GetInstance();
	static void ReleaseInstance();
private:
	CCommEvent();
	~CCommEvent();
public:
	//���һ���¼�
	void AddOneEvent(std::string strEventName, void *pEventFunc, void *pFuncParam);

	//�Ƴ�һ���¼�
	void RemoveOneEvent(std::string strEventName);

	//��Ӧһ���¼�
	void NotifyOneEvent(std::string strEventName);
private:
	static CCommEvent* m_pInstance;
	map<string, sEventInfo> m_mapEvent;
	CRITICAL_SECTION m_csEventLock;
	func_callback m_callBack;
};