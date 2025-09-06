#include "noun.h"
#include <stdbool.h>

typedef void mdns_cb(c3_c* ship, bool fake, c3_w saddr, c3_s port, void* context);

void mdns_init(uint16_t port, bool fake, char* our, mdns_cb* cb, void* context);
