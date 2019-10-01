#include <stdint.h>

// mfc: needed when linking transaction.cpp in consensus library, since we are not going to pull real GetAdjustedTime from timedata.cpp
int64_t GetAdjustedTime()
{
    return 0;
}
