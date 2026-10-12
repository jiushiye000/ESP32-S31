#pragma once
#include "esp_err.h"

/* Register once after console initialization and before starting the REPL.
 * Command callbacks use the same backend as the UI; registration errors are
 * returned to the application rather than swallowed or treated as success. */
esp_err_t bt_debug_register_commands(void);
