#include "command.h"
#include "storage.h"

storage_t *do_get(char *key)
{
	return storage_search_node(key);
}

int do_set(char *key, char *value)
{
	storage_insert_node(key, value);
	return 0;
}

int do_update(char *key, char *value)
{
	return do_set(key, value);
}

int do_delete(char *key)
{
	storage_delete_node(key);
	return 0;
}
