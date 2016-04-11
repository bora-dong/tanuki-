#include "../shogi.h"
#ifdef USE_ENTERING_KING_WIN
// ���ʔ��胋�[�`��

#include "../position.h"
#include "../search.h"

Move Position::DeclarationWin() const
{
  auto rule = Search::Limits.enteringKingRule;

  switch (rule)
  {
    // ���ʃ��[���Ȃ�
  case EKR_NONE: return MOVE_NONE;

    // CSA���[���Ɋ�Â��錾�����̏����𖞂����Ă��邩
    // �������Ă���Ȃ�Δ�0���Ԃ�B�Ԃ��l�͋�_�̍��v�B
    // cf.http://www.computer-shogi.org/protocol/tcp_ip_1on1_11.html
  case EKR_24_POINT: // 24�_�@(31�_�ȏ�Ő錾����)
  case EKR_27_POINT: // 27�_�@ == CSA���[��
    {
      /*
      �u���ʐ錾�����v�̏���(��13��I�茠�Ŏg�p�̂���):

      ���̏�������������ꍇ�A������錾�ł���(�ȉ��u���ʐ錾�����v�Ɖ]��)�B
      ����:
      (a) �錾���̎�Ԃł���B
      (b) �錾���̋ʂ��G�w�O�i�ڈȓ��ɓ����Ă���B
      (c) �錾����(���5�_����1�_�̌v�Z��)
      �E���̏ꍇ28�_�ȏ�̎��_������B
      �E���̏ꍇ27�_�ȏ�̎��_������B
      �E�_���̑ΏۂƂȂ�̂́A�錾���̎���ƓG�w�O�i��
      �ȓ��ɑ��݂���ʂ������錾���̋�݂̂ł���B
      (d) �錾���̓G�w�O�i�ڈȓ��̋�́A�ʂ�������10���ȏ㑶�݂���B
      (e) �錾���̋ʂɉ��肪�������Ă��Ȃ��B
      (�l�߂��K���ł��邱�Ƃ͊֌W�Ȃ�)
      (f) �錾���̎������Ԃ��c���Ă���B(�؂ꕉ���̏ꍇ)
      �ȏ�1�ł������𖞂����Ă��Ȃ��ꍇ�A�錾�������������ƂȂ�B
      (��) ���̃��[���́A���{�����A�����A�}�`���A�̌�����Ŏg�p���Ă�����̂ł���B

      �ȏ�̐錾�́A�R���s���[�^���s���A��ʏ�ɖ�������B
      */
      // (a)�錾���̎�Ԃł���B
      // ���@��ԑ��ł��̊֐����Ăяo���Ĕ��肷��̂ł������낤�B

      Color us = sideToMove;

      // �G�w
      Bitboard ef = enemy_field(us);
      
      // (b)�錾���̋ʂ��G�w�O�i�ڈȓ��ɓ����Ă���B
      if (!(ef & king_square(us)))
        return MOVE_NONE;

      // (e)�錾���̋ʂɉ��肪�������Ă��Ȃ��B
      if (checkers())
        return MOVE_NONE;


      // (d)�錾���̓G�w�O�i�ڈȓ��̋�́A�ʂ�������10���ȏ㑶�݂���B
      int p1 = (pieces(us) & ef).pop_count();
      // p1�ɂ͋ʂ��܂܂�Ă��邩��11���ȏ�Ȃ��Ƃ����Ȃ�
      if (p1 < 11)
        return MOVE_NONE;

      // �G�w�ɂ�����̐�
      int p2 = ((pieces(us, PIECE_TYPE_BITBOARD_BISHOP) | pieces(us, PIECE_TYPE_BITBOARD_ROOK)) & ef).pop_count();

      // ����1�_�A���5�_�A�ʏ���
      // ���@�G�w�̎��� + �G�w�̎���̑��~4 - ��

      // (c)
      // �E���̏ꍇ28�_�ȏ�̎��_������B
      // �E���̏ꍇ27�_�ȏ�̎��_������B
      Hand h = hand[us];
      int score = p1 + p2 * 4 - 1
        + hand_count(h,PAWN) + hand_count(h,LANCE) + hand_count(h,KNIGHT) + hand_count(h,SILVER)
        + (hand_count(h,BISHOP) + hand_count(h,ROOK)) * 5;

      // rule==EKR_27_POINT�Ȃ�CSA���[���Brule==EKR_24_POINT�Ȃ�24�_�@(30�_�ȉ����������Ȃ̂�31�_�ȏ゠��Ƃ��̂ݏ��������Ƃ���)
      if (score < (rule == EKR_27_POINT ? (us == BLACK ? 28 : 27) : 31))
        return MOVE_NONE;

      // �]���֐��ł��̂܂܎g�������̂Ŕ�0�̂Ƃ��͋�_��Ԃ��Ă����B
      return MOVE_WIN;
    }

    // �g���C���[���̏����𖞂����Ă��邩�B
  case EKR_TRY_RULE:
  {
    Color us = sideToMove;
    Square king_try_sq = (us == BLACK ? SQ_51 : SQ_59);
    Square king_sq = king_square(us);

    // 1) �����w�`�œG�ʂ������ꏊ�Ɏ��ʂ��ړ��ł��邩�B
    if (!(kingEffect(king_sq) & king_try_sq))
      return MOVE_NONE;

    // 2) �g���C���鏡�Ɏ���Ȃ����B
    if (pieces(us) & king_try_sq)
      return MOVE_NONE;

    // 3) �g���C���鏡�Ɉړ��������Ƃ��ɑ���Ɏ���Ȃ����B
    if (effected_to(~us, king_try_sq, king_sq))
      return MOVE_NONE;

    // ���̈ړ��̎w����ɂ�菟�����m�肷��
    return make_move(king_sq, king_try_sq);
  }

  default:
    UNREACHABLE;
    return MOVE_NONE;
  }
}

#endif // ENTERING_KING
