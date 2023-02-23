/// @file

#include "vere.h"
#include <sys/ioctl.h>
#include "noun.h"

/* u3_device: description of a device
 */
  typedef struct _u3_device {
    c3_w       nam_w;                   //  %name of device
    c3_c*      fil_c;                   //  file of device
    c3_w       fid_w;                   //  file descriptor
    c3_o       opn_o;                   //  is the file open
    c3_w       tus_w;                   //  device status
  } u3_device;

/* u3_loch: a list of devices
*/
  typedef struct _u3_loch {
    u3_auto    car_u;                   //  driver
    c3_w       cnt_w;                   //  count of devices
    u3_device* dev_u;                   //  list of devices
  } u3_loch;

//  XX review, move
//
/* _behn_bail_dire(): c3y if fatal error. RETAIN
*/
static c3_o
_behn_bail_dire(u3_noun lud)
{
  u3_noun mot = u3r_at(4, lud);

  if (  (c3__meme == mot)
     || (c3__intr == mot) )
  {
    return c3n;
  }

  return c3y;
}

/* _lock_read_bail(): figure out why loch failed to read and return error code
*/
static void
_loch_read_bail(u3_ovum* egg_u, u3_noun lud)
{
  u3l_log("loch: read failed");
}

/* u3_loch_ef_read(): read from device
*/
static void
_loch_ef_read(u3_loch* teh_u, u3_noun wir_i, u3_device dev_d, u3_noun wut, u3_noun cmd)
{
  if ( c3n == teh_u->car_u.liv_o ) {
    teh_u->car_u.liv_o = c3y;
  }
  c3_y* buf_y = malloc(cmd*sizeof(c3_y));
  u3l_log("loch cmd: %d", cmd);
  u3l_log("loch fid: %d", dev_d.fid_w);
  
  read(dev_d.fid_w, buf_y, cmd);


  u3_noun red = u3i_bytes(cmd, buf_y);
  u3l_log("red: %llu", red);
  // extract read count and device from cmd noun
  // preform read on device and store in c3_y*
  // turn that c3_y* into some u3_noun
  // send read event with u3_noun to arvo
  //[%turn =dev =act dat=@ud]

  {
    u3_noun wir = u3nc(c3__loch, u3_nul);
    u3_noun dat = u3nc(dev_d.nam_w, red);
    u3_noun cad = u3nc(c3__seen, dat);

    u3_auto_peer(
      u3_auto_plan(&teh_u->car_u, u3_ovum_init(0, c3__l, wir, cad)),
      0, 0, _loch_read_bail);
  }

  //u3z(cmd);
}

/* _loch_born_news(): initialization complete on %born.
*/
static void
_loch_born_news(u3_ovum* egg_u, u3_ovum_news new_e)
{
  u3l_log("loch: %%born win;");
  u3_auto* car_u = egg_u->car_u;

  if ( u3_ovum_done == new_e ) {
    car_u->liv_o = c3y;
  }
}

/* _loch_born_bail(): %born is essential, retry failures.
*/
static void
_loch_born_bail(u3_ovum* egg_u, u3_noun lud)
{

  u3l_log("loch: %%born failure;");
  u3z(lud);
  u3_ovum_free(egg_u);
}
/* _loch_io_talk(): notify %loch that we're live
 *                  pass it a list of device names
*/
static void
_loch_io_talk(u3_auto* car_u)
{
  u3_loch* loc_u = (u3_loch*)car_u;
  u3_noun  wir = u3nc(c3__loch, u3_nul);
  u3_noun  cad = u3nc(c3__born, u3_nul);

  u3_auto_peer(
    u3_auto_plan(car_u, u3_ovum_init(0, c3__l, wir, cad)),
    0,
    _loch_born_news,
    _loch_born_bail);
  // [1 %ovum [%l /test %devs [%i2c %.y]]]" 
  for( c3_w i = 0; i< loc_u->cnt_w; i++ ) {
    u3_noun  wir = u3nt(c3__loch,
                        u3dc("scot", c3__uv, i+1),
                        u3_nul);
    u3_noun  cad = u3nc(c3__devs, 
                        u3nc(loc_u->dev_u[i].nam_w, loc_u->dev_u[i].tus_w));
    u3_auto_peer(
      u3_auto_plan(car_u, u3_ovum_init(0, c3__l, wir, cad)),
      0,
      _loch_born_news,
      _loch_born_bail);
                        
  }
  
  car_u->liv_o = c3y;
}

