//
// Created by ADMIN on 08-Jan-24.
//

#ifndef XV6_PUBLIC_LINKEDLIST_H
#define XV6_PUBLIC_LINKEDLIST_H
typedef struct vtable_LinkedListNode {
  void *(*dataAlloc)(void);

  void (*dataFree)(void *);

  void *(*keyAlloc)(void);

  void (*keyFree)(void *);
} vtable_LinkedListNode;

//vtable_LinkedListNode default_vtable_LinkedListNode = {
//  .dataAlloc = lambda(void *, (), {return kmalloc(4)}),
//.dataFree = kmallocfree,
//};
typedef struct LinkedListHead {
  size_t length, keySize, dataSize;
  struct LinkedListNode *start;
  struct LinkedListNode *end;
  struct vtable_LinkedListNode *vtable;
} LinkedListHead;

typedef struct LinkedListNode {
  void *uniqueKey;
  void *data;
  struct LinkedListNode *next;
  struct LinkedListNode *prev;

} LinkedListNode;


LinkedListNode *LinkedListNodeAlloc(LinkedListHead *head);

void LinkedListNodeFree(LinkedListNode *node, LinkedListHead *head);

void LinkedListNodeInit(LinkedListNode *node, LinkedListHead *head, const void *uniqueKey, const void *data);

LinkedListNode *LinkedListNodeCreate(LinkedListHead *head, const void *uniqueKey, const void *data);;


void LinkedListAdd(LinkedListHead *head, const void *uniqueKey, const void *data);;

LinkedListNode *LinkedListNodeGetNextMatching(LinkedListNode *start, LinkedListHead *head, const void *uniqueKey);

LinkedListNode *LinkedListNodeRemoveNextMatching(LinkedListNode *start, LinkedListHead *head, const void *uniqueKey);

LinkedListNode *LinkedListGet(LinkedListHead *head, const void *uniqueKey);;

LinkedListNode * LinkedListRemove(LinkedListHead *head, void *uniqueKey);

LinkedListHead *LinkedListAlloc();

void LinkedListFree(LinkedListHead *list);


typedef struct SwapUniqueKey {
  uint log_a; // logical address
  uint pa; // physical pageno
} SwapUniqueKey;
typedef struct SwapData {
  uint swapfilePageNo;
  LinkedListHead *PTEs;
} SwapData;

SwapData *SwapDataAlloc();

void SwapDataFree(SwapData *swapData);

void SwapDataInit(SwapData *swapData);;

SwapData *SwapDataCreate();

SwapUniqueKey *SwapUniqueKeyAlloc();

void SwapUniqueKeyFree(SwapUniqueKey *uniqueKey);

void SwapUniqueKeyInit(SwapUniqueKey *uniqueKey);;

SwapUniqueKey *SwapUniqueKeyCreate();

// returns TRUE on success and FALSE on failure
BOOL SwapDataAddPTE(SwapData *swapData, pte_t **parentPTE, pte_t **PTE);

// returns TRUE on success and FALSE on failure
BOOL SwapDataRemovePTE(SwapData *swapData, pte_t **PTE);
#endif //XV6_PUBLIC_LINKEDLIST_H
