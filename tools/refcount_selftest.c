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

/* a typo'd block annotation must be reported, not silently ignored
** (block_asserts only regex-matches well-formed asserts)
*/
u3_noun
warn_typo_assert(u3_noun a)
{
  {  // @Refcount: asswert transfer
  }
  return a;
}

/* borrowed view held across a gate slam: the gate may have captured
** the same noun, and unifying equality inside the slam can free the
** caller-unprotected interior copy (the roll.c bug, PR #865)
** @Refcount: retains arguments
*/
u3_noun
bug_slam_stale(u3_noun a, u3_noun b)
{
  u3j_site sit_u;
  u3j_gate_prep(&sit_u, u3k(b));

  u3_noun t   = u3t(a);
  u3_noun res = u3j_gate_slam(&sit_u, u3k(u3h(a)));
  u3_noun pro = u3nc(res, u3k(t));   //  BUG: t may be dangling

  u3j_gate_lose(&sit_u);
  return pro;
}

/* u3x_atom only proves ATOM: using the word as a raw C integer needs
** a directness proof (u3a_is_cat) or a proper extraction (u3r_word)
** @Refcount: retains arguments
*/
u3_noun
bug_indirect_int(u3_noun a)
{
  return u3i_word(2 * u3x_atom(u3h(a)));
}

/* reads through a pointer-to-noun parameter without consuming it
** @Refcount: reads `a`
*/
static c3_w
_peek_pointee(u3_atom* a)
{
  return *a;
}

/* caller of a reads-pointee function (control)
*/
u3_noun
ok_pointee_reads(u3_noun a)
{
  c3_w w = _peek_pointee(&a);
  u3z(a);
  return u3i_word(w);
}

/* fills the out-parameter with an owned noun; old contents untouched
** @Refcount: fills transferred `out`
*/
static void
_fill_pointee(u3_noun* out)
{
  *out = u3nc(u3_nul, u3_nul);
}

/* an uninitialized local filled by an out-param call (control)
*/
u3_noun
ok_pointee_fill(u3_noun a)
{
  u3_noun out;
  u3z(a);
  _fill_pointee(&out);
  return out;
}

/* consumes the old pointee and fills in a new owned one
** @Refcount: consumes `acc`, fills transferred `acc`
*/
static void
_bump_pointee(u3_noun* acc)
{
  *acc = u3nc(*acc, u3_nul);
}

/* in-place accumulator update: old value consumed inside the call (control)
*/
u3_noun
ok_pointee_update(u3_noun a)
{
  _bump_pointee(&a);
  return a;
}

/* fill-only callee overwrites an owned pointee: the old reference leaks
*/
u3_noun
bug_pointee_overwrite(u3_noun a)
{
  _fill_pointee(&a);            //  BUG: a overwritten, old value leaked
  return a;
}

/* hands out an uncounted view of the argument's interior
** @Refcount: retains `som`, fills retained `hed`
*/
static void
_view_pointee(u3_noun som, u3_noun* hed)
{
  *hed = u3h(som);
}

/* a retained fill is copied before its source dies (control)
*/
u3_noun
ok_pointee_view(u3_noun a)
{
  u3_noun hed;
  _view_pointee(a, &hed);
  u3_noun pro = u3k(hed);
  u3z(a);
  return pro;
}

/* a retained fill dies with its source
*/
u3_noun
bug_pointee_view_uaf(u3_noun a)
{
  u3_noun hed;
  _view_pointee(a, &hed);
  u3z(a);
  return u3k(hed);              //  BUG: hed view died with a
}

/* deferred-cons list builder (the turn.c shape): each iteration fills
** the previous slot and leaves exactly one new hole (control)
** @Refcount: retains arguments
*/
u3_noun
ok_defcons_build(u3_noun a)
{
  u3_noun  pro;
  u3_noun* lit = &pro;

  while ( u3_nul != a ) {
    u3_noun* hed;
    u3_noun* tel;
    *lit = u3i_defcons(&hed, &tel);
    *hed = u3qa_inc(u3h(a));
    lit  = tel;
    a    = u3t(a);
  }
  *lit = u3_nul;
  return pro;
}

