#include "lzfse.h"

extern unsigned char _end[];

void decompress_lzfse(const unsigned char *src, unsigned int srclen, unsigned char *dest, unsigned int destlen) {
    if(lzfse_decode_buffer(dest, destlen, src, srclen, &_end[65536]) != destlen)
        __asm__ __volatile__("bkpt #0");
}


void *memcpy(void *dest, const void *src, size_t size) {
    unsigned char *destbyte = dest;
    const unsigned char *srcbyte = src;

    while(size--) {
        *destbyte++ = *srcbyte++;
    }

    return dest;
}

void *memset(void *dest, int c, size_t size) {
    unsigned char *destbyte = dest;

    while(size--) {
        *destbyte++ = (unsigned char)c;
    }

    return dest;
}

int raise(int sig) {
    (void)sig;
    while(1)
        __asm__ __volatile__("bkpt #0");
}
