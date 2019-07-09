
// TestIocpClientDlg.cpp : ʵ���ļ�
//

#include "stdafx.h"
#include "TestIocpClient.h"
#include "TestIocpClientDlg.h"
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


// CTestIocpClientDlg �Ի���




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


// CTestIocpClientDlg ��Ϣ�������

BOOL CTestIocpClientDlg::OnInitDialog()
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
	DWORD dwStyle = m_lstLog.GetExtendedStyle();
	dwStyle |= LVS_EX_FULLROWSELECT;
	dwStyle |= LVS_EX_GRIDLINES;
	m_lstLog.SetExtendedStyle(dwStyle);

	m_lstLog.InsertColumn(0, "��־��Ϣ", LVCFMT_LEFT, 750);
	m_edServerIp.SetWindowText("127.0.0.1");
	m_edServerPort.SetWindowText("6080");
	m_edClientNum.SetWindowText("3000");
	m_edMsgContent.SetWindowText("test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!test msg!");

	return TRUE;  // ���ǽ��������õ��ؼ������򷵻� TRUE
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

// �����Ի��������С����ť������Ҫ����Ĵ���
//  �����Ƹ�ͼ�ꡣ����ʹ���ĵ�/��ͼģ�͵� MFC Ӧ�ó���
//  �⽫�ɿ���Զ���ɡ�

void CTestIocpClientDlg::OnPaint()
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
HCURSOR CTestIocpClientDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

void CTestIocpClientDlg::OnBnClickedButtonConnect()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	//��ȡ��������ַ
	TCHAR szIp[32] = {0};
	m_edServerIp.GetWindowText(szIp, 32);
	CString strIp = szIp;

	//��ȡ�������˿�
	TCHAR szPort[32] = {0};
	m_edServerPort.GetWindowText(szPort, 32);
	int iPort = atoi(szPort);

	//��ȡ������
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
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	m_pClientSocket->DisConnectServer();
	CString strLog;
	strLog.Format("DisConnectServer!");
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}


void CTestIocpClientDlg::OnBnClickedButtonSendmsg()
{
	// TODO: �ڴ���ӿؼ�֪ͨ����������
	char szMsg[MAX_SEND_SIZE] = {0};
	m_edMsgContent.GetWindowText(szMsg, MAX_SEND_SIZE);
	UINT uLen = strlen(szMsg);
	m_pClientSocket->SendData(szMsg, uLen, 100, 10, 1);
	CString strLog;
	strLog.Format("SendData szMsg=%s", szMsg);
	m_lstLog.InsertItem(m_lstLog.GetItemCount(), strLog);
}
