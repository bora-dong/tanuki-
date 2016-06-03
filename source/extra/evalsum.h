#ifndef EVAL_SUM_H
#define EVAL_SUM_H

#include <array>

namespace Eval {

  // std::array<T,2>�ɑ΂��� += �� -= ��񋟂���B
  template <typename Tl, typename Tr>
  inline std::array<Tl, 2> operator += (std::array<Tl, 2>& lhs, const std::array<Tr, 2>& rhs) {
    lhs[0] += rhs[0];
    lhs[1] += rhs[1];
    return lhs;
  }
  template <typename Tl, typename Tr>
  inline std::array<Tl, 2> operator -= (std::array<Tl, 2>& lhs, const std::array<Tr, 2>& rhs) {
    lhs[0] -= rhs[0];
    lhs[1] -= rhs[1];
    return lhs;
  }


  //
  // ��Ԃ��̕]���l�𑫂��Ă����Ƃ��Ɏg��class
  //

  // EvalSum sum;
  // �ɑ΂���
  // sum.p[0] = ��BKKP
  // sum.p[1] = ��WKPP
  // sum.p[2] = ��KK
  // (���ꂼ��Ɏ�Ԃ͉�������Ă�����̂Ƃ���)
  // sum.sum() == ��BKPP - ��WKPP + ��KK

  struct EvalSum {

#if defined USE_AVX2_EVAL
    EvalSum(const EvalSum& es) {
      _mm256_store_si256(&mm, es.mm);
    }
    EvalSum& operator = (const EvalSum& rhs) {
      _mm256_store_si256(&mm, rhs.mm);
      return *this;
    }
#elif defined USE_SSE_EVAL
    EvalSum(const EvalSum& es) {
      _mm_store_si128(&m[0], es.m[0]);
      _mm_store_si128(&m[1], es.m[1]);
    }
    EvalSum& operator = (const EvalSum& rhs) {
      _mm_store_si128(&m[0], rhs.m[0]);
      _mm_store_si128(&m[1], rhs.m[1]);
      return *this;
    }
#endif
    EvalSum() {}

    // ��肩�猩���]���l��Ԃ��B���̋ǖʂ̎�Ԃ� c���ɂ�����̂Ƃ���Bc�����猩���]���l��Ԃ��B
    int32_t sum(const Color c) const {

      // NDF(2014)�̎�ԕ]���̎�@�B
      // cf. http://www.computer-shogi.org/wcsc24/appeal/NineDayFever/NDF.txt

      // ��ԂɈˑ����Ȃ��]���l���v
      // p[1][0]�̓�WKPP�Ȃ̂ŕ����̓}�C�i�X�B
      const int32_t scoreBoard = p[0][0] - p[1][0] + p[2][0];
      // ��ԂɈˑ�����]���l���v
      const int32_t scoreTurn = p[0][1] + p[1][1] + p[2][1];

      // ���̊֐��͎�ԑ����猩���]���l��Ԃ��̂�scoreTurn�͕K���v���X

      return (c == BLACK ? scoreBoard : -scoreBoard) + scoreTurn;
    }
    EvalSum& operator += (const EvalSum& rhs) {
#if defined USE_AVX2
      mm = _mm256_add_epi32(mm, rhs.mm);
#else
      m[0] = _mm_add_epi32(m[0], rhs.m[0]);
      m[1] = _mm_add_epi32(m[1], rhs.m[1]);
#endif
      return *this;
    }
    EvalSum& operator -= (const EvalSum& rhs) {
#ifdef USE_AVX2
      mm = _mm256_sub_epi32(mm, rhs.mm);
#else
      m[0] = _mm_sub_epi32(m[0], rhs.m[0]);
      m[1] = _mm_sub_epi32(m[1], rhs.m[1]);
#endif
      return *this;
    }
    EvalSum operator + (const EvalSum& rhs) const { return EvalSum(*this) += rhs; }
    EvalSum operator - (const EvalSum& rhs) const { return EvalSum(*this) -= rhs; }
    union {
      std::array<std::array<int32_t, 2>, 3> p;
      struct {
        uint64_t data[3];
        uint64_t key; // ehash�p�B
      };
#if defined USE_AVX2
      __m256i mm;
      __m128i m[2];
#else // SSE2�͂�����̂Ƃ���B
      __m128i m[2];
#endif
    };
  };

  // �o�͗p�@�f�o�b�O�p�B
  static std::ostream& operator<<(std::ostream& os, const EvalSum& sum)
  {
    os << "sum BKPP = " << sum.p[0][0] << " + " << sum.p[0][1] << std::endl;
    os << "sum WKPP = " << sum.p[1][0] << " + " << sum.p[1][1] << std::endl;
    os << "sum KK   = " << sum.p[2][0] << " + " << sum.p[2][1] << std::endl;
    return os;
  }

} // namespace Eval

#endif // EVAL_SUM_H

