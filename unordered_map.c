//
// Created by ADMIN on 07-Jan-24.
//
#include "types.h"
#include "mmu.h"
#include "memlayout.h"
#include "defs.h"
#include "spinlock.h"
#include "unordered_map.h"
vtable_LinkedListNode vtable_Swap = {
    .dataAlloc = (void *) SwapDataCreate,
    .dataFree = (void (*)(void *)) SwapDataFree,
    .keyAlloc = (void *) SwapUniqueKeyCreate,
    .keyFree = (void (*)(void *)) SwapUniqueKeyFree,
};
const LinkedListHead defaultSwapLL = {
    .keySize = sizeof(SwapUniqueKey),
    .dataSize = sizeof(SwapData),
    .vtable = &vtable_Swap,
};
UnorderedMap swapMap;

void UnorderedMapInit(UnorderedMap *map, const LinkedListHead *defaultHead,
                      size_t (*hashFunction)(const struct UnorderedMap *, const void *)) {
  initlock(&map->lock, "map");
  map->size = sizeof(map->bins) / sizeof(map->bins[0]);
  map->hashFunction = hashFunction;
  map->defaultHead = defaultHead;
  for (size_t i = 0; i < map->size; ++i) {
    map->bins[i] = NULL;
  }
}

LinkedListHead *UnorderedMapGetBin(UnorderedMap *map, const void *key) {
  size_t i = map->hashFunction(map, key);
  if (map->bins[i] == NULL) {
    map->bins[i] = LinkedListAlloc();
    *map->bins[i] = *map->defaultHead;
  }

  return map->bins[i];
}


size_t SwapMapHash(const UnorderedMap *map, const SwapUniqueKey *key) {
  return (key->log_a + key->pa) % map->size;
}

void SwapMapInit(UnorderedMap *map) {
  UnorderedMapInit(map, &defaultSwapLL, (size_t (*)(const UnorderedMap *, const void *)) SwapMapHash);
}

//void SwapLinkedListRemove()
void swapmap_add_remove_test() {

  SwapMapInit(&swapMap);
  pte_t *pte, *ppte;
  uint pa = NULL;
  uint log_a = NULL;
  SwapUniqueKey key = {.pa = pa, .log_a = log_a};
//  SwapData data = {pte};
//  SwapDataCreate();
  {
    acquire(&swapMap.lock);
    LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);
    LinkedListNode *node = LinkedListGet(bin, &key);
    while (!SwapDataAddPTE(node->data, &ppte, &pte)) {
      node = LinkedListNodeGetNextMatching(node, bin, &key);
    }
    release(&swapMap.lock);

  }
  {
    acquire(&swapMap.lock);
    LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);
    LinkedListNode *node = LinkedListGet(bin, &key);
    while (!SwapDataRemovePTE(node->data, &pte)) {
      node = LinkedListNodeGetNextMatching(node, bin, &key);
    }
    if (((SwapData *) node->data)->PTEs->length == 0){
      LinkedListNodeRemoveNextMatching(node, bin, NULL);
    }

    release(&swapMap.lock);

  }

}
