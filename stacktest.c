//
// Created by ADMIN on 20-Nov-23.
//
//
#include "types.h"
#include "user.h"
int main(void){

//  for(;;){
//    sleep(500);

//  }
  int huge_str[8192];
  huge_str[0] = 0;
  for (int i = 1; i < 8192; ++i) {
    huge_str[i] = huge_str[i - 1] + i;
  }
  printf(STDOUT, "cumsum = %d", huge_str[8192-1]);
  exit();
}
