//
// Created by ADMIN on 09-Oct-23.
//

#ifndef XV6_PUBLIC_STDC_HPP
#define XV6_PUBLIC_STDC_HPP
typedef void (*new_handler)();



new_handler get_new_handler() noexcept;

void *
operator new(uint sz);

void *
operator new[](uint sz);

void
operator delete(void* ptr) noexcept;

void
operator delete[](void* ptr) noexcept;
//size_t nullptr = 0;
#endif //XV6_PUBLIC_STDC_HPP
