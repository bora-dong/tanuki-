
// tanuki-book-viewerDlg.h : �w�b�_�[ �t�@�C��
//

#pragma once
#include "afxcmn.h"


// CtanukibookviewerDlg �_�C�A���O
class CtanukibookviewerDlg : public CDialogEx
{
// �R���X�g���N�V����
public:
	CtanukibookviewerDlg(CWnd* pParent = NULL);	// �W���R���X�g���N�^�[

// �_�C�A���O �f�[�^
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_TANUKIBOOKVIEWER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV �T�|�[�g


// ����
protected:
	HICON m_hIcon;

	// �������ꂽ�A���b�Z�[�W���蓖�Ċ֐�
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
  CRichEditCtrl m_richEditCtrlKifu;
  afx_msg void OnBnClickedOk();
  CRichEditCtrl m_richEditCtrlBook;
};
