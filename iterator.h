//
// Created by ADMIN on 05-Jan-24.
//

#ifndef XV6_PUBLIC_ITERATOR_H
#define XV6_PUBLIC_ITERATOR_H

#include "types.h"

const static uint iterator_magic = 0x266eae07;
typedef struct iterator {
  uint item_size;
  void * next_item;
  void * end;
//  uint type;
} iterator_t;
BOOL iterator_has_next (iterator_t * iterator, BOOL (*concrete_iterator_callback) (iterator_t *));
void * iterator_get_next (iterator_t * iterator, void * (*concrete_iterator_callback) (iterator_t *));

#endif //XV6_PUBLIC_ITERATOR_H
