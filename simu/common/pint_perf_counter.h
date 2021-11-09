#pragma once

#ifndef _PINT_PERF_COUNTER_H__
#define _PINT_PERF_COUNTER_H__

//// Dcache counter API and Macro. Called by ncore.
//=================================================================================================
// mask for {dcache_range,stack_range,hit}.  csr<12h3a>[15:17]
#define DCACHE_COUNTER_MASK_BIT0 0x20000
#define DCACHE_COUNTER_MASK_BIT1 0x10000
#define DCACHE_COUNTER_MASK_BIT2 0x8000

// reference for {dcache_range,stack_range,hit}. csr<12h3a>[14:12]
#define DCACHE_COUNTER_RO 0x4000
#define DCACHE_COUNTER_STACK 0x2000
#define DCACHE_COUNTER_HIT 0x1000

// clear dcache counter. csr<12h3a>[11]
#define DCACHE_COUNTER_CLEAR 0x800

// enable dcache counter. csr<12h3a>[10]
#define DCACHE_COUNTER_ENABLE 0x400

// mask for opcode. csr<12h3a>[9:5]
#define DCACHE_COUNTER_OPCODE_MASK_BIT0 0x20
#define DCACHE_COUNTER_OPCODE_MASK_BIT1 0x40
#define DCACHE_COUNTER_OPCODE_MASK_BIT2 0x80
#define DCACHE_COUNTER_OPCODE_MASK_BIT3 0x100
#define DCACHE_COUNTER_OPCODE_MASK_BIT4 0x200

// mask for opcode. csr<12h3a>[4:0]
#define DCACHE_COUNTER_OPCODE_LS_SB 0x01
#define DCACHE_COUNTER_OPCODE_LS_SH 0x05
#define DCACHE_COUNTER_OPCODE_LS_SW 0x09
#define DCACHE_COUNTER_OPCODE_LS_WLINE 0x19
#define DCACHE_COUNTER_OPCODE_LS_RW 0x02
#define DCACHE_COUNTER_OPCODE_LS_RLINE 0x06
#define DCACHE_COUNTER_OPCODE_LS_RONCE 0xE
#define DCACHE_COUNTER_OPCODE_LS_FAA 0x03

// record dcache miss address.csr<12h3c>[14]
#define DCACHE_COUNTER_RECORD_LAST_MISS 0x4000

// Dcache preformance counter setting.
static inline void pint_dcache_counter_start(unsigned int cfg) {
    unsigned int old;
    asm volatile("csrr\t%0,0x3a" : "=r"(old));
    // set enable and clear bit
    old = (old & 0xFFFC0000) | (cfg | DCACHE_COUNTER_CLEAR | DCACHE_COUNTER_ENABLE);
    asm volatile("csrw\t0x3a,%0" ::"r"(old));
}

// get dcache counter value but don't stop the counter
static inline unsigned int pint_get_dcache_counter() {
    unsigned int cnt;
    asm volatile("csrr\t%0,0x3b" : "=r"(cnt));
    return (cnt & 0x00FFFFFF);
}

#define PINT_DCACHE_COUNTER_CFG                                                                    \
    (DCACHE_COUNTER_MASK_BIT0 | DCACHE_COUNTER_MASK_BIT1 | DCACHE_COUNTER_MASK_BIT2 |              \
     DCACHE_COUNTER_RO | /*DCACHE_COUNTER_HIT |*/                                                  \
     DCACHE_COUNTER_OPCODE_MASK_BIT0 | DCACHE_COUNTER_OPCODE_MASK_BIT1 |                           \
     DCACHE_COUNTER_OPCODE_MASK_BIT2 | DCACHE_COUNTER_OPCODE_MASK_BIT3 |                           \
     DCACHE_COUNTER_OPCODE_MASK_BIT4 |                                                             \
     DCACHE_COUNTER_OPCODE_LS_RW /*DCACHE_COUNTER_OPCODE_LS_RLINE*/                                \
     )

//// Counter(only miss) API of dcache request to shared cache(scache). Called by ncore.
//=================================================================================================

// record dcache miss address. csr<12h3c>[14]
#define SCACHE_COUNTER_RECORD_LSAT_MISS 0x4000
#define SCACHE_COUNTER_RECORD_FIRST_MISS 0x0

// prefetch select. csr<12h3c>[13:12]
#define SCACHE_COUNTER_PF_SELECT_NO 0x2000
#define SCACHE_COUNTER_PF_SELECT_ONLY 0x3000
#define SCACHE_COUNTER_PF_SELECT_BOTH 0x0

