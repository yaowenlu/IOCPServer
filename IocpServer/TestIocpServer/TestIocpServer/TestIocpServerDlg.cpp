
// TestIocpServerDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "TestIocpServer.h"
#include "TestIocpServerDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ����Ӧ�ó��򡰹��ڡ��˵���� CAboutDlg �Ի���

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// �Ի�������
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV ֧��

// ʵ��
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


// CTestIocpServerDlg �Ի���




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


// CTestIocpServerDlg ��Ϣ�������

BOOL CTestIocpServerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// ��������...���˵�����ӵ�ϵͳ�˵��С�

	// IDM_ABOUTBOX ������ϵͳ���Χ�ڡ�
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

	// ���ô˶Ի����ͼ�ꡣ��Ӧ�ó��������ڲ��ǶԻ���ʱ����ܽ��Զ�
	//  ִ�д˲���
	SetIcon(m_hIcon, TRUE);			// ���ô�ͼ��
	SetIcon(m_hIcon, FALSE);		// ����Сͼ��

	// TODO: �ڴ���Ӷ���ĳ�ʼ������
	DWORD dwStyle = m_lstLogs.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	dwStyle |= LVS_EX_GRIDLINES;
	m_lstLogs.SetExtendedStyle(dwStyle);

	m_lstLogs.InsertColumn(0, "��־��Ϣ", LVCFMT_LEFT, 750);
	m_edListenPort.SetWindowText("6080");
	m_edIoThreadNum.SetWindowText("0");
	m_edWorkThreadNum.SetWindowText("0");

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
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

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CTestIocpServerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // ���ڻ��Ƶ��豸������

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// ʹͼ���ڹ����������о���
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// ����ͼ��
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//���û��϶���С������ʱϵͳ���ô˺���ȡ�ù��
//��ʾ��
HCURSOR CTestIocpServerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestIocpServerDlg::OnBnClickedButtonStartService()
{
	//��ȡ�����˿�
	TCHAR szPort[32] = {0};
	m_edListenPort.GetWindowText(szPort, 32);
	int iListenPort = atoi(szPort);
	//��ȡ�����߳���
	TCHAR szNum[32] = {0};
	m_edWorkThreadNum.GetWindowText(szNum, 32);
	int iWorkThreadNum = atoi(szNum);

	//��ȡ�����߳���
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
	// TODO: �ڴ���ӿؼ�֪ͨ����������
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
