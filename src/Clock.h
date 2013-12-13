/**
 * @file
 * Definition of system clock interface.
 */

/******************************************************************************
 * Copyright (c) 2013, doubleTwist Corporation and AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#ifndef _CLOCK_H
#define _CLOCK_H

#include <stdint.h>
#include <time.h>

namespace ajn {
namespace services {

#ifdef CLOCK_REALTIME
__inline__ uint64_t GetCurrentTimeNanos() {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000) + ts.tv_nsec;
}
__inline__ void SleepNanos(uint64_t nanos) {
    struct timespec req;
    struct timespec rem;
    req.tv_sec = nanos / 1000000000;
    req.tv_nsec = nanos % 1000000000;
    nanosleep(&req, &rem);
}
#endif /* CLOCK_REALTIME */

}
}

#endif /* _CLOCK_H */