// clear/enable scache counter. csr<12h3c>[11:10]
#define SCACHE_COUNTER_CLEAR 0x800  // bit[11]
#define SCACHE_COUNTER_ENABLE 0x400 // bit[10]

// opcode kind. csr<12h3c>[9:0].
#define SCACHE_COUNTER_OPCODE_SB (0x3E0 | 0x01)    // store byte.
#define SCACHE_COUNTER_OPCODE_SH (0x3E0 | 0x05)    // store half word.
#define SCACHE_COUNTER_OPCODE_SW (0x3E0 | 0x09)    // store word.
#define SCACHE_COUNTER_OPCODE_SL (0x3E0 | 0x19)    // store cacheline.
#define SCACHE_COUNTER_OPCODE_RW (0x3E0 | 0x02)    // read word.
#define SCACHE_COUNTER_OPCODE_RL (0x3E0 | 0x06)    // read cacheline.
#define SCACHE_COUNTER_OPCODE_RONCE (0x3E0 | 0x0E) // read once.(read line and no shared caching)
#define SCACHE_COUNTER_OPCODE_FAA (0x3E0 | 0x03)   // fetch and add.
#define SCACHE_COUNTER_OPCODE_SALL (0x60 | 0x01)   // count all store opcodes.(SB/SH/SW/SL)
#define SCACHE_COUNTER_OPCODE_RALL (0x60 | 0x02)   // count all load opcodes.(RW/RL/RONCE)

// Dcache preformance counter setting.
static inline void pint_dcache2shared_counter_start(unsigned int cfg) {
    unsigned int old;
    asm volatile("csrr\t%0,0x3c" : "=r"(old));
    // set enable and clear bit
    old = (old & 0xFFFFF000) | (cfg | DCACHE_COUNTER_CLEAR | DCACHE_COUNTER_ENABLE);
    asm volatile("csrw\t0x3c,%0" ::"r"(old));
}

// get dcache2scache counter value but don't stop the counter
static inline unsigned int pint_get_dcache2shared_counter() {
    unsigned int cnt;
    asm volatile("csrr\t%0,0x3d" : "=r"(cnt));
    return (cnt & 0x00FFFFFF);
}

//// Counter(miss/hit) API of ncore request to dcache. Called by ncore.
//=================================================================================================

// record dcache miss address. csr<12h3c>[14]
#define SCACHE_COUNTER_RECORD_LSAT_MISS 0x4000
#define SCACHE_COUNTER_RECORD_FIRST_MISS 0x0

// prefetch select. csr<12h3c>[13:12]
#define SCACHE_COUNTER_PF_SELECT_NO 0x2000
#define SCACHE_COUNTER_PF_SELECT_ONLY 0x3000
#define SCACHE_COUNTER_PF_SELECT_BOTH 0x0

// clear/enable scache counter. csr<12h3c>[11:10]
#define SCACHE_COUNTER_CLEAR 0x800  // bit[11]
#define SCACHE_COUNTER_ENABLE 0x400 // bit[10]

// opcode kind. csr<12h3c>[9:0].
#define SCACHE_COUNTER_OPCODE_SB (0x3E0 | 0x01)    // store byte.
#define SCACHE_COUNTER_OPCODE_SH (0x3E0 | 0x05)    // store half word.
#define SCACHE_COUNTER_OPCODE_SW (0x3E0 | 0x09)    // store word.
#define SCACHE_COUNTER_OPCODE_SL (0x3E0 | 0x19)    // store cacheline.
#define SCACHE_COUNTER_OPCODE_RW (0x3E0 | 0x02)    // read word.
#define SCACHE_COUNTER_OPCODE_RL (0x3E0 | 0x06)    // read cacheline.
#define SCACHE_COUNTER_OPCODE_RONCE (0x3E0 | 0x0E) // read once.(read line and no shared caching)
#define SCACHE_COUNTER_OPCODE_FAA (0x3E0 | 0x03)   // fetch and add.
#define SCACHE_COUNTER_OPCODE_SALL (0x60 | 0x01)   // count all store opcodes.(SB/SH/SW/SL)
#define SCACHE_COUNTER_OPCODE_RALL (0x60 | 0x02)   // count all load opcodes.(RW/RL/RONCE)

#define PINT_DCACHE2SHARED_COUNTER_CFG                                                             \
    (SCACHE_COUNTER_RECORD_FIRST_MISS | SCACHE_COUNTER_PF_SELECT_BOTH | SCACHE_COUNTER_OPCODE_RALL)

#define Enable_PARALLEL_COUNT 0
#define Enable_DCACHE_COUNT 1
#define Enable_DCACHE2SCACHE_COUNT 2
#define Enable_DCACHE_AND_SCACHE_COUNT 3

#endif
