#ifndef STRING_UTIL_HPP
#define STRING_UTIL_HPP

namespace string_util {

  // ��������X�y�[�X�ŋ�؂��ĕ�����̔z��ɕϊ�����
  void split(const std::string& in, std::vector<std::string>& out);

  // ������̔z����X�y�[�X��؂�Ō�������
  void concat(const std::vector<std::string>& words, std::string& out);

}

#endif
