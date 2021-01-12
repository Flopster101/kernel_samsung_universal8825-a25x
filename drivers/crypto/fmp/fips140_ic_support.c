#include "stdint.h"
#include "fips140_ic_support.h"

#pragma clang optimize off

#define FIPS_HMAC_SIZE 32
#define FIPS_CHUNKS_MAX_AMOUNT 2048
#define FIPS_SECTIONS_MAX_AMOUNT 48
#define FIPS_ANCHOR_SYMS_OFFS_MAX_AMOUNT FIPS_SECTIONS_MAX_AMOUNT

extern void *ret_addr_text(void); // fmp_test_init
extern void *ret_addr_rodata(void); // pass

void *fips140_get_anchor_addr(uint32_t i)
{
	switch(i) {
	case 0: return (void *)ret_addr_text();
	case 1: return (void *)ret_addr_rodata();
	default: return (void*)0;
	};
}

__attribute__ ((section(".rodata"), unused))
uint32_t fips140_anchor_offset[FIPS_SECTIONS_MAX_AMOUNT];

__attribute__ ((section(".rodata"), unused))
uint32_t fips140_sec_amount;

__attribute__ ((section(".rodata"), unused))
struct chunks_info fips140_chunks_info[FIPS_CHUNKS_MAX_AMOUNT];

__attribute__ ((section(".rodata"), unused))
uint32_t fips140_chunk_amount;

__attribute__ ((section(".rodata"), unused))
uint8_t fips140_builtime_hmac[FIPS_HMAC_SIZE];

#pragma clang optimize on

