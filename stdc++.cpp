//
// Created by ADMIN on 09-Oct-23.
//

#include "types.h"
#include "stat.h"
#include "user.h"
#include "stdc++.hpp"
new_handler default_new_handler = []{
  // i'd rather throw but now i cannot be bothered
  printf(STDERR, "*** an error occured while allocating.\n");
  exit();
};

new_handler get_new_handler() noexcept {
  return default_new_handler;
};

void *
operator new(uint sz) {
  void *p;

  /* malloc (0) is unpredictable; avoid it.  */
  if (__builtin_expect(sz == 0, false))
    sz = 1;

  while ((p = malloc(sz)) == 0) { // was able to allocate 0 bytes
    new_handler handler = get_new_handler();
    if (!handler){
      printf(STDERR, "new handler is a nullptr\n");
      exit();
    }
    handler();
//      _GLIBCXX_THROW_OR_ABORT(bad_alloc());
  }

  return p;
}

void *
operator new[](uint sz){
  void* p = nullptr;
  p =  ::operator new(sz);
  return p;
};
void
operator delete(void* ptr) noexcept {
  free(ptr);
};

void
operator delete[](void* ptr) noexcept {
  ::operator delete(ptr);
};