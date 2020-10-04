#ifndef __COMMAND_H__
#define __COMMAND_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "storage.h"

storage_t *do_get(char *key);
int do_set(char *key, char *value);
int do_update(char *key, char *value);
int do_delete(char *key);

#ifdef __cplusplus
}
#endif

#endif /* __COMMAND_H__ */
