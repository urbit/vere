/// @file
///
/// Bridge header file between the Rust-implemented [IO
/// drivers](https://github.com/urbit/io_drivers) and the rest of the runtime.
///
/// Any comment that references a Rust file (i.e. a file with a `.rs` extension)
/// refers to the https://github.com/urbit/io_drivers unless otherwise noted.

#ifndef U3_VERE_IO_H
#define U3_VERE_IO_H

#include "c3.h"

//==============================================================================
// Types
//==============================================================================

/// The return status of a driver. See the `Status` enum in `src/lib.rs`.
typedef enum {
  Success,
  BadSource,
  BadChannel,
  BadSink,
  NoRuntime,
  NoDriver,
} Status;

/// A jammed IO request sent to a driver.
typedef struct {
  /// Length of jammed request.
  c3_d len_d;

  /// Jammed request.
  c3_y* req_y;
} io_request;

/// A jammed IO response received from a driver.
typedef struct {
  /// Length of jammed response.
  c3_d len_d;

  /// Jammed response.
  c3_y resp_y[];
} io_response;

//==============================================================================
// Functions
//==============================================================================

/// Launch the HTTP client IO driver that is implemented in Rust. See
/// `http_client_run()` in `src/http/client.rs` for more.
c3_y
http_client_run(void);

#endif /* ifndef U3_VERE_IO_H */
