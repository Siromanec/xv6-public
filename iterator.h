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

typedef struct pa_pte_iterator {
  iterator_t iterator;
  page_data_t pd; // never use ptr; is modified by iterator
  uint pa;
} pa_pte_iterator_t;

void pa_pte_iterator_init(pa_pte_iterator_t *iterator, const page_data_t * pd, uint pa);;


BOOL pa_pte_iterator_has_next(pa_pte_iterator_t *iterator);;

void * pa_pte_iterator_get_next(pa_pte_iterator_t *iterator);;




void interract_with_pte(uint pa, page_data_t * pd, int (*callback)(pte_t *));

#endif //XV6_PUBLIC_ITERATOR_H
