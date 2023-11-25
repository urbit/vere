/// @file

#include "jets/q.h"
#include "jets/w.h"

#include "noun.h"


  u3_noun
  u3qf_core(u3_noun pac,
            u3_noun con)
  {
    if ( (c3_tas(void) == pac) ) {
      return c3_tas(void);
    } else {
      {
        u3_noun p_con, q_con, r_con, hr_con, tr_con;

        u3r_trel(con, &p_con, &q_con, &r_con);
        u3r_cell(r_con, &hr_con, &tr_con);
        if ( (c3y == u3du(hr_con)) &&
             (u3_nul == u3h(hr_con)) &&
             (u3_nul == u3t(hr_con)) )
        {
          u3l_log("old core");
          abort();
        }
      }
      return u3nt(c3_tas(core), u3k(pac), u3k(con));
    }
  }
  u3_noun
  u3wf_core(u3_noun cor)
  {
    u3_noun pac, con;

    if ( c3n == u3r_mean(cor, u3x_sam_2, &pac, u3x_sam_3, &con, 0) ) {
      return u3m_bail(c3_tas(fail));
    } else {
      return u3qf_core(pac, con);
    }
  }

#if 0
  static void
  _fork_test(const c3_c *lab_c, u3_noun set)
  {
    if ( u3_nul == set ) {
      return;
    } else {
      u3_noun n_set, l_set, r_set;

      u3x_trel(set, &n_set, &l_set, &r_set);

      u3qf_test(lab_c, n_set);
      _fork_test(lab_c, l_set);
      _fork_test(lab_c, r_set);
    }
  }
  void
  u3qf_test(const c3_c* lab_c, u3_noun sut)
  {
    u3_noun p_sut, q_sut;

    if ( c3n == u3du(sut) ) switch ( sut ) {
      default: u3m_bail(c3_tas(fail)); return;

      case c3_tas(noun):
      {
        return;
      }
      case c3_tas(void):
      {
        return;
      }
    }
    else switch ( u3h(sut) ) {
      default: u3m_bail(c3_tas(fail)); return;

      case c3_tas(atom): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        return;
      }
      case c3_tas(cell): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        u3qf_test(lab_c, p_sut);
        u3qf_test(lab_c, q_sut);
        return;
      }
      case c3_tas(core): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        u3qf_test(lab_c, p_sut);
        return;
      }
      case c3_tas(face): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        u3qf_test(lab_c, q_sut);
        return;
      }
      case c3_tas(fork): p_sut = u3t(sut);
      {
        _fork_test(lab_c, p_sut);
        return;
      }
      case c3_tas(hint): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        u3qf_test(lab_c, q_sut);
        u3qf_test(lab_c, u3h(p_sut));
        return;
      }
      case c3_tas(hold): u3x_cell(u3t(sut), &p_sut, &q_sut);
      {
        u3qf_test(lab_c, p_sut);
        return;
      }
    }
  }
#endif
