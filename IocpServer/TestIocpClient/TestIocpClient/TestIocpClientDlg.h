
// TestIocpClientDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "yClientImpl.h"


// CTestIocpClientDlg �Ի���
class CTestIocpClientDlg : public CDialogEx
{
// ����
public:
	CTestIocpClientDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CTestIocpClientDlg();

// �Ի�������
	enum { IDD = IDD_TESTIOCPCLIENT_DIALOG };

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
	CListCtrl m_lstLog;
	// ������IP
	CEdit m_edServerIp;
	// �������˿�
	CEdit m_edServerPort;
	// �ͻ�������
	CEdit m_edClientNum;
	// ������Ϣ����
	CEdit m_edMsgContent;
	// ���ӷ�����
	CButton m_btConnect;
	// �Ͽ�����
	CButton m_btDisconnect;
	// ������Ϣ
	CButton m_btSendMsg;
private:
	yClientImpl* m_pClientImpl;
public:
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSendmsg();
};
