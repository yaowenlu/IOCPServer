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

//回调函数指针
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
	//添加一个事件
	void AddOneEvent(std::string strEventName, void *pEventFunc, void *pFuncParam);

	//移除一个事件
	void RemoveOneEvent(std::string strEventName);

	//响应一个事件
	void NotifyOneEvent(std::string strEventName);
private:
	static CCommEvent* m_pInstance;
	map<string, sEventInfo> m_mapEvent;
	CRITICAL_SECTION m_csEventLock;
	func_callback m_callBack;
};