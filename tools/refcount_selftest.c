#include "noun.h"

//  a sink for the block-annotation fixture below
//
u3_noun sink_glob;

/* transfer fn that leaks its argument on one path
*/
u3_noun
bug_leak(u3_noun a)
{
  if ( c3y == u3a_is_cell(a) ) {
    return u3_nul;              //  BUG: a leaked
  }
  u3z(a);
  return u3_nul;
}

/* double free
*/
u3_noun
bug_double(u3_noun a)
{
  u3z(a);
  u3z(a);                       //  BUG: double free
  return u3_nul;
}

/* frees a retained argument
** @Refcount: retains `a`
*/
u3_noun
bug_overfree(u3_noun a)
{
  u3z(a);                       //  BUG: freeing retained arg
  return u3_nul;
}

/* use after free via derived reference
*/
u3_noun
bug_uaf(u3_noun a)
{
  u3_noun t = u3t(a);
  u3z(a);
  return u3k(t);                //  BUG: t may be freed with a
}

/* returns uncounted reference from transfer-product fn
*/
u3_noun
bug_borrow(u3_noun a)
{
  u3_noun h = u3h(a);
  u3z(a);
  return h;                     //  BUG(s): borrowed return + UAF
}

/* correct transfer function (control)
*/
u3_noun
bug_ok(u3_noun a)
{
  u3_noun pro = u3nc(u3k(u3h(a)), u3k(u3t(a)));
  u3z(a);
  return pro;
}

/* parks an owned product in a retained argument variable, then drops it
** @Refcount: retains `a`
*/
u3_noun
bug_smuggle(u3_noun a)
{
  a = u3qa_inc(a);
  return u3_nul;                //  BUG: the product parked in [a] leaks
}

/* retains its argument and returns an uncounted reference (control)
** @Refcount: retains
*/
u3_noun
ok_retain_prod(u3_noun a)
{
  return u3h(a);
}

/* identity function: the product is the argument itself (control)
** @Refcount: passthrough `a`
*/
u3_noun
ok_passthrough(u3_noun a)
{
  return a;
}

/* protocol too complex to model; body not checked despite the leak
** @Refcount: custom
*/
u3_noun
custom_unchecked(u3_noun a)
{
  return u3_nul;               //  would leak a, but @Refcount: custom
}

/* trusted transfer; body not checked despite the leak
** @Refcount: assert transfers
*/
u3_noun
assert_unchecked(u3_noun a)
{
  return u3_nul;               //  would leak a, but @Refcount: assert
}

/* bails unless `a` is a direct atom; not checked, used at a call site below
** @Refcount: assert
** @Refcount: direct `a`
*/
u3_noun
needs_direct(u3_noun a)
{
  return u3_nul;
}

/* passing an owned reference to a DIRECT parameter cannot leak (control)
*/
u3_noun
ok_direct_caller(u3_noun a)
{
  u3_noun b = needs_direct(a);  //  a refined to a direct atom here
  u3z(b);
  return u3_nul;
}

/* a store blessed by a block-level annotation is the intended consumption
** @Refcount: retains `a`
*/
u3_noun
ok_block(u3_noun a)
{
  u3_noun pro = u3k(a);
  {  // @Refcount: assert transfer pro
    sink_glob = pro;            //  blessed store: consumes pro
  }
  return u3_nul;
}

/* conflicting annotations: last write wins, but the conflict is reported
** @Refcount: transfers
** @Refcount: retains product
*/
u3_noun
warn_conflict(u3_noun a)
{
  u3z(a);
  return u3_nul;                //  BUG: conflicting @Refcount annotations
}

/* forward goto to a cleanup label: fully analyzable, no findings
*/
u3_noun
ok_fwd_goto(u3_noun a)
{
  u3_noun pro = u3qa_inc(a);
  if ( 0 == u3h(a) ) {
    goto end;
  }
  u3z(pro);
  pro = u3_nul;
  end:
  u3z(a);
  return pro;
}

/* backward goto forms a loop the walker cannot model: skipped
*/
u3_noun
skip_back_goto(u3_noun a)
{
  again:
  if ( 0 == u3h(a) ) {
    goto again;
  }
  u3z(a);
  return u3_nul;
}

/* an early-terminating first arm must not starve later arms or the
** code after the switch (the fl.c shape)
*/
u3_noun
bug_switch_tail(u3_noun a, c3_m tag_m)
{
  u3_noun pro = u3qa_inc(a);
  switch ( tag_m ) {
    default:
      u3z(pro); u3z(a);
      return u3m_bail(c3__exit);
    case c3__fl:
      break;
  }
  u3z(a);
  return u3_nul;              //  BUG: pro leaked after the switch
}

/* c3_min-style statement-expression with a one-sided directness guard:
** the word-min of {>=1 direct} is direct, provable via var-vs-var bounds
** @Refcount: retains arguments
*/
u3_noun
ok_min_shape(u3_noun a, u3_noun b)
{
  if ( _(u3a_is_cat(a)) || _(u3a_is_cat(b)) ) {
    return c3_min(a, b);
  }
  return u3k(a);
}
