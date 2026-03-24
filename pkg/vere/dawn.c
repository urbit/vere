/// @file
///
/// Ethereum-integrated pre-boot validation.

#include "vere.h"

#include "curl/curl.h"
#include "noun.h"
#include "uv.h"

/* _dawn_oct_to_buf(): +octs to uv_buf_t
*/
static uv_buf_t
_dawn_oct_to_buf(u3_noun oct)
{
  if ( c3n == u3a_is_cat(u3h(oct)) ) {
    exit(1);
  }

  c3_w len_w  = u3h(oct);
  c3_y* buf_y = c3_malloc(1 + len_w);
  buf_y[len_w] = 0;

  u3r_bytes(0, len_w, buf_y, u3t(oct));

  u3z(oct);
  return uv_buf_init((void*)buf_y, len_w);
}

/* _dawn_buf_to_oct(): uv_buf_t to +octs
*/
static u3_noun
_dawn_buf_to_oct(uv_buf_t buf_u)
{
  u3_noun len = u3i_words(1, (c3_w*)&buf_u.len);

  if ( c3n == u3a_is_cat(len) ) {
    exit(1);
  }

  return u3nc(len, u3i_bytes(buf_u.len, (const c3_y*)buf_u.base));
}

/* _dawn_post_json(): POST JSON to url_c
*/
static uv_buf_t
_dawn_post_json(c3_c* url_c, uv_buf_t lod_u)
{
  CURL *curl;
  CURLcode result;
  long cod_l;
  struct curl_slist* hed_u = 0;

  uv_buf_t buf_u = uv_buf_init(c3_malloc(1), 0);

  if ( !(curl = curl_easy_init()) ) {
    u3l_log("failed to initialize libcurl");
    exit(1);
  }

  hed_u = curl_slist_append(hed_u, "Accept: application/json");
  hed_u = curl_slist_append(hed_u, "Content-Type: application/json");
  hed_u = curl_slist_append(hed_u, "charsets: utf-8");

  //  XX require TLS, pin default cert?
  //
  u3K.ssl_curl_f(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url_c);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, king_curl_alloc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buf_u);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, hed_u);

  // note: must be terminated!
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, lod_u.base);

  result = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &cod_l);

  // XX retry?
  if ( CURLE_OK != result ) {
    u3l_log("failed to fetch %s: %s",
            url_c, curl_easy_strerror(result));
    exit(1);
  }
  if ( 300 <= cod_l ) {
    u3l_log("error fetching %s: HTTP %ld", url_c, cod_l);
    exit(1);
  }

  curl_easy_cleanup(curl);
  curl_slist_free_all(hed_u);

  return buf_u;
}

/* _dawn_get_jam(): GET a jammed noun from url_c
*/
static u3_noun
_dawn_get_jam(c3_c* url_c)
{
  CURL *curl;
  CURLcode result;
  long cod_l;

  uv_buf_t buf_u = uv_buf_init(c3_malloc(1), 0);

  if ( !(curl = curl_easy_init()) ) {
    u3l_log("failed to initialize libcurl");
    exit(1);
  }

  //  XX require TLS, pin default cert?
  //
  u3K.ssl_curl_f(curl);
  curl_easy_setopt(curl, CURLOPT_URL, url_c);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, king_curl_alloc);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&buf_u);

  result = curl_easy_perform(curl);
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &cod_l);

  // XX retry?
  if ( CURLE_OK != result ) {
    u3l_log("failed to fetch %s: %s",
            url_c, curl_easy_strerror(result));
    exit(1);
  }
  if ( 300 <= cod_l ) {
    u3l_log("error fetching %s: HTTP %ld", url_c, cod_l);
    exit(1);
  }

  curl_easy_cleanup(curl);

  //  throw away the length from the octs
  //
  u3_noun octs = _dawn_buf_to_oct(buf_u);
  u3_noun jammed = u3k(u3t(octs));
  u3z(octs);

  c3_free(buf_u.base);

  return u3ke_cue(jammed);
}

