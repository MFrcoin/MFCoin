#ifndef ENTROPY_BIT_TABLE
#define ENTROPY_BIT_TABLE

// entropy bits of old blocks (pre-0.4), up to (and including) block 136999
// each uint32_t contains entropy bits of 32 blocks

extern int32_t vEntropyBits_number_of_blocks;
extern uint32_t vEntropyBits[];

#endif
