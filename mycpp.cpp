//
// Created by ADMIN on 08-Oct-23.
//
#include "types.h"
#include "stat.h"
#include "user.h"
#include "stdc++.hpp"
//extern "C" {
//    void printf(int, const char*, ...);
//    exit();
//}
class Foo{
    char* bar;
    char* fuzz;
public:
    Foo(char* bar, char* fuzz): bar{bar}, fuzz{fuzz}{};
    int deephi(){
        printf(STDOUT, "bar:%s\tfuzz:%s", bar, fuzz);
        return strlen(bar) + strlen(fuzz);
    }
    ~Foo() = default;

};

int main(int argc, char * argv[]) {
    if (argc!=3){
        printf(STDERR, "Usage: str1 str2");
        exit();
    }
    char * string = new char[100];
    strcpy(string, "my cool new allocated string");
    Foo foo{argv[1], argv[2]};
    printf(STDOUT, "barfuzz len %d, %s", foo.deephi(), string);
    delete[] string;
    exit();
}
