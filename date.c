//
// Created by ADMIN on 26-Sep-23.
//
//#include "defs.h"
#include "types.h"
#include "stat.h"
#include "user.h"

#include "date.h"

int main(int argv, char **argc) {


    struct rtcdate mydate;
    if(date(&mydate) < 0){
        printf(STDERR, "date error\n");
        exit();
    }
    printf(STDOUT, "%02u:%02u:%02u %02u/%02u/%02u\n", mydate.hour, mydate.minute, mydate.second, mydate.day, mydate.month, mydate.year);
    exit();
}
