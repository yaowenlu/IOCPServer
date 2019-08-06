
// TestIocpClientDlg.cpp : 实现文件
//

#include <string>
#include "stdafx.h"
#include "TestIocpClient.h"
#include "TestIocpClientDlg.h"
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
	char* pNew = new char[iLen*sizeof(char)];
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


// CTestIocpClientDlg 对话框




CTestIocpClientDlg::CTestIocpClientDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTestIocpClientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pClientImpl = new yClientImpl;
}

CTestIocpClientDlg::~CTestIocpClientDlg()
{
	if(nullptr != m_pClientImpl)
	{
		delete m_pClientImpl;
		m_pClientImpl = nullptr;
	}
	CSpdlogImpl::GetInstance()->ReleaseInstance();
}

void CTestIocpClientDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_lstLog);
	DDX_Control(pDX, IDC_EDIT_SERVER_IP, m_edServerIp);
	DDX_Control(pDX, IDC_EDIT_SERVER_PORT, m_edServerPort);
	DDX_Control(pDX, IDC_EDIT_CLIENT_NUM, m_edClientNum);
	DDX_Control(pDX, IDC_EDIT_MSG_CONTENT, m_edMsgContent);
	DDX_Control(pDX, IDC_BUTTON_CONNECT, m_btConnect);
	DDX_Control(pDX, IDC_BUTTON_DISCONNECT, m_btDisconnect);
	DDX_Control(pDX, IDC_BUTTON_SENDMSG, m_btSendMsg);
	DDX_Control(pDX, IDC_CHECK_USE_PROXY, m_chUseProxy);
	DDX_Control(pDX, IDC_BUTTON_SAVECFG, m_btSaveCfg);
	
}

BEGIN_MESSAGE_MAP(CTestIocpClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CTestIocpClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CTestIocpClientDlg::OnBnClickedButtonDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SENDMSG, &CTestIocpClientDlg::OnBnClickedButtonSendmsg)
	ON_BN_CLICKED(IDC_CHECK_USE_PROXY, &CTestIocpClientDlg::OnBnClickedButtonUseProxy)
	ON_BN_CLICKED(IDC_BUTTON_SAVECFG, &CTestIocpClientDlg::OnBnClickedButtonSaveCfg)
END_MESSAGE_MAP()


// CTestIocpClientDlg 消息处理程序

BOOL CTestIocpClientDlg::OnInitDialog()
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
	DWORD dwStyle = m_lstLog.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	dwStyle |= LVS_EX_GRIDLINES;
	m_lstLog.SetExtendedStyle(dwStyle);
	m_lstLog.InsertColumn(0, _T("日志信息"), LVCFMT_LEFT, 835);
	//读取原配置
	ReadCfg();
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CTestIocpClientDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CTestIocpClientDlg::OnPaint()
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
HCURSOR CTestIocpClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestIocpClientDlg::OnBnClickedButtonConnect()
{
	WriteCfg();
	CString strLog;
	int iRet = m_pClientImpl->ConnectServer(m_sConnectInfo);
	strLog.Format(_T("ConnectServer iRet=%d"), iRet);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}


void CTestIocpClientDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码
	m_pClientImpl->DisConnectServer();
	CString strLog;
	strLog.Format(_T("DisConnectServer!"));
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}


void CTestIocpClientDlg::OnBnClickedButtonSendmsg()
{
	// TODO: 在此添加控件通知处理程序代码
	TCHAR szMsg[MAX_SEND_SIZE] = {0};
	m_edMsgContent.GetWindowText(szMsg, sizeof(szMsg));
	m_pClientImpl->SendData((void*)(TChar2String(szMsg).c_str()), MAX_SEND_SIZE*sizeof(char), 100, 10, 1);
	CString strLog;
	strLog.Format(_T("SendData szMsg=%s"), szMsg);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}

void CTestIocpClientDlg::OnBnClickedButtonSaveCfg()
{
	WriteCfg();
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), _T("SaveCfg finish!"));
}

void CTestIocpClientDlg::OnBnClickedButtonUseProxy()
{
	m_sConnectInfo.bUseProxy = m_chUseProxy.GetCheck();
}

//读取配置文件
void CTestIocpClientDlg::ReadCfg()
{
	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(sizeof(szPath), szPath);
	std::string strCfgFileName = szPath;
	strCfgFileName += "/conf/CommonCfg.ini";

	std::string strKey = "config";
	CString strTmp;
	m_sConnectInfo.bUseProxy = GetPrivateProfileIntA(strKey.c_str(), "UseProxy", 1, strCfgFileName.c_str());
	m_chUseProxy.SetCheck(m_sConnectInfo.bUseProxy);

	char szIp[32] = { 0 };
	GetPrivateProfileStringA(strKey.c_str(), "ServerIp", "127.0.0.1", szIp, sizeof(szIp), strCfgFileName.c_str());
	SetWindowTextA(m_edServerIp, szIp);
	memcpy(m_sConnectInfo.szServerIp, szIp, sizeof(szIp));

	m_sConnectInfo.iServerPort = GetPrivateProfileIntA(strKey.c_str(), "ServerPort", 6080, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sConnectInfo.iServerPort);
	m_edServerPort.SetWindowText(strTmp);

	m_sConnectInfo.iConnectNum = GetPrivateProfileIntA(strKey.c_str(), "ConnectNum", 3000, strCfgFileName.c_str());
	strTmp.Format(_T("%d"), m_sConnectInfo.iConnectNum);
	m_edClientNum.SetWindowText(strTmp);

	char szMsg[MAX_SEND_SIZE] = { 0 };
	GetPrivateProfileStringA(strKey.c_str(), "SendMsg", "test msg!", szMsg, sizeof(szMsg), strCfgFileName.c_str());
	SetWindowTextA(m_edMsgContent, szMsg);
	m_strSendMsg = szMsg;
}

//读取配置文件
void CTestIocpClientDlg::WriteCfg()
{
	char szPath[MAX_PATH] = { 0 };
	GetCurrentDirectoryA(sizeof(szPath), szPath);
	std::string strCfgFileName = szPath;
	strCfgFileName += "/conf/CommonCfg.ini";

	std::string strKey = "config";
	m_sConnectInfo.bUseProxy = m_chUseProxy.GetCheck();
	CStringA strTmp;
	strTmp.Format("%d", m_sConnectInfo.bUseProxy);
	WritePrivateProfileStringA(strKey.c_str(), "UseProxy", strTmp, strCfgFileName.c_str());

	TCHAR szTmp[64] = { 0 };
	m_edServerIp.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "ServerIp", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	memcpy(m_sConnectInfo.szServerIp, TChar2String(szTmp).c_str(), sizeof(m_sConnectInfo.szServerIp));

	memset(szTmp, 0, sizeof(szTmp));
	m_edServerPort.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "ServerPort", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sConnectInfo.iServerPort = atoi(TChar2String(szTmp).c_str());

	memset(szTmp, 0, sizeof(szTmp));
	m_edClientNum.GetWindowText(szTmp, sizeof(szTmp));
	WritePrivateProfileStringA(strKey.c_str(), "ConnectNum", TChar2String(szTmp).c_str(), strCfgFileName.c_str());
	m_sConnectInfo.iConnectNum = atoi(TChar2String(szTmp).c_str());

	char szMsg[MAX_SEND_SIZE] = { 0 };
	GetWindowTextA(m_edMsgContent, szMsg, sizeof(szMsg));
	WritePrivateProfileStringA(strKey.c_str(), "SendMsg", szMsg, strCfgFileName.c_str());
	m_strSendMsg = szMsg;
	return;
}