#ifndef _NF_NATFEAT_H
#define _NF_NATFEAT_H

#ifdef __cplusplus
extern "C" {
#endif

uint32_t nf_get_id(uint32_t args);
int32_t nf_call(uint32_t args);


static inline uint32_t nf_getparameter(uint32_t args, int i)
{
    if (i < 0)
        return 0;

    return m68ki_read_32(args + i * 4);
}


static inline void Atari2Host_memcpy(void *_dst, uint32_t src, size_t count)
{
    unsigned char *dst = (unsigned char *)_dst;
    while (count > 0)
    {
        *dst++ = m68ki_read_8(src);
        src++;
        count--;
    }
}

static inline void Host2Atari_memcpy(uint32_t dst, const void *_src, size_t count)
{
    const unsigned char *src = (const unsigned char *)_src;
    while (count > 0)
    {
        m68ki_write_8(dst, *src++);
        dst++;
        count--;
    }
}

#ifdef __cplusplus
}
#endif

#endif
