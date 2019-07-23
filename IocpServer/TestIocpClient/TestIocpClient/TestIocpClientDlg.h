
// TestIocpClientDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include "yClientImpl.h"


// CTestIocpClientDlg 对话框
class CTestIocpClientDlg : public CDialogEx
{
// 构造
public:
	CTestIocpClientDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CTestIocpClientDlg();

// 对话框数据
	enum { IDD = IDD_TESTIOCPCLIENT_DIALOG };

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
	CListCtrl m_lstLog;
	// 服务器IP
	CEdit m_edServerIp;
	// 服务器端口
	CEdit m_edServerPort;
	// 客户端数量
	CEdit m_edClientNum;
	// 发送消息内容
	CEdit m_edMsgContent;
	// 连接服务器
	CButton m_btConnect;
	// 断开连接
	CButton m_btDisconnect;
	// 发送消息
	CButton m_btSendMsg;
private:
	yClientImpl* m_pClientImpl;
public:
	afx_msg void OnBnClickedButtonConnect();
	afx_msg void OnBnClickedButtonDisconnect();
	afx_msg void OnBnClickedButtonSendmsg();
};
