
// TestIocpServerDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "yServerSocket.h"


// CTestIocpServerDlg �Ի���
class CTestIocpServerDlg : public CDialogEx
{
// ����
public:
	CTestIocpServerDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CTestIocpServerDlg();

// �Ի�������
	enum { IDD = IDD_TESTIOCPSERVER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV ֧��


// ʵ��
protected:
	HICON m_hIcon;

	// ���ɵ���Ϣӳ�亯��
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	// ��־��ʾ
	CListCtrl m_lstLogs;
	// �����˿�
	CEdit m_edListenPort;
	// �����߳���
	CEdit m_edWorkThreadNum;
	// ��������
	CButton m_btStartService;
	// ֹͣ����
	CButton m_btStopService;
	//Socket��ʵ��
	yServerSocket *m_pServerScoket;
public:
	afx_msg void OnBnClickedButtonStartService();
	afx_msg void OnBnClickedButtonStopService();
private:
	// IO�߳���
	CEdit m_edIoThreadNum;
};
