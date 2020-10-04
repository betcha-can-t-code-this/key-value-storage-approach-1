#define _GNU_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "crc32.h"
#include "storage.h"

storage_t *storage = NULL;

static uint32_t storage_generate_hash(char *key)
{
	return crc32_hash(key);
}

static void storage_set_parent(storage_t *storage, storage_t *parent)
{
	storage->parent = parent;
}

static storage_t *storage_get_minimum_node(storage_t *storage)
{
	if (storage->left != NULL) {
		storage = storage_get_minimum_node(storage->left);
	}

	return storage;
}

static void storage_transplant_node(storage_t **storage, storage_t *current, storage_t *next)
{
	if (current->parent == NULL) {
		*(storage) = next;
		return;
	}

	if (current == current->parent->left) {
		current->parent->left = next;
	}

	if (current == current->parent->right) {
		current->parent->right = next;
	}

	if (next != NULL) {
		next->parent = current->parent;
	}
}

static void storage_destroy_single_node(storage_t *storage)
{
	free(storage->key);
	free(storage->value);
	free(storage);
}

static storage_t *storage_create(char *key, char *value)
{
	storage_t *storage = malloc(sizeof(storage_t));

	if (storage == NULL) {
		return NULL;
	}

	storage->hash = storage_generate_hash(key);
	storage->key = strdup(key);
	storage->value = strdup(value);

	storage->left = NULL;
	storage->right = NULL;
	storage->parent = NULL;

	return storage;
}

static void __storage_insert_node(storage_t **storage, char *key, char *value)
{
	if (*(storage) == NULL) {
		assert((*(storage) = storage_create(key, value)) != NULL);
		return;
	}

	uint32_t hash = storage_generate_hash(key);

	// integer comparison is faster :)
	if (hash == (*(storage))->hash) {
		// free previous allocated memory
		// for 'key' and 'value' in
		// storage structure.
		free((*(storage))->key);
		free((*(storage))->value);

		(*(storage))->key = strdup(key);
		(*(storage))->value = strdup(value);
		return;
	}

	if (hash < (*(storage))->hash) {
		__storage_insert_node(&(*(storage))->left, key, value);
		storage_set_parent((*(storage))->left, *(storage));		
	} else {
		__storage_insert_node(&(*(storage))->right, key, value);
		storage_set_parent((*(storage))->right, *(storage));
	}
}

void storage_insert_node(char *key, char *value)
{
	__storage_insert_node(&storage, key, value);
}

static storage_t *__storage_search_node(storage_t *storage, char *key)
{
	if (storage == NULL) {
		return NULL;
	}

	uint32_t hash = storage_generate_hash(key);

	if (hash == storage->hash) {
		return storage;
	}

	return hash < storage->hash
		? __storage_search_node(storage->left, key)
		: __storage_search_node(storage->right, key);
}

storage_t *storage_search_node(char *key)
{
	return __storage_search_node(storage, key);
}

static void __storage_delete_node(storage_t **storage, char *key)
{
	storage_t *tmp, *x;

	if ((tmp = __storage_search_node(*(storage), key)) == NULL) {
		return;
	}

	if (tmp->left == NULL) {
		storage_transplant_node(storage, tmp, tmp->right);
		goto done;
	}

	if (tmp->right == NULL) {
		storage_transplant_node(storage, tmp, tmp->left);
		goto done;
	}

	x = storage_get_minimum_node(tmp->right);

	if (x->parent != tmp) {
		storage_transplant_node(storage, x, x->right);
		x->right = tmp->right;
		x->right->parent = x;
	}

	storage_transplant_node(storage, tmp, x);
	x->left = tmp->left;
	x->left->parent = x;

done:
	storage_destroy_single_node(tmp);
	return;
}

void storage_delete_node(char *key)
{
	__storage_delete_node(&storage, key);
}

static void __storage_destroy(storage_t **storage)
{
	if (*(storage) == NULL) {
		return;
	}

	__storage_destroy(&(*(storage))->left);
	__storage_destroy(&(*(storage))->right);
	storage_destroy_single_node(*(storage));
}

void storage_destroy(void)
{
	__storage_destroy(&storage);
}
