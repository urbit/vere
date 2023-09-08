/// @file

#include "vere.h"
#include "mdns.h"
#include "dns_sd.h"

typedef struct _mdns_payload {
  mdns_cb*      cb;
  DNSServiceRef sref;
  char          who[58];
  uint16_t      port;
  uv_poll_t     poll;
  void*         context;
} mdns_payload;


static void getaddrinfo_cb(uv_getaddrinfo_t* req, int status, struct addrinfo* res) {
  mdns_payload* payload = (mdns_payload*)req->data;

  if (status < 0) {
    u3l_log("mdns: getaddrinfo error: %s", uv_strerror(status));
    DNSServiceRefDeallocate(payload->sref);
    c3_free(payload);
    c3_free(req);
    uv_freeaddrinfo(res);
    return;
  }

  struct sockaddr_in* addr = (struct sockaddr_in*)res->ai_addr;
  payload->cb(payload->who, addr->sin_addr.s_addr, payload->port, payload->context);

  uv_freeaddrinfo(res);
  c3_free(req);
  DNSServiceRefDeallocate(payload->sref);
  c3_free(payload);

}

static void resolve_cb(DNSServiceRef sref,
                       DNSServiceFlags f,
                       uint32_t interface,
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
    c3_free(payload);
    DNSServiceRefDeallocate(sref);
    return;
  }

  payload->sref = sref;
  payload->port = port;

  int i;
  payload->who[0] = '~';
  for (i = 0; name[i] != '\0' && name[i] != '.' && i < 58; ++i)
  {
    payload->who[i+1] = name[i];
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET; // Request only IPv4 addresses
  hints.ai_socktype = SOCK_STREAM; // TCP socket

  uv_getaddrinfo_t* req = (uv_getaddrinfo_t*)c3_malloc(sizeof(uv_getaddrinfo_t));
  req->data = (void*)payload;

  uv_loop_t* loop = uv_default_loop();
  int error = uv_getaddrinfo(loop, req, getaddrinfo_cb, host, NULL, &hints);

  if (error < 0) {
    u3l_log("mdns: getaddrinfo error: %s\n", uv_strerror(error));
    c3_free(payload);
    c3_free(req);
    DNSServiceRefDeallocate(sref);
  }
}

static void poll_cb(uv_poll_t* handle, int status, int events) {
  DNSServiceRef sref = (DNSServiceRef) handle->data;
  int err = DNSServiceProcessResult(sref);
}

static void init_sref_poll(DNSServiceRef sref, mdns_payload* payload) {
  int fd = DNSServiceRefSockFD(sref);
  uv_loop_t* loop = uv_default_loop();
  payload->poll.data = (void*)sref;
  uv_poll_init(loop, &payload->poll, fd);
  uv_poll_start(&payload->poll, UV_READABLE, poll_cb);
}

static void browse_cb(DNSServiceRef s,
                      DNSServiceFlags f,
                      uint32_t interface,
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
    DNSServiceRef sr;

    mdns_payload* payload = (mdns_payload*)context;
    mdns_payload* payload_copy = c3_malloc(sizeof *payload_copy);

    // copy to prevent asynchronous thrashing of payload
    memcpy(payload_copy, payload, sizeof(mdns_payload));

    DNSServiceErrorType err =
      DNSServiceResolve(&sr, 0, interface,
                        name, type, domain, resolve_cb,
                        (void*)payload_copy);

    if (err != kDNSServiceErr_NoError) {
      u3l_log("mdns: dns service resolve error %i", err);
      c3_free(payload_copy);
      DNSServiceRefDeallocate(sr);
      return;
    }
    init_sref_poll(sr, payload_copy);
  }
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
  // not freeing sref with DNSServiceRefDeallocate since that
  // deregisters us from mdns
  uv_poll_stop(&payload->poll);
  c3_free(payload);
}

void mdns_init(uint16_t port, char* our, mdns_cb* cb, void* context)
{
  #if defined(U3_OS_linux)
  setenv("AVAHI_COMPAT_NOWARN", "1", 0);
  setenv("DBUS_SYSTEM_BUS_ADDRESS", "unix:path=/var/run/dbus/system_bus_socket", 0);
  #   endif

  memmove(our, our+1, strlen(our)); // certain url parsers don't like the sig

  mdns_payload* register_payload = (mdns_payload*)c3_malloc(sizeof(mdns_payload));

  DNSServiceRef sref;
  DNSServiceErrorType err;
  err = DNSServiceRegister(&sref, 0, 0, our, "_ames._udp",
                           NULL, NULL, htons(port), 0, NULL, register_cb, (void*)register_payload);

  if (err != kDNSServiceErr_NoError) {
    u3l_log("mdns: service register error %i", err);
    DNSServiceRefDeallocate(sref);
    return;
  }

  init_sref_poll(sref, register_payload);

  mdns_payload* browse_payload = (mdns_payload*)c3_malloc(sizeof(mdns_payload));

  browse_payload->cb = cb;
  browse_payload->context = context;

  DNSServiceErrorType dnserr;
  DNSServiceRef sref2;

  dnserr = DNSServiceBrowse(&sref2, 0, 0, "_ames._udp", NULL, browse_cb, (void *)browse_payload);

  if (dnserr != kDNSServiceErr_NoError) {
    u3l_log("mdns: service browse error %i", dnserr);
    DNSServiceRefDeallocate(sref2);
    return;
  }

  init_sref_poll(sref2, browse_payload);

}
