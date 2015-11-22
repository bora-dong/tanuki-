
// tanuki-book-viewerDlg.cpp : �����t�@�C��
//

#include "stdafx.h"

#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#include <limits>

#include "tanuki-book-viewer.h"
#include "tanuki-book-viewerDlg.h"
#include "afxdialogex.h"
#include "../../src/book.hpp"
#include "../../src/csa.hpp"
#include "../../src/search.hpp"
#include "../../src/usi.hpp"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// �A�v���P�[�V�����̃o�[�W�������Ɏg���� CAboutDlg �_�C�A���O

class CAboutDlg : public CDialogEx
{
public:
  CAboutDlg();

  // �_�C�A���O �f�[�^
#ifdef AFX_DESIGN_TIME
  enum { IDD = IDD_ABOUTBOX };
#endif

protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV �T�|�[�g

// ����
protected:
  DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CtanukibookviewerDlg �_�C�A���O



CtanukibookviewerDlg::CtanukibookviewerDlg(CWnd* pParent /*=NULL*/)
  : CDialogEx(IDD_TANUKIBOOKVIEWER_DIALOG, pParent)
{
  m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CtanukibookviewerDlg::DoDataExchange(CDataExchange* pDX)
{
  CDialogEx::DoDataExchange(pDX);
  DDX_Control(pDX, IDC_RICHEDIT22, m_richEditCtrlKifu);
  DDX_Control(pDX, IDC_RICHEDIT23, m_richEditCtrlBook);
}

BEGIN_MESSAGE_MAP(CtanukibookviewerDlg, CDialogEx)
  ON_WM_SYSCOMMAND()
  ON_WM_PAINT()
  ON_WM_QUERYDRAGICON()
  ON_BN_CLICKED(IDOK, &CtanukibookviewerDlg::OnBnClickedOk)
END_MESSAGE_MAP()


// CtanukibookviewerDlg ���b�Z�[�W �n���h���[

BOOL CtanukibookviewerDlg::OnInitDialog()
{
  CDialogEx::OnInitDialog();

  // "�o�[�W�������..." ���j���[���V�X�e�� ���j���[�ɒǉ����܂��B

  // IDM_ABOUTBOX �́A�V�X�e�� �R�}���h�͈͓̔��ɂȂ���΂Ȃ�܂���B
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

  // ���̃_�C�A���O�̃A�C�R����ݒ肵�܂��B�A�v���P�[�V�����̃��C�� �E�B���h�E���_�C�A���O�łȂ��ꍇ�A
  //  Framework �́A���̐ݒ�������I�ɍs���܂��B
  SetIcon(m_hIcon, TRUE);			// �傫���A�C�R���̐ݒ�
  SetIcon(m_hIcon, FALSE);		// �������A�C�R���̐ݒ�

  // TODO: �������������ɒǉ����܂��B

  return TRUE;  // �t�H�[�J�X���R���g���[���ɐݒ肵���ꍇ�������ATRUE ��Ԃ��܂��B
}

void CtanukibookviewerDlg::OnSysCommand(UINT nID, LPARAM lParam)
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

// �_�C�A���O�ɍŏ����{�^����ǉ�����ꍇ�A�A�C�R����`�悷�邽�߂�
//  ���̃R�[�h���K�v�ł��B�h�L�������g/�r���[ ���f�����g�� MFC �A�v���P�[�V�����̏ꍇ�A
//  ����́AFramework �ɂ���Ď����I�ɐݒ肳��܂��B

void CtanukibookviewerDlg::OnPaint()
{
  if (IsIconic())
  {
    CPaintDC dc(this); // �`��̃f�o�C�X �R���e�L�X�g

    SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

    // �N���C�A���g�̎l�p�`�̈���̒���
    int cxIcon = GetSystemMetrics(SM_CXICON);
    int cyIcon = GetSystemMetrics(SM_CYICON);
    CRect rect;
    GetClientRect(&rect);
    int x = (rect.Width() - cxIcon + 1) / 2;
    int y = (rect.Height() - cyIcon + 1) / 2;

    // �A�C�R���̕`��
    dc.DrawIcon(x, y, m_hIcon);
  }
  else
  {
    CDialogEx::OnPaint();
  }
}

// ���[�U�[���ŏ��������E�B���h�E���h���b�O���Ă���Ƃ��ɕ\������J�[�\�����擾���邽�߂ɁA
//  �V�X�e�������̊֐����Ăяo���܂��B
HCURSOR CtanukibookviewerDlg::OnQueryDragIcon()
{
  return static_cast<HCURSOR>(m_hIcon);
}



void CtanukibookviewerDlg::OnBnClickedOk()
{
  CString csa;
  m_richEditCtrlKifu.GetWindowText(csa);

  std::string tempFilePath = "temp.csa";
  std::ofstream ofs(tempFilePath);
  if (!ofs.is_open()) {
    ::AfxMessageBox("�ꎞ�t�@�C���̍쐬�Ɏ��s���܂���");
    return;
  }
  if (!(ofs << std::string(csa))) {
    ::AfxMessageBox("�ꎞ�t�@�C���ւ̏������݂Ɏ��s���܂���");
    return;
  }
  ofs.close();

  GameRecord gameRecord;
  if (!csa::readCsa(tempFilePath, gameRecord)) {
    ::AfxMessageBox("�ꎞ�t�@�C���̓ǂݍ��݂Ɏ��s���܂���");
    return;
  }

  Position position;
  setPosition(position, "startpos moves");
  std::list<StateInfo> stateInfos;
  for (const auto& move : gameRecord.moves) {
    stateInfos.push_back(StateInfo());
    position.doMove(move, stateInfos.back());
  }

  Book book;
  std::vector<std::pair<Move, int> > bookMoves = book.enumerateMoves(position, "../bin/book.bin");

  if (bookMoves.empty()) {
    m_richEditCtrlBook.SetWindowText("��Ճf�[�^�x�[�X�Ƀq�b�g���܂���ł���");
    return;
  }

  int sumOfCount = 0;
  for (const auto& move : bookMoves) {
    sumOfCount += move.second;
  }
  std::string output;
  const std::vector<std::string> columnStrings = { "�P", "�Q", "�R", "�S", "�T", "�U", "�V", "�W", "�X", };
  const std::vector<std::string> rowStrings = { "��", "��", "�O", "�l", "��", "�Z", "��", "��", "��", };
  const std::vector<std::string> pieceTypeStrings = {
    "", "��", "��", "�j", "��", "�p", "��", "��", "��",
    "��", "����", "���j", "����", "�n", "��", };

  for (const auto& move : bookMoves) {
    char buffer[1024];
    int column = move.first.to() / 9;
    std::string columnString = columnStrings[column];
    int row = move.first.to() % 9;
    std::string rowString = rowStrings[row];
    PieceType pieceType = move.first.pieceTypeTo();
    std::string pieceTypeString = pieceTypeStrings[pieceType];
    sprintf_s(buffer, "%s%s%s %7d (%5.1f%%)\n",
      columnString.c_str(), rowString.c_str(), pieceTypeString.c_str(), move.second, 100.0 * move.second / sumOfCount);
    output += buffer;
  }
  m_richEditCtrlBook.SetWindowText(output.c_str());
}
