//
// Created by ADMIN on 02-Jan-24.
//

#ifndef XV6_PUBLIC_UNORDERED_MAP_H
#define XV6_PUBLIC_UNORDERED_MAP_H

#include "types.h"
//#include ""


typedef struct LinkedListHead {
  size_t length, capacity, listSize, dataSize;
  struct LinkedListNode * start;
} LinkedListHead;

typedef struct LinkedListNode {
  void * uniqueKey;
  void * data;
  struct LinkedListNode * next;
} LinkedListNode;

typedef struct UnorderedMap {
  size_t (* hashFunction ) (void *);
  size_t size;
  LinkedListHead * array;
} UnorderedMap;

UnorderedMap CreateUnorderedMap(size_t size,
                                LinkedListHead * defaultHead,
                                size_t (* hashFunction ) (void *)){
  UnorderedMap unorderedMap;
  unorderedMap.size = size;

  return unorderedMap;
};




#endif //XV6_PUBLIC_UNORDERED_MAP_H