/* the tail slot is never filled: an incomplete cell escapes
** @Refcount: retains arguments
*/
u3_noun
bug_defcons_unfilled(u3_noun a)
{
  u3_noun* hed;
  u3_noun* tel;
  u3_noun  pro = u3i_defcons(&hed, &tel);
  *hed = u3qa_inc(a);
  return pro;                   //  BUG: tel's slot never filled
}

/* the same slot filled twice: the first value leaks and the cell is
** corrupted
** @Refcount: retains arguments
*/
u3_noun
bug_defcons_double(u3_noun a)
{
  u3_noun* hed;
  u3_noun* tel;
  u3_noun  pro = u3i_defcons(&hed, &tel);
  *hed = u3qa_inc(a);
  *hed = u3qa_inc(a);           //  BUG: slot already filled
  *tel = u3_nul;
  return pro;
}

/* two counts on one temporary: one consumed by the gate slam, one by
** the cons -- the refactored skid.c shape (control)
** @Refcount: retains arguments
*/
u3_noun
ok_double_gain(u3_noun a, u3_noun b)
{
  u3j_site sit_u;
  u3j_gate_prep(&sit_u, u3k(b));

  u3_noun i   = u3k(u3k(u3h(a)));
  u3_noun res = u3j_gate_slam(&sit_u, i);
  u3_noun pro = u3nc(res, i);

  u3j_gate_lose(&sit_u);
  return pro;
}

/* noreturn: execution ends at the call site; sloppy counts are fine
** @Refcount: noreturn
*/
void
selftest_die(u3_noun msg)
{
  u3m_p("die", msg);
  abort();
}

/* borrowed views may be handed to a noreturn callee without u3k
** @Refcount: retains arguments
*/
void
ok_noreturn_caller(u3_noun a)
{
  if ( c3n == u3du(a) ) {
    selftest_die(u3h(a));
  }
}

/* a call through a function pointer transfers: consuming an owned
** value this way is correct (control)
*/
void
ok_fnptr_transfer(u3_noun a, u3_noun (*fun_f)(u3_noun))
{
  u3z(fun_f(a));
}

/* a call through a function pointer transfers: passing a borrowed
** view without u3k is an error
** @Refcount: retains arguments
*/
void
bug_fnptr_borrowed(u3_noun a, void (*fun_f)(u3_noun))
{
  fun_f(u3h(a));                //  BUG: borrowed view transferred
}

/* u3i_list consumes every vararg (control)
** @Refcount: retains arguments
*/
u3_noun
ok_vararg_list(u3_noun a)
{
  return u3i_list(u3k(a), u3qa_inc(a), u3_none);
}

/* u3i_list consumes every vararg: a borrowed view needs u3k
** @Refcount: retains arguments
*/
u3_noun
bug_vararg_borrowed(u3_noun a)
{
  return u3i_list(u3h(a), u3_none);  //  BUG: borrowed view consumed
}

/* u3h_git's product borrows from the table, not the key: freeing the
** key does not invalidate it (control)
** @Refcount: retains
*/
u3_weak
ok_git_untied(u3p(u3h_root) har_p, u3_noun a)
{
  u3_noun key = u3nc(u3k(a), 0);
  u3_weak pro = u3h_git(har_p, key);
  u3z(key);
  return pro;
}

/* a local function-pointer declaration is not an initializer;
** the call through it transfers (control)
*/
void
ok_fnptr_decl(u3_noun a, void (*pass_f)(u3_noun))
{
  void (*fun_f)(u3_noun);
  fun_f = pass_f;
  fun_f(a);
}

/* leaks are tolerated in a noreturn function: the process dies anyway
** @Refcount: noreturn
*/
void
ok_noreturn_leak(u3_noun a)
{
  c3_free(u3r_string(u3k(a)));  //  leaked copy: fine, we die
  abort();
}

/* a noreturn function with a reachable return is an annotation error
** @Refcount: noreturn
*/
void
warn_noreturn_return(u3_noun a)
{
  if ( 0 == a ) {
    return;                     //  BUG: annotated noreturn
  }
  abort();
}