/* _dawn_gat_rpc(): ethereum JSON RPC with request/response as +octs
*/
static u3_noun
_dawn_gat_rpc_old(c3_c* url_c, u3_noun oct)
{
  uv_buf_t buf_u = _dawn_post_json(url_c, _dawn_oct_to_buf(oct));
  u3_noun    pro = _dawn_buf_to_oct(buf_u);

  c3_free(buf_u.base);

  return pro;
}

/* _dawn_fail(): pre-boot validation failed
*/
static void
_dawn_fail(u3_noun who, u3_noun rac, u3_noun sas)
{
  u3_noun how = u3dc("scot", 'p', u3k(who));
  c3_c* how_c = u3r_string(u3k(how));

  c3_c* rac_c;

  switch (rac) {
    default: u3_assert(0);
    case c3__czar: {
      rac_c = "galaxy";
      break;
    }
    case c3__king: {
      rac_c = "star";
      break;
    }
    case c3__duke: {
      rac_c = "planet";
      break;
    }
    case c3__earl: {
      rac_c = "moon";
      break;
    }
    case c3__pawn: {
      rac_c = "comet";
      break;
    }
  }

  u3l_log("boot: invalid keys for %s '%s'", rac_c, how_c);

  // XX deconstruct sas, print helpful error messages
  while ( u3_nul != sas ) {
    u3m_p("pre-boot error", u3h(sas));
    sas = u3t(sas);
  }

  u3z(how);
  c3_free(how_c);
  exit(1);
}

/* _dawn_need_unit(): produce a value or print error and exit
*/
static u3_noun
_dawn_need_unit(u3_noun nit, c3_c* msg_c)
{
  if ( u3_nul == nit ) {
    u3l_log("%s", msg_c);
    exit(1);
  }
  else {
    u3_noun pro = u3k(u3t(nit));
    u3z(nit);
    return pro;
  }
}

/* _dawn_turf(): override contract domains with -H
*/
static u3_noun
_dawn_turf(c3_c* dns_c)
{
  u3_noun tuf;

  u3_noun par = u3v_wish("thos:de-purl:html");
  u3_noun dns = u3i_string(dns_c);
  u3_noun rul = u3dc("rush", u3k(dns), u3k(par));

  if ( (u3_nul == rul) || (c3n == u3h(u3t(rul))) ) {
    u3l_log("boot: invalid domain specified with -H %s", dns_c);
    exit(1);
  }
  else {
    u3l_log("boot: overriding network domains with %s", dns_c);
    u3_noun dom = u3t(u3t(rul));
    tuf = u3nc(u3k(dom), u3_nul);
  }

  u3z(par); u3z(dns); u3z(rul);

  return tuf;
}

/* _dawn_sponsor(): retrieve sponsor from point
*/
static u3_noun
_dawn_sponsor(u3_noun who, u3_noun rac, u3_noun pot, c3_o azi_o)
{
  u3_noun uni = u3dt("sponsor:dawn", u3k(who), u3k(pot), azi_o);

  if ( c3n == u3h(uni) ) {
    _dawn_fail(who, rac, u3nc(u3t(uni), u3_nul));
    return u3_none;
  }

  u3_noun pos = u3k(u3t(uni));

  u3z(who); u3z(rac); u3z(pot); u3z(uni);

  return pos;
}

/* _dawn_is_az(): check whether azimuth ship
*/
static u3_noun
_dawn_is_az(u3_noun who, u3_noun fed)
{
  u3_noun rank = u3do("clan:title", who);

  if ( c3__pawn != rank ) {
    u3z(fed);
    return c3y;
  }

  u3_noun suite = u3do("suite:dawn", fed);

  if ( u3_nul == suite || c3__b == u3t(suite) ) {
    return c3y;
  }

  u3z(suite);
  return c3n;
}

#ifdef unsafe_dawn
/*  _dawn_feed_to_point: convert feed to point:jael
*/
static u3_noun
_dawn_feed_to_point(u3_noun fed)
{
  u3_noun pon = u3do("feed-to-point:dawn", fed);
  
  return pon;
}

/*  _dawn_lift_feed: convert feed [%2 ~], highest life
*/
static u3_noun
_dawn_lift_feed(u3_noun fed)
{
  u3_noun pon = u3do("lift-feed:dawn", fed);
  
  return pon;
}
#endif

