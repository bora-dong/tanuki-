#ifndef STRING_UTIL_HPP
#define STRING_UTIL_HPP

namespace string_util {

  // ��������X�y�[�X�ŋ�؂��ĕ�����̔z��ɕϊ�����
  std::vector<std::string> split(const std::string& in);

  // ������̔z����X�y�[�X��؂�Ō�������
  std::string concat(const std::vector<std::string>& words);

}

#endif