/* _loch_io_kick(): apply effects.
*/
static c3_o
_loch_io_kick(u3_auto* car_u, u3_noun wir, u3_noun cad)
{
  u3_loch* teh_u = (u3_loch*)car_u;

  u3_noun tag, i_wir, par;
  u3_noun wut, dev, cmd, dat, tmp;
  u3_device* dev_d;
  c3_o ret_o, dev_o;

  if (  (c3n == u3r_cell(wir, &i_wir, 0))
     || (c3n == u3r_cell(cad, &tag, &par))
     || (c3__loch != i_wir) )
  {
    ret_o = c3n;
  }
  else {
    if (  (c3n == u3r_cell(par, &wut, &tmp))
       || (c3n == u3r_cell(tmp, &dev, &cmd)))
    {
      ret_o = c3n;
    }
    else {
      //See if the device is present and open
      dev_o = c3n;
      for( c3_w i = 0; i< teh_u->cnt_w; i++ ) {
        if(  (dev == teh_u->dev_u[i].nam_w)
          && (c3y == teh_u->dev_u[i].opn_o)) {
            u3l_log("loch dev found");
            dev_o = c3y;
            dev_d = &teh_u->dev_u[i];
        }
      }
      if (c3y == dev_o) {
        if ( (c3__read == tag) )
        {
          u3l_log("loch read");
          _loch_ef_read(teh_u, wir, *dev_d, wut, cmd); // execute read command
          ret_o = c3y;
        }
        else if ( c3__writ == tag )
        {
          u3l_log("loch: writ ");
        }
      }
      else {
        ret_o = c3n;
      }
    }
  }

  u3z(wir); u3z(cad);
  return ret_o;
}


/* _loch_io_exit(): close all open device files and exit
*/
static void
_loch_io_exit(u3_auto* car_u)
{
  u3_loch* teh_u = (u3_loch*)car_u;

  for( c3_w i = 0; i< teh_u->cnt_w; i++ ) {
    if( c3y == teh_u->dev_u[i].opn_o ) {
      close(teh_u->dev_u[i].fid_w);
    }
    //c3_free(&teh_u->dev_u[i]);
  }
  //c3_free(&teh_u->dev_u);
  c3_free(teh_u);
}

/* u3_loch(): initialize hardware control vane.
*/
u3_auto*
u3_loch_io_init(u3_pier* pir_u)
{
  u3_loch* teh_u = c3_calloc(sizeof(*teh_u));

  //  TODO: Open all devices specified in pier.
  //  Right now this only opens /dev/random and will provide a random number
  u3_device* random = c3_calloc(sizeof(random));

  random->nam_w = c3_s4('r','a','n','d');
  random->fil_c = "/dev/urandom";

  teh_u->cnt_w = 1;
  teh_u->dev_u = c3_calloc(teh_u->cnt_w*sizeof(teh_u));
  teh_u->dev_u[0] = *random;

  //  Open all devices
  for( c3_w i = 0; i< teh_u->cnt_w; i++ ) {
    teh_u->dev_u[i].fid_w = open(teh_u->dev_u[i].fil_c, O_RDWR);
    if ( teh_u->dev_u[i].fid_w  < 0 ) { //If any error report it and keep device marked as close
      u3l_log("Unable to open device file %s \n", teh_u->dev_u[i].fil_c);
    } else {
      teh_u->dev_u[i].opn_o = c3y;
      teh_u->dev_u[i].tus_w = 0;
    }
  }

  u3_auto* car_u = &teh_u->car_u;
  car_u->nam_m = c3__loch;

  car_u->liv_o = c3n;
  car_u->io.talk_f = _loch_io_talk;
  car_u->io.kick_f = _loch_io_kick;
  car_u->io.exit_f = _loch_io_exit;

  return car_u;
}
