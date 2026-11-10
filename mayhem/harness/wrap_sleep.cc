// Linked with -Wl,--wrap=nanosleep. std::this_thread::sleep_for, which the `Sleep` request uses for up
// to 50 s, compiles to a nanosleep call; the wrapper skips the wait so a sleeping batch does not trip
// the fuzzer's per-input timeout. Time has no effect on what the request handlers compute.
#include <sched.h>
#include <time.h>

extern "C" int __wrap_nanosleep(const struct timespec *, struct timespec *rem)
{
	sched_yield();
	if (rem) {
		rem->tv_sec = 0;
		rem->tv_nsec = 0;
	}
	return 0;
}
