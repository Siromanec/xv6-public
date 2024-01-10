//
// Created by ADMIN on 02-Jan-24.
//

#ifndef XV6_PUBLIC_UNORDERED_MAP_H
#define XV6_PUBLIC_UNORDERED_MAP_H


#include "types.h"

#include "LinkedList.h"
#define SWAP_MAP_SIZE (PHYSTOP / PGSIZE)




typedef struct UnorderedMap {
  size_t (*hashFunction )(const struct UnorderedMap *map, const void *argstruct);
  struct spinlock lock;

  const LinkedListHead *defaultHead;

  size_t size;
  LinkedListHead *bins[SWAP_MAP_SIZE];
} UnorderedMap;






void UnorderedMapInit(UnorderedMap *map,
                      const LinkedListHead *defaultHead,
                      size_t (*hashFunction )(const struct UnorderedMap *, const void *argstruct));;

LinkedListHead *UnorderedMapGetBin(UnorderedMap *map, const void *key);;

size_t SwapMapHash(const UnorderedMap *map, const SwapUniqueKey *key);

void SwapMapInit(UnorderedMap *map);

void SwapMapAdd(const void *key, pte_t **ppte, pte_t **pte);
void SwapMapRemove(const void *key, pte_t **pte);

void swapmap_add_remove_test();

#endif //XV6_PUBLIC_UNORDERED_MAP_H
