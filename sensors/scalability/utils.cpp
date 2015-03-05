#include <unistd.h>
#include <ctime>

int64_t timevalToNano(timeval const& t)
{
        return static_cast<int64_t>(t.tv_sec)*1000000000LL + static_cast<int64_t>(t.tv_usec)*1000;
}

int64_t getTimestamp()
{
        struct timespec t = { 0, 0 };
        clock_gettime(CLOCK_MONOTONIC, &t);
        return static_cast<int64_t>(t.tv_sec)*1000000000LL + static_cast<int64_t>(t.tv_nsec);
}

