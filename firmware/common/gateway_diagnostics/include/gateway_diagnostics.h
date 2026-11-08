#pragma once
#include <stdbool.h>

/* Optional, one-shot connectivity probe on its own task. */
void gateway_diagnostics_start(bool station);