/* u3_dawn_vent(): validated boot event
*/
u3_noun
u3_dawn_vent(u3_noun ship, u3_noun feed, u3_noun* rift)
{
  u3_noun fed, pos, pon, zar, tuf, src, sax;

  u3_noun rank = u3do("clan:title", u3k(ship));

  c3_o azi_o = _dawn_is_az(u3k(ship), u3k(feed));

  c3_c url_c[4096];

#ifdef unsafe_dawn
  {
    u3_noun put = _dawn_feed_to_point(u3k(feed));
    u3_noun pot = _dawn_need_unit(put, "boot: keyfile missing keys");
    pon = u3nc(u3nc(u3k(ship), pot), u3_nul);
    
    fed = _dawn_lift_feed(u3k(feed));
    if ( u3_nul == fed ) {
      u3l_log("boot: keyfile missing keys");
      _dawn_fail(ship, rank, u3_nul);
      return u3_none;
    }
  }
#else
  {
    //  +point:jael: gateway state
    //
    u3_noun pot;

    if ( c3__pawn == rank && c3y == azi_o ) {
      //  irrelevant, just bunt +point
      //
      pot = u3v_wish("*point:jael");
    }
    else  if ( c3__earl == rank ) {
      pot = u3v_wish("*point:jael");
    }
    else {
      u3l_log("boot: retrieving %s's public keys",
              u3_Host.ops_u.who_c);

      {
        sprintf(url_c, "%s/_~_/=pynt=/j/%s",
                u3_Host.ops_u.gat_c, u3_Host.ops_u.who_c);
        u3_noun top = u3_king_get_noun(url_c);

        pot = _dawn_need_unit(top, "boot: failed to retrieve public keys");
      }
    }

    //  +live:dawn: network state
    //  XX actually make request
    //
    u3_noun liv = u3_nul;
    // u3_noun liv = _dawn_get_json(parent, /some/url)

    u3l_log("boot: verifying keys");

    if ( c3y == u3a_is_cell(feed) && 
         c3n == u3a_is_cell(u3h(feed)) &&
         c3__earl == rank ) {
      // bails, won't return
      u3l_log("boot: incorrect keyfile please use updated format");
      _dawn_fail(ship, rank, u3_nul);
      return u3_none;
    }

    //  (each feed:jael (lest error=term))
    //
    fed = u3dq("veri:dawn", u3k(ship), u3k(feed), u3k(pot), u3k(liv));

    if ( c3n == u3h(fed) ) {
      // bails, won't return
      _dawn_fail(ship, rank, u3t(fed));
      return u3_none;
    }

    u3_assert(c3y == u3du(u3h(u3t(fed))) && u3h(u3h(u3t(fed))) == 2);
    *rift = u3k(u3h(u3t(u3t(u3t(fed)))));

    u3l_log("boot: getting sponsor");
    pos = _dawn_sponsor(u3k(ship), u3k(rank), u3k(pot), azi_o);
    u3z(pot); u3z(liv);
  }
#endif

  //  (map ship [=rift =life =pass]): galaxy table
  //
  {
    u3l_log("boot: retrieving galaxy table");

    sprintf(url_c, "%s/_~_/=lamp=/j",
            u3_Host.ops_u.gat_c);
    zar = u3_king_get_noun(url_c);
  }

  //  (list turf): ames domains
  //
  if ( 0 != u3_Host.ops_u.dns_c ) {
    tuf = _dawn_turf(u3_Host.ops_u.dns_c);
  }
  else {
    u3l_log("boot: retrieving network domains");

    sprintf(url_c, "%s/_~_/=turf=/j",
            u3_Host.ops_u.gat_c);
    tuf = u3_king_get_noun(url_c);
  }

#ifndef unsafe_dawn
  //  (list ship): %saxo sponsorship chain
  //
  {
    u3l_log("boot: retrieving sponsorship chain");
    u3_noun who = u3dc("scot", 'p', u3k(pos));
    c3_c* who_c = u3r_string(who);
    sprintf(url_c, "%s/_~_/=saxo=/j/%s",
            u3_Host.ops_u.gat_c, who_c);
    sax = u3_king_get_noun(url_c);
    
    // shouldn't occur as saxo includes the ship itself
    //
    if ( u3_nul == sax ) {
      u3l_log("boot: sponsorship chain empty");
      _dawn_fail(ship, rank, u3_nul);
      return u3_none;
    }

    u3z(who);
    c3_free(who_c);
  }

  pon = u3_nul;
  while (u3_nul != sax) {
    u3_noun son;
    //  print message
    //
    {
      u3_noun who = u3dc("scot", 'p', u3k(pos));
      c3_c* who_c = u3r_string(who);
      u3l_log("boot: retrieving keys for sponsor %s", who_c);
      u3z(who);
      c3_free(who_c);
    }

    //  retrieve +point:jael of pos (sponsor of ship)
    //
    {
      u3_noun top = u3dc("scot", c3__p, u3k(pos));
      c3_c* pot_c = u3r_string(top);
      u3z(top);
      sprintf(url_c, "%s/_~_/=pynt=/j/%s",
              u3_Host.ops_u.gat_c, pot_c);
      u3_noun nos = u3_king_get_noun(url_c);
      c3_free(pot_c);

      son = _dawn_need_unit(nos, "boot: failed to retrieve public keys");
      // append to sponsor chain list
      //
      pon = u3nc(u3nc(u3k(pos), son), pon);
    }
    
    // next sponsor
    //
    u3z(ship); u3z(rank);
    ship = pos;
    rank = u3do("clan:title", u3k(ship));
    sax = u3t(sax);
    if ( u3_nul != sax ) {
      pos = u3h(sax);
    }
  }
#endif
  
  if ( 0 != u3_Host.ops_u.src_c ) {
    src = u3v_wish(u3_Host.ops_u.src_c);
  }
  else {
    src = u3_nul;
  }

  //  [%dawn %1 seed sponsors galaxies domains eth-url sources]
  //
  u3_noun ven = u3nc(c3__dawn,
                     u3nq(1, u3k(u3t(fed)), pon, u3nq(zar, tuf, u3_nul, src)));

  u3z(fed); u3z(rank); u3z(ship); u3z(feed);
#ifndef unsafe_dawn
  u3z(pos); u3z(sax);
#endif

  return ven;
}

