#ifndef U3_SHIP_H
#define U3_SHIP_H

#include "c3/c3.h"
#include "types.h"

typedef struct _u3_ship {
  c3_d hed_d;
  c3_d tel_d;
} u3_ship;

void
u3_ship_to_bytes(u3_ship who_u, c3_y len_y, c3_y* buf_y);

u3_ship
u3_ship_of_bytes(c3_y len_y, c3_y* buf_y);

u3_atom
u3_ship_to_noun(u3_ship who_u);

u3_ship
u3_ship_of_noun(u3_noun who);

c3_c*
u3_ship_to_string(u3_ship who_u);

c3_o
u3_ships_equal(u3_ship sip_u, u3_ship sap_u);

c3_l
u3_ship_rank(u3_ship who_u);

/**
* Returns a ship's galaxy byte prefix.
*/
c3_y
u3_ship_czar(u3_ship who_u);

/**
* Returns a ship's star prefix.
*/
c3_s
u3_ship_king(u3_ship who_u);

/**
* Returns a ship's planet prefix.
*/
c3_w
u3_ship_duke(u3_ship who_u);

/**
* Returns a ship's moon prefix.
*/
c3_d
u3_ship_earl(u3_ship who_u);

#endif /* ifndef U3_SHIP_H */
