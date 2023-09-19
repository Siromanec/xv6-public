//
// Created by ADMIN on 13-Sep-23.
//
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h" // file constants

#define ARG_CNT 3

int main(int argc, char *argv[]) {
    if (argc != ARG_CNT) {
        printf(2, "incorrect number of arguments\n");
        exit();
    }
    if (link(argv[1], argv[2])) {
        printf(2, "link error\n");
        exit();
    }
    if (unlink(argv[1])) {
        printf(2, "unlink error\n");
        exit();
    }


    exit();
}