/* _dawn_come(): mine a comet under a list of stars
*/
static u3_noun
_dawn_come(u3_noun stars)
{
  u3_noun seed;
  {
    c3_w    eny_w[16];
    u3_noun eny;

    c3_rand(eny_w);
    eny = u3i_words(16, eny_w);

    u3l_log("boot: mining a comet. May take up to an hour.");
    u3l_log("If you want to boot faster, get an Urbit identity.");

    seed = u3dc("come:dawn", u3k(stars), u3k(eny));
    u3z(eny);
  }

  {
    /* [[%2 ~] who=ship ryf=rift kyz=(list [lyf=life key=ring])]  */
    u3_noun who = u3dc("scot", 'p', u3k(u3h(u3t(seed))));
    c3_c* who_c = u3r_string(who);

    u3l_log("boot: found comet %s", who_c);

  //  enable to print and save comet private key for future reuse
  //
#if 0
    {
      u3_noun key = u3dc("scot", c3__uw, u3qe_jam(seed));
      c3_c* key_c = u3r_string(key);

      u3l_log("boot: comet private key\n  %s", key_c);

      {
        c3_c  pat_c[64];
        snprintf(pat_c, 64, "%s.key", who_c + 1);

        FILE* fil_u = c3_fopen(pat_c, "w");
        fprintf(fil_u, "%s\n", key_c);
        fclose(fil_u);
      }

      c3_free(key_c);
      u3z(key);
    }
#endif

    c3_free(who_c);
    u3z(who);
  }

  u3z(stars);

  return seed;
}

/* u3_dawn_come(): mine a comet under a list of stars we download
*/
u3_noun
u3_dawn_come()
{
  return _dawn_come(
      _dawn_get_jam("https://bootstrap.urbit.org/comet-stars.jam"));
}
