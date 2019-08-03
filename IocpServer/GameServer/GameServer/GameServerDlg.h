
// GameServerDlg.h : ͷ�ļ�
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include <string>
#include "yServerImpl.h"


// CGameServerDlg �Ի���
class CGameServerDlg : public CDialogEx
{
// ����
public:
	CGameServerDlg(CWnd* pParent = NULL);	// ��׼���캯��
	~CGameServerDlg();

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
	CEdit m_edJobThreadNum;
	// IO�߳���
	CEdit m_edIoThreadNum;
	//��������
	CEdit m_edSrvType;
	//����ID
	CEdit m_edSrvID;
	//����ѡ��
	CButton m_chUseProxy;
	//����IP
	CEdit m_edProxyIp;
	//����˿�
	CEdit m_edProxyPort;
	//��������
	CButton m_btSaveCfg;
	// ��������
	CButton m_btStartService;
	// ֹͣ����
	CButton m_btStopService;
	//Socket��ʵ��
	yServerImpl *m_pServerImpl;
public:
	afx_msg void OnBnClickedButtonStartService();
	afx_msg void OnBnClickedButtonStopService();
	afx_msg void OnBnClickedButtonSaveCfg();
	afx_msg void OnBnClickedButtonUseProxy();
	//��ȡ����
	void ReadCfg();
	//д������
	void WriteCfg();
private:
	sServerInfo m_sServerInfo;//������Ϣ
	bool m_bUseProxy;
	std::string m_strProxyIp;
	int m_iProxyPort;
};
