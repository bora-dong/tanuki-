#ifndef SCANNER_HPP
#define SCANNER_HPP

#include <deque>
#include <string>
#include <vector>

// ������1�s�����󂯎��A�擪�̒P�ꂩ�珇�ɕԂ����[�e�B���e�B�N���X�B
// ���͂ɘA�������󔒂��܂܂�Ă���ꍇ��
// 1 �ɂ܂Ƃ߂�ꂽ���Ƃɏ��������B
// java.util.Scanner �̃N���[��
class Scanner
{
public:
  Scanner();
  Scanner(const char* input);
  Scanner(const std::string& input);
  Scanner(const std::vector<std::string>& input);
  void SetInput(const std::string& input);
  void SetInput(const std::vector<std::string>& input);
  bool hasNext() const;
  char hasNextChar() const;
  bool hasNextInt() const;
  std::string next();
  char nextChar();
  int nextInt();
private:
  std::deque<std::string> q;
};

#endif
