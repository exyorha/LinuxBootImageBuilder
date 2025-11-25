#include "7zTypes.h"
#include "LzmaDec.h"

extern unsigned char _end[];

struct Allocator {
    ISzAlloc header;
    unsigned char *allocPtr;
};

static void *lzmaAlloc(ISzAllocPtr p, size_t size) {
    struct Allocator *this = (struct Allocator *)p;

    unsigned char *result = this->allocPtr;

    this->allocPtr += (size + 15) & ~15;

	return result;
}

static void lzmaFree(ISzAllocPtr p, void *address) {
	(void)p;
    (void)address;
}

[[noreturn]] static inline void fault(SRes value) {
    __asm__ __volatile__("1: bkpt #0; b 1b" :: "r"(value));
    __builtin_unreachable();
}

void decompress_lzma(const unsigned char *src, unsigned int srclen, unsigned char *dest, unsigned int destlen) {
    struct Allocator alloc = {
        .header = {
            .Alloc = lzmaAlloc,
            .Free = lzmaFree,
        },
        .allocPtr = (unsigned char *)(((unsigned int)&_end[65536] + 15) & ~15)
    };

    SizeT destLenVar = destlen;
    SizeT srcLenVar = srclen - LZMA_PROPS_SIZE;
    ELzmaStatus stat;

    SRes res = LzmaDecode(dest, &destLenVar, src + LZMA_PROPS_SIZE, &srcLenVar, src, LZMA_PROPS_SIZE,
                          LZMA_FINISH_END, &stat, &alloc.header);

    if(res != SZ_OK)
        fault(res);

    if(stat != LZMA_STATUS_FINISHED_WITH_MARK)
        fault(32 + stat);

    if(destLenVar != destlen)
        fault(64);

    if(srcLenVar != srclen - LZMA_PROPS_SIZE)
        fault(128);
}

void *memcpy(void *dest, const void *src, size_t size) {
    unsigned char *destbyte = dest;
    const unsigned char *srcbyte = src;

    while(size--) {
        *destbyte++ = *srcbyte++;
    }

    return dest;
}
