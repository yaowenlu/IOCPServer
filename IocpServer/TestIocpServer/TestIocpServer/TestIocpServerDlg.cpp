
// TestIocpServerDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "TestIocpServer.h"
#include "TestIocpServerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


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


// CTestIocpServerDlg 对话框




CTestIocpServerDlg::CTestIocpServerDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTestIocpServerDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pServerImpl = new yServerImpl;
}

CTestIocpServerDlg::~CTestIocpServerDlg()
{
	if(nullptr != m_pServerImpl)
	{
		delete m_pServerImpl;
		m_pServerImpl = nullptr;
	}
}

void CTestIocpServerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_LIST_LOG, m_lstLogs);
	DDX_Control(pDX, IDC_EDIT_LISTEN_PORT, m_edListenPort);
	DDX_Control(pDX, IDC_EDIT_THREAD_NUM, m_edWorkThreadNum);
	DDX_Control(pDX, IDC_BUTTON_START_SERVICE, m_btStartService);
	DDX_Control(pDX, IDC_BUTTON_STOP_SERVICE, m_btStopService);
	DDX_Control(pDX, IDC_EDIT_IO_THREAD_NUM, m_edIoThreadNum);
}

BEGIN_MESSAGE_MAP(CTestIocpServerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_START_SERVICE, &CTestIocpServerDlg::OnBnClickedButtonStartService)
	ON_BN_CLICKED(IDC_BUTTON_STOP_SERVICE, &CTestIocpServerDlg::OnBnClickedButtonStopService)
END_MESSAGE_MAP()


// CTestIocpServerDlg 消息处理程序

BOOL CTestIocpServerDlg::OnInitDialog()
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

	m_lstLogs.InsertColumn(0, "日志信息", LVCFMT_LEFT, 750);
	m_edListenPort.SetWindowText("6080");
	m_edIoThreadNum.SetWindowText("0");
	m_edWorkThreadNum.SetWindowText("0");

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CTestIocpServerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

void CTestIocpServerDlg::OnPaint()
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
HCURSOR CTestIocpServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestIocpServerDlg::OnBnClickedButtonStartService()
{
	//获取监听端口
	TCHAR szPort[32] = {0};
	m_edListenPort.GetWindowText(szPort, 32);
	int iListenPort = atoi(szPort);
	//获取工作线程数
	TCHAR szNum[32] = {0};
	m_edWorkThreadNum.GetWindowText(szNum, 32);
	int iWorkThreadNum = atoi(szNum);

	//获取工作线程数
	memset(szNum, 0, sizeof(szNum));
	m_edIoThreadNum.GetWindowText(szNum, 32);
	int iIoThreadNum = atoi(szNum);

	CString strLog;
	strLog.Format("StartService iListenPort{}, iWorkThreadNum{}", iListenPort, iWorkThreadNum);
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), strLog);
	int iRet = m_pServerImpl->StartService(iListenPort, iIoThreadNum, iWorkThreadNum);
	if(0 != iRet)
	{
		strLog.Format("StartService ERROR iRet{}", iRet);
	}
	else
	{
		strLog.Format("StartService Success!");
	}
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), strLog);
}


void CTestIocpServerDlg::OnBnClickedButtonStopService()
{
	// TODO: 在此添加控件通知处理程序代码
	int iRet = m_pServerImpl->StopService();
	CString strLog;
	if(0 != iRet)
	{
		strLog.Format("StopService ERROR iRet{}", iRet);
	}
	else
	{
		strLog.Format("StopService Success!");
	}
	m_lstLogs.InsertItem(m_lstLogs.GetItemCount(), strLog);
}
