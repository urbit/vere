#include "noun.h"

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
*/
u3_noun
bug_overfree(u3_noun a)    // RETAINS a
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
*/
u3_noun
bug_smuggle(u3_noun a)  //  RETAINS a
{
  a = u3qa_inc(a);
  return u3_nul;                //  BUG: the product parked in [a] leaks
}
