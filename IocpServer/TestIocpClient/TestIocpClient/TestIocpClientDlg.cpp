
// TestIocpClientDlg.cpp : 实现文件
//

#include "stdafx.h"
#include "TestIocpClient.h"
#include "TestIocpClientDlg.h"
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


// CTestIocpClientDlg 对话框




CTestIocpClientDlg::CTestIocpClientDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CTestIocpClientDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	m_pClientSocket = new yClientSocket;
}

CTestIocpClientDlg::~CTestIocpClientDlg()
{
	if(nullptr != m_pClientSocket)
	{
		delete m_pClientSocket;
		m_pClientSocket = nullptr;
	}
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
}

BEGIN_MESSAGE_MAP(CTestIocpClientDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON_CONNECT, &CTestIocpClientDlg::OnBnClickedButtonConnect)
	ON_BN_CLICKED(IDC_BUTTON_DISCONNECT, &CTestIocpClientDlg::OnBnClickedButtonDisconnect)
	ON_BN_CLICKED(IDC_BUTTON_SENDMSG, &CTestIocpClientDlg::OnBnClickedButtonSendmsg)
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

	m_lstLog.InsertColumn(0, "日志信息", LVCFMT_LEFT, 750);
	m_edServerIp.SetWindowText("127.0.0.1");
	m_edServerPort.SetWindowText("6080");
	m_edClientNum.SetWindowText("3000");
	m_edMsgContent.SetWindowText("test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!");

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
	// TODO: 在此添加控件通知处理程序代码
	//获取服务器地址
	TCHAR szIp[32] = {0};
	m_edServerIp.GetWindowText(szIp, 32);
	CString strIp = szIp;

	//获取服务器端口
	TCHAR szPort[32] = {0};
	m_edServerPort.GetWindowText(szPort, 32);
	int iPort = atoi(szPort);

	//获取连接数
	TCHAR szNum[32] = {0};
	m_edClientNum.GetWindowText(szNum, 32);
	int iNum = atoi(szNum);

	CString strLog;
	strLog.Format("ConnectServer ip=%s, port=%d, num=%d", strIp, iPort, iNum);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);

	int iRet = m_pClientSocket->ConnectServer(strIp.GetBuffer(), iPort, iNum);
	strLog.Format("ConnectServer iRet=%d", iRet);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}


void CTestIocpClientDlg::OnBnClickedButtonDisconnect()
{
	// TODO: 在此添加控件通知处理程序代码
	m_pClientSocket->DisConnectServer();
	CString strLog;
	strLog.Format("DisConnectServer!");
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}


void CTestIocpClientDlg::OnBnClickedButtonSendmsg()
{
	// TODO: 在此添加控件通知处理程序代码
	char szMsg[MAX_SEND_SIZE] = {0};
	m_edMsgContent.GetWindowText(szMsg, MAX_SEND_SIZE);
	UINT uLen = strlen(szMsg);
	m_pClientSocket->SendData(szMsg, uLen, 100, 10, 1);
	CString strLog;
	strLog.Format("SendData szMsg=%s", szMsg);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}
