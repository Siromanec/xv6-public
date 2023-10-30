struct stat;
struct rtcdate;
struct procinfo;
struct cpuinfo;

#define STDIN  0
#define STDOUT 1
#define STDERR 2
#define NULL   0

#ifdef __cplusplus
extern "C" {
#endif
// system calls
int fork(void);
#ifndef DEFS_HEADER
int exit(void) __attribute__((noreturn));
#endif
int wait(void);
int pipe(int*);
int write(int, const void*, int);
int read(int, void*, int);
int close(int);
int kill(int);
int exec(char*, char**);
int open(const char*, int);
int mknod(const char*, short, short);
int unlink(const char*);
int fstat(int fd, struct stat*);
int link(const char*, const char*);
int mkdir(const char*);
int chdir(const char*);
int dup(int);
int getpid(void);
char* sbrk(int);
#ifndef DEFS_HEADER
int sleep(int);
#endif
int uptime(void);
int date(struct rtcdate *);
int toggleLogging(void);
int state(struct procinfo* pi_arr[], struct cpuinfo* cpui_arr[]);
int swap(void);


// ulib.c
int stat(const char*, struct stat*);
char* strcpy(char*, const char*);
#ifndef DEFS_HEADER
void *memmove(void*, const void*, int);
#endif
char* strchr(const char*, char c);
int strcmp(const char*, const char*);
void printf(int, const char*, ...);
char* gets(char*, int max);
#ifndef DEFS_HEADER
uint strlen(const char*);
#endif
void* memset(void*, int, uint);
void* malloc(uint);
void free(void*);
int atoi(const char*);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
 #include "stdc++.hpp"
#endif