//
// Created by ADMIN on 13-Sep-23.
//
#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h" // file constants
#define BUFFER_SIZE 512
#define ARG_CNT 3
//#include<conio.h>
int main(int argc, char *argv[]) {
    int from_fd;
    int to_fd;
    int n;

    char buff[BUFFER_SIZE];



    if (argc != ARG_CNT) {
        printf(2, "incorrect number of arguments\n");
        exit();
    }

    from_fd = open(argv[1], O_RDONLY);
    to_fd = open(argv[2],  O_CREATE|O_WRONLY);

    for(;;){
        n = read(from_fd, buff, sizeof buff);
        if(n == 0)
            break;
        if(n < 0){
            printf(2, "read error\n");
            exit();
        }
        if(write(to_fd, buff, n) != n){
            printf(2, "write error\n");
            exit();
        }
    }
    //although i could do file closing manually, the code of exit() is supposed to close the files anyway


    exit();
}