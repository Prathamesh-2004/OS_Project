/*
 * ipc_core.h - IPC Simulator Header
 */
#ifndef IPC_CORE_H
#define IPC_CORE_H

#include <sys/sem.h>

/* Demo functions */
void demo_unnamed_pipe(void);
void demo_named_pipe(void);
void demo_shared_memory(void);
void demo_race_condition(int use_semaphore);
void benchmark_ipc(void);

/* Semaphore helpers */
void sem_wait_op(int semid);
void sem_signal_op(int semid);

#endif /* IPC_CORE_H */
