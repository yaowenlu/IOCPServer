
// ServerProxy.h : PROJECT_NAME Ӧ�ó������ͷ�ļ�
//

#pragma once

#ifndef __AFXWIN_H__
	#error "�ڰ������ļ�֮ǰ������stdafx.h�������� PCH �ļ�"
#endif

#include "resource.h"		// ������


// CServerProxyApp:
// �йش����ʵ�֣������ ServerProxy.cpp
//

class CServerProxyApp : public CWinApp
{
public:
	CServerProxyApp();

// ��д
public:
	virtual BOOL InitInstance();

// ʵ��

	DECLARE_MESSAGE_MAP()
};

extern CServerProxyApp theApp;