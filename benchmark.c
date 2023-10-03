//
// Created by ADMIN on 26-Sep-23.
//
#include "types.h"
#include "stat.h"
#include "user.h"

#define ITERCOUNT 1000000000
#define STDOUT 1

int main(int argv, char **argc) {

    int ans = 1;
    for (int j = 0; j < ITERCOUNT; ++j) {
        for (uint i = 0; i < ITERCOUNT; ++i) {
            ans += i + j;
        }
    }

    printf(STDOUT, "ans: %d\n", ans);
    exit();
}

