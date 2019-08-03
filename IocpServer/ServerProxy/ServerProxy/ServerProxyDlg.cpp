
// ServerProxyDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "ServerProxy.h"
#include "ServerProxyDlg.h"
#include "afxdialogex.h"
#include "SpdlogDef.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

std::string TChar2String(TCHAR *pChar)
{
	if (nullptr == pChar)
	{
		return "";
	}
	int iLen = WideCharToMultiByte(CP_ACP, 0, pChar, -1, NULL, 0, NULL, NULL);
	char* pNew = new char[iLen * sizeof(char)];
	if (nullptr == pNew)
	{
		return "";
	}
	WideCharToMultiByte(CP_ACP, 0, pChar, -1, pNew, iLen, NULL, NULL);
	std::string str(pNew);
	delete[] pNew;
	return str;
}

// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CServerProxyDlg 对话框




CServerProxyDlg::CServerProxyDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CServerProxyDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pServerImpl = new yServerImpl;
}

CServerProxyDlg::~CServerProxyDlg()
{
	if(nullptr != m_pServerImpl)
	{
		delete m_pServerImpl;
		m_pServerImpl = nullptr;
	}
	CSpdlogImpl::GetInstance()->ReleaseInstance();
}

void CServerProxyDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_lstLogs);
	DDX_Control(pDX, IDC_EDIT_LISTEN_PORT, m_edListenPort);
	DDX_Control(pDX, IDC_EDIT_THREAD_NUM, m_edJobThreadNum);
	DDX_Control(pDX, IDC_BUTTON_START_SERVICE, m_btStartService);
	DDX_Control(pDX, IDC_BUTTON_STOP_SERVICE, m_btStopService);
	DDX_Control(pDX, IDC_EDIT_IO_THREAD_NUM, m_edIoThreadNum);
	DDX_Control(pDX, IDC_EDIT_SRV_TYPE, m_edSrvType);
	DDX_Control(pDX, IDC_EDIT_SRV_ID, m_edSrvID);
	DDX_Control(pDX, IDC_BUTTON_SAVE_CFG, m_btSaveCfg);
}

BEGIN_MESSAGE_MAP(CServerProxyDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_START_SERVICE, &CServerProxyDlg::OnBnClickedButtonStartService)
	ON_BN_CLICKED(IDC_BUTTON_STOP_SERVICE, &CServerProxyDlg::OnBnClickedButtonStopService)
	ON_BN_CLICKED(IDC_BUTTON_SAVE_CFG, &CServerProxyDlg::OnBnClickedButtonSaveCfg)
END_MESSAGE_MAP()


// CServerProxyDlg 消息处理程序

BOOL CServerProxyDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	DWORD dwStyle = m_lstLogs.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	dwStyle |= LVS_EX_GRIDLINES;
	m_lstLogs.SetExtendedStyle(dwStyle);
	m_lstLogs.InsertColumn(0, _T("日志信息"), LVCFMT_LEFT, 750);
	ReadCfg();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CServerProxyDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CServerProxyDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CServerProxyDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CServerProxyDlg::OnBnClickedButtonStartService()
{
	WriteCfg();
	int iRet = m_pServerImpl->StartService(m_sServerInfo);
	CString strLog;
	if(0 != iRet)
	{
		strLog.Format(_T("StartService ERROR iRet=%d"), iRet);
	}
	else
	{
		strLog.Format(_T("StartService Success!"));
	}
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), strLog);
}


void CServerProxyDlg::OnBnClickedButtonStopService()
{
	// TODO: 在此添加控件通知处理程序代码
	int iRet = m_pServerImpl->StopService();
	CString strLog;
	if(0 != iRet)
	{
		strLog.Format(_T("StopService ERROR iRet=%d"), iRet);
	}
	else
	{
		strLog.Format(_T("StopService Success!"));
	}
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), strLog);
}

void CServerProxyDlg::OnBnClickedButtonSaveCfg()
{
	WriteCfg();
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), _T("WriteCfg finish!"));
}


//读取配置文件
void CServerProxyDlg::ReadCfg()
{
	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(sizeof(szPath), szPath);
	std::string strCfgFileName = szPath;
	strCfgFileName += "/conf/CommonCfg.ini";

	std::string strKey = "config";
	CString strTmp;
	m_sServerInfo.iListenPort = GetPrivateProfileIntA(strKey.c_str(), "ListenPort", 6080, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sServerInfo.iListenPort);
	m_edListenPort.SetWindowText(strTmp);

	m_sServerInfo.iIoThreadNum = GetPrivateProfileIntA(strKey.c_str(), "IoThreadNum", 0, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sServerInfo.iIoThreadNum);
	m_edIoThreadNum.SetWindowText(strTmp);

	m_sServerInfo.iJobThreadNum = GetPrivateProfileIntA(strKey.c_str(), "JobThreadNum", 0, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sServerInfo.iJobThreadNum);
	m_edJobThreadNum.SetWindowText(strTmp);

	m_sServerInfo.iSrvType = GetPrivateProfileIntA(strKey.c_str(), "SrvType", 1, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sServerInfo.iSrvType);
	m_edSrvType.SetWindowText(strTmp);

	m_sServerInfo.iSrvID = GetPrivateProfileIntA(strKey.c_str(), "SrvID", 1, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sServerInfo.iSrvID);
	m_edSrvID.SetWindowText(strTmp);
}

//读取配置文件
void CServerProxyDlg::WriteCfg()
{
	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(sizeof(szPath), szPath);
	std::string strCfgFileName = szPath;
	strCfgFileName += "/conf/CommonCfg.ini";

	std::string strKey = "config";
	TCHAR szTmp[64] = { 0 };
	m_edListenPort.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "ListenPort", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sServerInfo.iListenPort = atoi(TChar2String(szTmp).c_str());

	memset(szTmp, 0, sizeof(szTmp));
	m_edIoThreadNum.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "IoThreadNum", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sServerInfo.iIoThreadNum = atoi(TChar2String(szTmp).c_str());

	memset(szTmp, 0, sizeof(szTmp));
	m_edJobThreadNum.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "JobThreadNum", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sServerInfo.iJobThreadNum = atoi(TChar2String(szTmp).c_str());

	memset(szTmp, 0, sizeof(szTmp));
	m_edSrvType.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "SrvType", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sServerInfo.iSrvType = atoi(TChar2String(szTmp).c_str());

	memset(szTmp, 0, sizeof(szTmp));
	m_edSrvID.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "SrvID", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sServerInfo.iSrvID = atoi(TChar2String(szTmp).c_str());
	return;
}



