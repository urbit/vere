/// @file

#include "vere.h"
#include "mdns.h"
#include "dns_sd.h"

typedef struct _mdns_payload {
  mdns_cb*      cb;
  DNSServiceRef sref;
  char          who[58];
  bool          fake;
  uint16_t      port;
  uv_poll_t     poll;
  void*         context;
} mdns_payload;

#ifdef ASAN_ENABLED
  void __lsan_ignore_object(const void *p);
#else
  #define __lsan_ignore_object(p) ((void) (p))
#endif

static void close_cb(uv_handle_t* poll) {
  mdns_payload* payload = (mdns_payload*)poll->data;
  DNSServiceRefDeallocate(payload->sref);
  c3_free(payload);
}

static void poll_cb(uv_poll_t* handle, int status, int events) {
  mdns_payload* payload = (mdns_payload*) handle->data;
  int err = DNSServiceProcessResult(payload->sref);
}

static void init_sref_poll(mdns_payload* payload) {
  int fd = DNSServiceRefSockFD(payload->sref);
  uv_loop_t* loop = uv_default_loop();
  payload->poll.data = (void*)payload;
  uv_poll_init(loop, &payload->poll, fd);
  uv_poll_start(&payload->poll, UV_READABLE, poll_cb);
}

static void query_record_cb(DNSServiceRef sref,
                            DNSServiceFlags f,
                            uint32_t iface,
                            DNSServiceErrorType err,
                            const char *fullname,
                            uint16_t rrtype,
                            uint16_t rrclass,
                            uint16_t rdlen,
                            const void *rdata,
                            uint32_t ttl,
                            void *context)
{
  mdns_payload* payload = (mdns_payload*)context;

  uv_poll_stop(&payload->poll);

  if (err != kDNSServiceErr_NoError) {
    u3l_log("mdns: dns query error %d", err);
  }
  else if ( (kDNSServiceType_A != rrtype) ||
            (kDNSServiceClass_IN != rrclass) ||
            (4 != rdlen) ||
            (0 == (f & kDNSServiceFlagsAdd)) ) {
    u3l_log("mdns: unexpected A record response for %s", payload->who);
  }
  else {
    c3_w saddr_w;

    memcpy(&saddr_w, rdata, sizeof(saddr_w));
    payload->cb(payload->who, payload->fake, saddr_w, payload->port, payload->context);
  }

  payload->poll.data = payload;
  uv_close((uv_handle_t*)&payload->poll, close_cb);
}

static void resolve_cb(DNSServiceRef sref,
                       DNSServiceFlags f,
                       uint32_t iface,
                       DNSServiceErrorType err,
                       const char *name,
                       const char *host,
                       uint16_t port,
                       uint16_t tl,
                       const unsigned char *t,
                       void *context)
{
  mdns_payload* payload = (mdns_payload*)context;

  uv_poll_stop(&payload->poll);

  if (err != kDNSServiceErr_NoError) {
    u3l_log("mdns: dns resolve error %d", err);
    payload->poll.data = payload;
    uv_close((uv_handle_t*)&payload->poll, close_cb);
    return;
  }

  payload->sref = sref;
  payload->port = port;

  const char *start = name;
  if (strncmp(name, "fake-", 4) == 0) {
    payload->fake = 1;
    start = name + 5;
  } else {
    payload->fake = 0;
  }

  payload->who[0] = '~';
  for (int i = 0; start[i] != '\0' && start[i] != '.' && i < 58; ++i)
  {
    payload->who[i+1] = start[i];
  }

  mdns_payload* query_payload = c3_calloc(sizeof *query_payload);
  memcpy(query_payload, payload, sizeof(*query_payload));

  DNSServiceErrorType query_err =
    DNSServiceQueryRecord(&query_payload->sref,
                          0,
                          iface,
                          host,
                          kDNSServiceType_A,
                          kDNSServiceClass_IN,
                          query_record_cb,
                          (void*)query_payload);

  if (query_err != kDNSServiceErr_NoError) {
    u3l_log("mdns: dns A query error %d", query_err);
    c3_free(query_payload);
  }

  payload->poll.data = payload;
  uv_close((uv_handle_t*)&payload->poll, close_cb);

  if (query_err == kDNSServiceErr_NoError) {
    init_sref_poll(query_payload);
  }
}