/* _cond_fill(): fills rid with an owned value only when returning c3y
** @Refcount: retains arguments, fills transferred `rid` on `c3y`
*/
c3_o
_cond_fill(u3_noun a, u3_noun* rid)
{
  if ( c3n == u3a_is_cell(a) ) {
    return c3n;
  }
  *rid = u3k(u3h(a));
  return c3y;
}

/* the conditional fill lands on the matching branch (control)
** @Refcount: retains arguments
*/
u3_noun
ok_cond_fill(u3_noun a)
{
  u3_noun rid;
  if ( c3n == _cond_fill(a, &rid) ) {
    return u3_nul;
  }
  return rid;
}

/* filled but claims failure
** @Refcount: retains arguments, fills transferred `rid` on `c3y`
*/
c3_o
bug_cond_fill_wrongpath(u3_noun a, u3_noun* rid)
{
  *rid = u3k(a);
  return c3n;                   //  BUG: filled on the c3n path
}

/* leaks on the failing path are fine: the caller must die on c3n
** @Refcount: retains arguments, doomed on `c3n`
*/
c3_o
ok_doomed(u3_noun a)
{
  u3_noun pro = u3qa_inc(a);
  if ( c3n == u3a_is_cell(a) ) {
    return c3n;                 //  pro leaked: the caller dies
  }
  u3z(pro);
  return c3y;
}

/* out-params handed through to a destructurer
** @Refcount: retains arguments, fills retained `hed`, `tel`
*/
void
ok_slot_destructure(u3_noun a, u3_noun* hed, u3_noun* tel)
{
  u3x_cell(a, hed, tel);
}

/* bare u3k of a pointee upgrades it in place
** @Refcount: retains arguments, fills transferred `out`
*/
void
ok_gain_deref(u3_noun a, u3_noun* out)
{
  u3x_cell(a, out, 0);
  u3k(*out);
}

/* a callback field annotated on its declarator
*/
typedef struct _selftest_cb {
  void (*ret_f)(u3_noun);  //  @Refcount: retains
} selftest_cb;

/* the annotated field is called as retaining
** @Refcount: retains arguments
*/
void
ok_fnptr_field(selftest_cb* cb_u, u3_noun a)
{
  cb_u->ret_f(u3h(a));     //  borrowed view: fine, the field retains
}

/* the u3_mars_grab shape: a conditional destructurer fill upgraded in
** place with a bare u3k, surviving the parent's death; the untouched
** path keeps the u3_nul initializer
** @Refcount: retains arguments
*/
u3_noun
ok_cond_view_gain(u3_noun a)
{
  u3_noun sac = u3_nul;
  u3_noun gon = u3qa_inc(a);

  if ( c3y == u3r_cell(gon, 0, &sac) ) {
    u3k(sac);
  }
  u3z(gon);

  return sac;
}

/* _cond_view(): fills out with a borrowed view only when returning c3y
** @Refcount: retains arguments, fills retained `out` on `c3y`
*/
c3_o
_cond_view(u3_noun a, u3_noun* out)
{
  if ( c3n == u3a_is_cell(a) ) {
    return c3n;
  }
  *out = u3h(a);
  return c3y;
}

/* the u3_mars_grab shape through an ANNOTATED conditional retained
** fill: the optimistic view fill is undone on the failing branch,
** upgraded in place on the matching one
** @Refcount: retains arguments
*/
u3_noun
ok_cond_view_annot(u3_noun a)
{
  u3_noun sac = u3_nul;
  u3_noun gon = u3qa_inc(a);

  if ( c3y == _cond_view(gon, &sac) ) {
    u3k(sac);
  }
  u3z(gon);

  return sac;
}

/* fills the view but claims failure
** @Refcount: retains arguments, fills retained `out` on `c3y`
*/
c3_o
bug_cond_view_wrongpath(u3_noun a, u3_noun* out)
{
  *out = u3h(a);
  return c3n;                   //  BUG: filled on the c3n path
}

/* trusted directness assertion without a runtime check
** @Refcount: retains arguments
*/
c3_w
ok_assert_direct(u3_noun a)
{
  u3_noun inc = u3qa_inc(a);
  { //  @Refcount: assert direct inc
    return (c3_w)inc;
  }
}

