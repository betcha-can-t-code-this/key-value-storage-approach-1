#ifndef __STORAGE_H__
#define __STORAGE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

typedef struct storage {
	uint32_t hash;
	char *key;
	char *value;
	struct storage *left;
	struct storage *right;
	struct storage *parent;
} storage_t;

void storage_insert_node(char *key, char *value);
void storage_delete_node(char *key);
storage_t *storage_search_node(char *key);
void storage_destroy();

#ifdef __cplusplus
}
#endif

#endif /* __STORAGE_H__ */
