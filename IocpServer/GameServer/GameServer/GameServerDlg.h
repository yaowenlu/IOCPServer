
// GameServerDlg.h : 头文件
//

#pragma once
#include "afxcmn.h"
#include "afxwin.h"
#include <string>
#include "yServerImpl.h"


// CGameServerDlg 对话框
class CGameServerDlg : public CDialogEx
{
// 构造
public:
	CGameServerDlg(CWnd* pParent = NULL);	// 标准构造函数
	~CGameServerDlg();

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
	CEdit m_edJobThreadNum;
	// IO线程数
	CEdit m_edIoThreadNum;
	//服务类型
	CEdit m_edSrvType;
	//服务ID
	CEdit m_edSrvID;
	//代理选择
	CButton m_chUseProxy;
	//代理IP
	CEdit m_edProxyIp;
	//代理端口
	CEdit m_edProxyPort;
	//保存配置
	CButton m_btSaveCfg;
	// 启动服务
	CButton m_btStartService;
	// 停止服务
	CButton m_btStopService;
	//Socket类实例
	yServerImpl *m_pServerImpl;
public:
	afx_msg void OnBnClickedButtonStartService();
	afx_msg void OnBnClickedButtonStopService();
	afx_msg void OnBnClickedButtonSaveCfg();
	afx_msg void OnBnClickedButtonUseProxy();
	//读取配置
	void ReadCfg();
	//写入配置
	void WriteCfg();
private:
	sServerInfo m_sServerInfo;//服务信息
	bool m_bUseProxy;
	std::string m_strProxyIp;
	int m_iProxyPort;
};
