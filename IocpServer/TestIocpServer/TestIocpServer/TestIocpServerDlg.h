
// TestIocpServerDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "yServerSocket.h"


// CTestIocpServerDlg 对话框
class CTestIocpServerDlg : public CDialogEx
{
// 构造
public:
	CTestIocpServerDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CTestIocpServerDlg();

// 对话框数据
	enum { IDD = IDD_TESTIOCPSERVER_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
private:
	// 日志显示
	CListCtrl m_lstLogs;
	// 监听端口
	CEdit m_edListenPort;
	// 工作线程数
	CEdit m_edWorkThreadNum;
	// 启动服务
	CButton m_btStartService;
	// 停止服务
	CButton m_btStopService;
	//Socket类实例
	yServerSocket *m_pServerScoket;
public:
	afx_msg void OnBnClickedButtonStartService();
	afx_msg void OnBnClickedButtonStopService();
private:
	// IO线程数
	CEdit m_edIoThreadNum;
};
