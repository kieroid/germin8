#pragma once

#include <stdbool.h>

// Boot the pwned phone into m1n1 from the images built into the firmware.
// in_recovery skips the iBSS, for a phone whose iBSS is already up.
int boot_run(bool in_recovery);

// Is the phone already in iBSS's recovery mode
bool boot_in_recovery(void);