static void browse_cb(DNSServiceRef s,
                      DNSServiceFlags f,
                      uint32_t iface,
                      DNSServiceErrorType err,
                      const char* name,
                      const char* type,
                      const char* domain,
                      void* context)
{
  if (err != kDNSServiceErr_NoError) {
    u3l_log("mdns: service browse error %i", err);
    return;
  }

  if (f & kDNSServiceFlagsAdd) {	// Add
    // we are leaking payload because we don't know when we are done
    // browsing, luckily we only browse once
    mdns_payload* payload = (mdns_payload*)context;
    mdns_payload* payload_copy = c3_calloc(sizeof *payload_copy);

    // copy to prevent asynchronous thrashing of payload
    memcpy(payload_copy, payload, sizeof(mdns_payload));


    DNSServiceErrorType err =
      DNSServiceResolve(&payload_copy->sref, 0, iface,
                        name, type, domain, resolve_cb,
                        (void*)payload_copy);

    init_sref_poll(payload_copy);

    if (err != kDNSServiceErr_NoError) {
      u3l_log("mdns: dns service resolve error %i", err);
      payload_copy->poll.data = payload_copy;
      uv_close((uv_handle_t*)&payload_copy->poll, close_cb);
    }
  }
}

static void register_close_cb(uv_handle_t* poll) {
  // not freeing sref with DNSServiceRefDeallocate since that
  // deregisters us from mdns
  mdns_payload* payload = (mdns_payload*)poll->data;
  c3_free(payload);
}


static void register_cb(DNSServiceRef sref,
                        DNSServiceFlags f,
                        DNSServiceErrorType err,
                        const char* name,
                        const char* type,
                        const char* domain,
                        void* context)
{
  mdns_payload* payload = (mdns_payload*)context;

  if (err != kDNSServiceErr_NoError) {
    u3l_log("mdns: service register error %i", err);
  } else {
    u3l_log("mdns: %s registered on all interfaces", name);
  }
  uv_poll_stop(&payload->poll);
  uv_close((uv_handle_t*)&payload->poll, register_close_cb);
}

void mdns_init(uint16_t port, bool fake, char* our, mdns_cb* cb, void* context)
{
  #if defined(U3_OS_linux)
  setenv("AVAHI_COMPAT_NOWARN", "1", 0);
  setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket", 0);
  #   endif

  mdns_payload* register_payload = (mdns_payload*)c3_calloc(sizeof(mdns_payload));
  __lsan_ignore_object(register_payload);

  DNSServiceErrorType err;

  char* domain;
  char s[63] = {0};
  if (fake) {
    strcat(s, "fake-");
    strcat(s, our + 1); // certain url parsers don't like the ~
    domain = s;
  } else {
    domain = our + 1;
  }

  err = DNSServiceRegister(&register_payload->sref, 0, 0, domain, "_ames._udp",
                           NULL, NULL, htons(port), 0, NULL, register_cb, (void*)register_payload);

  if (err != kDNSServiceErr_NoError) {
    if (err == kDNSServiceErr_Unknown) {
      #if defined(U3_OS_linux)
      u3l_log("mdns: init failed, install avahi on this system for mdns support");
      #else
      u3l_log("mdns: service register error %i", err);
      #   endif
    } else {
      u3l_log("mdns: service register error %i", err);
    }
    DNSServiceRefDeallocate(register_payload->sref);
    return;
  }

  init_sref_poll(register_payload);

  mdns_payload* browse_payload = (mdns_payload*)c3_calloc(sizeof(mdns_payload));

  browse_payload->cb = cb;
  browse_payload->context = context;

  DNSServiceErrorType dnserr;

  dnserr = DNSServiceBrowse(&browse_payload->sref, 0, 0, "_ames._udp", NULL, browse_cb, (void *)browse_payload);

  if (dnserr != kDNSServiceErr_NoError) {
    u3l_log("mdns: service browse error %i", dnserr);
    DNSServiceRefDeallocate(browse_payload->sref);
    return;
  }

  init_sref_poll(browse_payload);

}
