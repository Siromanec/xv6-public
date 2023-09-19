//
// Created by ADMIN on 12-Sep-23.
//
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h" // file constants
//#include<conio.h>

int main(int argc, char *argv[]) {
    int fd;
    if (argc == 1) {
        printf(2, "no args given\n");
        exit();
    }
//    int status = mknod(argv[1], 1, 1);
//    if (status != 0) {
//        printf(2, "an error occurred while creating the file\n");
//    }
    fd = open(argv[1], O_CREATE);
    if (close(fd)) {
        printf(2, "file close error\n");
        exit();
    }
    exit();
}