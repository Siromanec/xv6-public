//
// Created by ADMIN on 05-Jan-24.
//
#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"
#include "spinlock.h"

#include "iterator.h"
BOOL iterator_has_next (iterator_t * iterator, BOOL (*concrete_iterator_callback) (iterator_t *)) {
  // [begin, end)
  if(concrete_iterator_callback != NULL)
    return concrete_iterator_callback(iterator);
  return iterator->next_item != iterator->end;
};

void * iterator_get_next (iterator_t * iterator, void * (*concrete_iterator_callback) (iterator_t *)) {
  if(concrete_iterator_callback != NULL)
    return concrete_iterator_callback(iterator);
  void * this_item = iterator->next_item;
  iterator->next_item += iterator->item_size;
  return this_item;
};


