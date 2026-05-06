#include <stdio.h>
#include <stdlib.h>
#include "../include/ipc_core.h"

int main(int argc, char *argv[]) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   IPC Simulator — OS Course Project          ║\n");
    printf("║   PID: %-6d                                ║\n", getpid());
    printf("╚══════════════════════════════════════════════╝\n");

    int mode = 0;
    if (argc > 1) mode = atoi(argv[1]);
    switch (mode) {
        case 1: demo_unnamed_pipe();    break;
        case 2: demo_named_pipe();      break;
        case 3: demo_shared_memory();   break;
        case 4: demo_race_condition(0); break;
        case 5: demo_race_condition(1); break;
        case 6: benchmark_ipc();        break;
        default:
            demo_unnamed_pipe();
            demo_named_pipe();
            demo_shared_memory();
            demo_race_condition(0);
            demo_race_condition(1);
            benchmark_ipc();
    }
    printf("\n[DONE] All demos finished successfully.\n");
    return 0;
}
