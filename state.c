//
// Created by ADMIN on 01-Oct-23.
//

//#include "defs.h"
#include "types.h"
#include "user.h"
#include "param.h"
#include "stateinfo.h"

int main(int argv, char **argc) {
    // i do not use normal structures such as proc and cpu, because they carry too much info
    // and it is better to incapsulate it
    struct procinfo *pi_arr = malloc(NPROC * sizeof (struct procinfo));
    struct cpuinfo *cpui_arr = malloc(NCPU * sizeof (struct cpuinfo));

    if (state(&pi_arr, &cpui_arr) < 0)
    {
        printf(STDERR, "*** an error occurred\n");
        exit();
    }

    for (int i = 0; i < NPROC; ++i) {
        if (pi_arr[i].pid == 0 && pi_arr[i].size == 0) { // if no mem the process probably does not exist
            break;
        }
        printf(STDOUT, "pid:%2d\tstate:%s\tname:%4s\tmemory:%9u\tnfiles:%02u\t",
               pi_arr[i].pid, pi_arr[i].state, pi_arr[i].name, pi_arr[i].size, pi_arr[i].file_count);
        printf(STDOUT, "inodes:(");
        for (int j = 0; j < NOFILE; ++j) {
            if(pi_arr[i].inodeIds[j] == 0)
                break;
            printf(STDOUT, " %d,", pi_arr[i].inodeIds[j]);

        }
//        printf(STDOUT, ")\t");
        printf(STDOUT, ")\n");
    }

    for (int i = 0; i < NCPU; ++i) {
        if (cpui_arr[i].pid == 0 && cpui_arr[i].id == 0 && i!=0)
            break;
        printf(STDOUT, "cpu:%d\tpid:%d\n", cpui_arr[i].id, cpui_arr[i].pid);
    }
    free(pi_arr);
    free(cpui_arr);

    exit();
}
