//
// Created by ADMIN on 08-Jan-24.
//
#include "types.h"
#include "mmu.h"
#include "memlayout.h"
#include "defs.h"

#include "LinkedList.h"

LinkedListHead defaultLLPTE = {
    .keySize = sizeof(pte_t *)
};


LinkedListNode *LinkedListNodeAlloc(LinkedListHead *head) {
  LinkedListNode *node;
  node = kmalloc(sizeof(LinkedListNode));
  memset(node, 0, sizeof(LinkedListNode));
  if (head->vtable != NULL) {
    node->uniqueKey = head->vtable->keyAlloc();
    node->data = head->vtable->dataAlloc();
  } else {
    node->uniqueKey = kmalloc(head->keySize);
    node->data = kmalloc(head->dataSize);
    memset(node->uniqueKey, 0, head->keySize);
    memset(node->data, 0, head->dataSize);

  }

  return node;
};

void LinkedListNodeFree(LinkedListNode *node, LinkedListHead *head) {
  if (head->vtable != NULL) {
    head->vtable->dataFree(node->data);
    head->vtable->keyFree(node->uniqueKey);
  } else {
    kmallocfree(node->data);
    kmallocfree(node->uniqueKey);
  }
  kmallocfree(node);
}

void LinkedListNodeInit(LinkedListNode *node, LinkedListHead *head, const void *uniqueKey, const void *data) {
  memmove(node->uniqueKey, uniqueKey, head->keySize);
  if(data != NULL)
    memmove(node->data, data, head->dataSize);
}

LinkedListNode *LinkedListNodeCreate(LinkedListHead *head, const void *uniqueKey, const void *data) {

  LinkedListNode *node;
  node = LinkedListNodeAlloc(head);
  LinkedListNodeInit(node, head, uniqueKey, data);
  return node;
}

void LinkedListAdd(LinkedListHead *head, const void *uniqueKey, const void *data) {


  LinkedListNode *prev = head->end;

  head->end = LinkedListNodeCreate(head, uniqueKey, data);

  if (head->start == NULL) {
    head->start = head->end;
  } else {
    prev->next = head->end;
    head->end->prev = prev;
  }
  head->length++;

}

// Returns the node
// Returns NULL if either no nodes left
LinkedListNode *LinkedListNodeGetNextMatching(LinkedListNode *start, LinkedListHead *head, const void *uniqueKey) {
  LinkedListNode *cur;

  for (cur = start; cur != NULL; cur = cur->next) {
    if (uniqueKey == NULL || memcmp(cur->uniqueKey, uniqueKey, head->keySize) == 0)
      return cur;
  }
  return NULL;

}

// Returns next item after the deleted node
// Returns NULL if either no nodes left or the end node was removed
LinkedListNode *LinkedListNodeRemoveNextMatching(LinkedListNode *start, LinkedListHead *head, const void *uniqueKey) {
  LinkedListNode *cur;
  for (cur = start; cur != NULL; cur = cur->next) {
    if (uniqueKey == NULL || memcmp(cur->uniqueKey, uniqueKey, head->keySize) == 0) {
      head->length--;
      if (cur->prev == NULL)
        head->start = cur->next;
      if (cur->next == NULL)
        head->end = cur->prev;
      cur->prev->next = cur->next;
      cur->next->prev = cur->prev;
      LinkedListNode *next = cur->next;
      LinkedListNodeFree(cur, head);
      return next;
    }
  }
  return NULL;

}

LinkedListNode *LinkedListGet(LinkedListHead *head, const void *uniqueKey) {

  return LinkedListNodeGetNextMatching(head->start, head, uniqueKey);

}

LinkedListNode *LinkedListRemove(LinkedListHead *head, void *uniqueKey) {
  return LinkedListNodeRemoveNextMatching(head->start, head, uniqueKey);
}

LinkedListHead *LinkedListAlloc() {
  LinkedListHead *list;
  list = kmalloc(sizeof(LinkedListHead));
  memset(list, 0, sizeof(LinkedListHead));
  return list;
}

void LinkedListFree(LinkedListHead *list) {
  LinkedListNode *node = list->start;
  LinkedListNode *next;
  for (; node != NULL; node = next) {
    next = node->next;
    LinkedListNodeFree(node, list);
  }
  kmallocfree(list);
}

SwapData *SwapDataAlloc() {
  SwapData *swapData;
  swapData = kmalloc(sizeof(SwapData));
  memset(swapData, 0, sizeof(SwapData));

  swapData->PTEs = LinkedListAlloc();
  return swapData;
}

void SwapDataFree(SwapData *swapData) {
  LinkedListFree(swapData->PTEs);
  kmallocfree(swapData);
}

void SwapDataInit(SwapData *swapData) {
  swapData->swapfilePageNo = -1;
  *swapData->PTEs = defaultLLPTE;
}

SwapData *SwapDataCreate() {
  SwapData *swapData = SwapDataAlloc();
  SwapDataInit(swapData);
  return swapData;
}

SwapUniqueKey *SwapUniqueKeyAlloc() {
  SwapUniqueKey *uniqueKey;
  uniqueKey = kmalloc(sizeof(SwapUniqueKey));
  memset(uniqueKey, 0, sizeof(SwapUniqueKey));
  return uniqueKey;
}

void SwapUniqueKeyFree(SwapUniqueKey *uniqueKey) {
  kmallocfree(uniqueKey);
}

void SwapUniqueKeyInit(SwapUniqueKey *uniqueKey) {
  uniqueKey->log_a = NULL;
  uniqueKey->pa = NULL;

}

SwapUniqueKey *SwapUniqueKeyCreate() {
  SwapUniqueKey *uniqueKey = SwapUniqueKeyAlloc();
  SwapUniqueKeyInit(uniqueKey);
  return uniqueKey;
}


// parentPTE != NULL is only valid if called from the copyuvm
BOOL SwapDataAddPTE(SwapData *swapData, pte_t **parentPTE, pte_t **PTE) {

  LinkedListNode *node;
  if (parentPTE == NULL || (node = LinkedListGet(swapData->PTEs, parentPTE)) != NULL) {
    LinkedListAdd(swapData->PTEs, PTE, NULL);
    return TRUE;
  }
  return FALSE;
}

BOOL SwapDataRemovePTE(SwapData *swapData, pte_t **PTE) {

  /* edge cases:
   * 1. removed last element - should delete the node from swaplist*/

  LinkedListNode *node;
  if ((node = LinkedListGet(swapData->PTEs, PTE)) != NULL) {
    LinkedListNodeRemoveNextMatching(node, swapData->PTEs, NULL);
    return TRUE;
  }
  return FALSE;
}