/* a lookup that may miss: u3_weak product (control source for the
** u3_none fixtures below)
** @Refcount: retains arguments
*/
static u3_weak
weak_find(u3_noun key)
{
  if ( c3y == u3a_is_cat(key) ) {
    return u3k(key);
  }
  return u3_none;
}

/* checks a maybe-none product before using it (control)
*/
u3_noun
ok_weak_flow(u3_noun a)
{
  u3_weak fnd = weak_find(u3h(a));

  u3z(a);
  if ( u3_none == fnd ) {
    return u3_nul;
  }
  return fnd;
}

/* hands a maybe-none product to a u3_noun parameter
*/
u3_noun
bug_weak_use(u3_noun a)
{
  u3_weak fnd = weak_find(u3h(a));

  u3z(a);
  return u3nc(fnd, u3_nul);     //  BUG: fnd may be u3_none
}

/* returns a maybe-none product from a u3_noun function
*/
u3_noun
bug_weak_return(u3_noun a)
{
  u3_weak fnd = weak_find(u3h(a));

  u3z(a);
  return fnd;                   //  BUG: fnd may be u3_none
}

/* returns the u3_none literal from a u3_noun function
*/
u3_noun
bug_weak_lit(u3_noun a)
{
  u3z(a);
  return u3_none;               //  BUG: u3_noun promises a valid noun
}

/* gains a maybe-none product
*/
u3_noun
bug_weak_gain(u3_noun a)
{
  u3_weak fnd = weak_find(u3h(a));

  u3z(a);
  return u3k(fnd);              //  BUG: u3a_gain asserts on u3_none
}

/* binds a maybe-none product to a u3_noun variable
*/
u3_noun
bug_weak_bind(u3_noun a)
{
  u3_noun fnd = weak_find(u3h(a));  //  BUG: fnd may be u3_none

  u3z(a);
  return fnd;
}

/* loses a maybe-none product without checking: safe by default (u3z
** no-ops on u3_none), reported under --strict-weak
*/
u3_noun
weak_lose_unchecked(u3_noun a)
{
  u3_weak fnd = weak_find(u3h(a));

  u3z(a);
  u3z(fnd);
  return u3_nul;
}

/* u3_weak parameter blessed by an inline check (control)
** @Refcount: retains `may`
*/
u3_noun
ok_weak_param(u3_weak may)
{
  if ( u3_none != may ) {
    return u3k(may);
  }
  return u3_nul;
}

/* u3_weak parameter used without checking
** @Refcount: retains `may`
*/
u3_noun
bug_weak_param(u3_weak may)
{
  return u3k(may);              //  BUG: may can be u3_none
}

/* equality with a valid literal proves the value is not u3_none
** @Refcount: retains `may`
*/
u3_noun
ok_weak_eqlit(u3_weak may)
{
  if ( 42 == may ) {
    return u3k(may);
  }
  return u3_nul;
}

/* u3x_good promises a valid product by its u3_noun signature
** @Refcount: retains `may`
*/
u3_noun
ok_weak_good(u3_weak may)
{
  return u3k(u3x_good(may));
}

/* a u3_none arm makes a conditional product maybe-none
** @Refcount: retains arguments
*/
u3_noun
bug_weak_ternary(u3_noun a)
{
  u3_weak pro = ( c3y == u3a_is_cell(a) ) ? u3t(a) : u3_none;

  return u3k(pro);              //  BUG: pro may be u3_none
}

/* a variable initialized inside the loop body survives the loop: the
** post-iteration exit joins the zero-iteration exit, and pre-loop
** Uninit adopts the initialized side
** @Refcount: retains `a`
*/
u3_noun
ok_loop_init(u3_noun a)
{
  u3_noun las;

  while ( u3_nul != a ) {
    las = u3h(a);
    a = u3t(a);
  }
  return u3k(las);
}

/* one path consumes the value, the other stashes it in an alias: the
** count survives through [b] on the else path (join count rescue)
*/
u3_noun
ok_consume_or_alias(u3_noun a)
{
  u3_noun b;

  if ( c3y == u3du(a) ) {
    u3z(a);
    b = u3_nul;
  }
  else {
    b = a;
  }
  return b;
}
