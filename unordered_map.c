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
  initlock(&map->lock, "unordered_map");
  map->size = sizeof(map->bins) / sizeof(LinkedListHead *);
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

extern char end[];
size_t SwapMapHash(const UnorderedMap *map, const SwapUniqueKey *key) {

  cprintf("SwapMapHash: la: 0x%x, pa: 0x%x\n", key->log_a/ PGSIZE,  (key->pa - V2P(end))/ PGSIZE);
  return (key->log_a / PGSIZE + (key->pa - V2P(end))/ PGSIZE) % map->size;
}

void SwapMapInit(UnorderedMap *map) {
  UnorderedMapInit(map, &defaultSwapLL, (size_t (*)(const UnorderedMap *, const void *)) SwapMapHash);
}

//void SwapLinkedListRemove()

void SwapMapAdd(const void *key, pte_t **ppte, pte_t **pte) {
  acquire(&swapMap.lock);
  LinkedListHead *bin = UnorderedMapGetBin(&swapMap, key);
  LinkedListNode *node = LinkedListGet(bin, key);

  if (node == NULL) {
    LinkedListAdd(bin, key, NULL);
    node = bin->end;
    SwapDataAddPTE(node->data, NULL, pte);
  } else {
    while (!SwapDataAddPTE(node->data, ppte, pte)) {
      node = LinkedListNodeGetNextMatching(node, bin, key);
      if (node == NULL) {
        LinkedListAdd(bin, key, NULL);
        node = bin->end;
        SwapDataAddPTE(node->data, NULL, pte);
        break;
      }
    }
  }

  release(&swapMap.lock);
}

void SwapMapRemove(const void *key, pte_t **pte) {
  acquire(&swapMap.lock);
  LinkedListHead *bin = UnorderedMapGetBin(&swapMap, key);
  LinkedListNode *node = LinkedListGet(bin, key);
  while (!SwapDataRemovePTE(node->data, pte)) {
    node = LinkedListNodeGetNextMatching(node, bin, key);
  }
  if (((SwapData *) node->data)->PTEs->length == 0) {
    LinkedListNodeRemoveNextMatching(node, bin, NULL);
  }
  release(&swapMap.lock);
}

void swapmap_add_remove_test() {

  SwapMapInit(&swapMap);
  cprintf("map created successfully\n");
  pte_t a = 1, b = 2;
  pte_t *pte = &a, *ppte = &b;
  uint pa = NULL;
  uint log_a = NULL;
  SwapUniqueKey key = {.pa = pa, .log_a = log_a};

  cprintf("map size: %d\n", swapMap.size);
  {
    acquire(&swapMap.lock);
    LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);

    cprintf("swapmap: accessed a bin successfully\n");

    LinkedListNode *node = LinkedListGet(bin, &key);

    cprintf("swapmap: retrieved node at 0x%x successfully\n", node);
    if (node == NULL) {
      LinkedListAdd(bin, &key, NULL);
      node = bin->end;
      SwapDataAddPTE(node->data, NULL, &pte);
    } else {
      while (!SwapDataAddPTE(node->data, &ppte, &pte)) {
        node = LinkedListNodeGetNextMatching(node, bin, &key);
        if (node == NULL) {
          LinkedListAdd(bin, &key, NULL);
          node = bin->end;
          SwapDataAddPTE(node->data, NULL, &pte);
          break;
        }
      }
    }


//    if (node == NULL) {
//      LinkedListAdd(bin, &key, NULL);
//      if (!SwapDataAddPTE(data, NULL, &pte)) {
//        panic("swapmap: did not add pte\n");
//      }
//    } else {

//    }

    release(&swapMap.lock);
    cprintf("swapmap: added successfully\n");
  }
  {
    acquire(&swapMap.lock);

    LinkedListHead *bin = UnorderedMapGetBin(&swapMap, &key);
    cprintf("swapmap: accessed a bin successfully\n");
    LinkedListNode *node = LinkedListGet(bin, &key);
    if (node == NULL) {
      panic("swapmap: removing twice\n");
    }
    while (!SwapDataRemovePTE(node->data, &pte)) {
      node = LinkedListNodeGetNextMatching(node, bin, &key);
    }
    if (((SwapData *) node->data)->PTEs->length == 0) {
      LinkedListNodeRemoveNextMatching(node, bin, NULL);
    }

    release(&swapMap.lock);
    cprintf("swapmap: removed successfully\n");


  }

}
