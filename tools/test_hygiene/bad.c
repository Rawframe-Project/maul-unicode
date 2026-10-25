// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Fixture for tools/check_test_hygiene.py --self-test: each line marked
// "expect" must be reported with that rule, and no other line. It is
// read, never compiled.

#include <stdlib.h>
#include <threads.h>
#include <time.h>

int Bad(void);

int Bad(void)
{
    sleep(1);                     // expect: sleep
    nanosleep(nullptr, nullptr);  // expect: sleep
    thrd_sleep(nullptr, nullptr); // expect: sleep
    time_t now = time(nullptr);   // expect: wall-clock
    struct timespec real;
    clock_gettime(CLOCK_REALTIME, &real);         // expect: wall-clock
    timespec_get(&real, TIME_UTC);                // expect: wall-clock
    for (int retries = 0; retries < 3; retries++) // expect: retry
    {
    }
    // hygiene:
    exit(77);     // expect: skip
    GTEST_SKIP(); // expect: skip
    (void)now;
    return 77; // expect: skip
}
