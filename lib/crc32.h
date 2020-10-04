#ifndef __CRC32_H__
#define __CRC32_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

uint32_t crc32_hash(char *buf);

#ifdef __cplusplus
}
#endif

#endif /* __CRC32_H__ */
