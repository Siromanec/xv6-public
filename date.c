//
// Created by ADMIN on 26-Sep-23.
//
//#include "defs.h"
#include "types.h"
#include "stat.h"
#include "user.h"

#include "date.h"

#define STDOUT 1

int main(int argv, char **argc) {


    struct rtcdate mydate= {0,0,0,0,0,0};
//    mydate = (struct rtcdate *)malloc(sizeof (struct rtcdate));
//    printf(STDOUT, "%u\n", mydate.day);
    date(&mydate);
//    free(mydate);
//    printf(1, "%02u:%02u:%02u %02u/%02u/%02u\n", mydate.hour, mydate.minute, mydate.second, mydate.day, mydate.month, mydate.year);
    printf(1, "%02u:%02u:%02u %02u/%02u/%02u\n", mydate.hour, mydate.minute, mydate.second, mydate.day, mydate.month, mydate.year);

//    printf(1, "%02u:%02u:%02u %02u/%02u/%02u\n", mydate->hour, mydate->minute, mydate->second, mydate->day, mydate->month, mydate->year);

    exit();
//    return 0;
}

// кривий варіант для логування: написати іфку в кожному сисколі, чи відобпажати аргументи
// варіант кращий: написати обгортку