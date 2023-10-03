//
// Created by ADMIN on 03-Oct-23.
//

#ifndef XV6_PUBLIC_STATEINFO_H
#define XV6_PUBLIC_STATEINFO_H
typedef struct cpuinfo{
    int id;
    int pid;
} cpuinfo_t;
typedef struct procinfo{
    int pid;
    char state[16];
    char name[16];
    uint size;
    uint file_count;
    int inodeIds[NOFILE];
} procinfo_t;
struct stateinfo {
    procinfo_t *proc[NPROC];
    cpuinfo_t *cpuinfo[NCPU];
};
#endif //XV6_PUBLIC_STATEINFO_H
