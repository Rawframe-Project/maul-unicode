// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sirac Ozmen
//
// Fixture for tools/check_test_hygiene.py --self-test: nothing here may
// be reported. It is read, never compiled.

#include <time.h>

void Good(void);

void Good(void)
{
    // Words in comments do not count: sleep(1), time(NULL), retry.
    const char* text = "sleep(1); time(NULL); exit(77); retry";
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    clock_t used = clock();
    // hygiene: the fixture shows a marked sleep on the line after
    usleep(10);
    gettimeofday(nullptr, nullptr); // hygiene: a marked wall-clock read
    (void)text;
    (void)used;
}
