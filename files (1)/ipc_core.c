/*
 * ipc_core.c - Inter-Process Communication Simulator
 * Implements: Pipes, Named Pipes (FIFO), Shared Memory + Semaphores
 * Author: IPC Simulator Project
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include "../include/ipc_core.h"

/* ──────────────────────────────────────────────
   1. UNNAMED PIPE DEMO
   ────────────────────────────────────────────── */
void demo_unnamed_pipe(void) {
    int pipefd[2];
    pid_t pid;
    char write_msg[] = "Hello from parent via unnamed pipe!";
    char read_buf[256];

    printf("\n[PIPE] Creating unnamed pipe...\n");

    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid = fork();
    if (pid < 0) { perror("fork"); exit(EXIT_FAILURE); }

    if (pid == 0) {
        /* Child: read from pipe */
        close(pipefd[1]);
        ssize_t n = read(pipefd[0], read_buf, sizeof(read_buf) - 1);
        if (n > 0) {
            read_buf[n] = '\0';
            printf("[PIPE] Child (PID %d) received: \"%s\"\n", getpid(), read_buf);
        }
        close(pipefd[0]);
        exit(EXIT_SUCCESS);
    } else {
        /* Parent: write to pipe */
        close(pipefd[0]);
        printf("[PIPE] Parent (PID %d) sending: \"%s\"\n", getpid(), write_msg);
        write(pipefd[1], write_msg, strlen(write_msg));
        close(pipefd[1]);
        wait(NULL);
        printf("[PIPE] Unnamed pipe demo complete.\n");
    }
}

/* ──────────────────────────────────────────────
   2. NAMED PIPE (FIFO) DEMO
   ────────────────────────────────────────────── */
#define FIFO_PATH "/tmp/ipc_sim_fifo"

void demo_named_pipe(void) {
    pid_t pid;
    printf("\n[FIFO] Creating named pipe at %s...\n", FIFO_PATH);

    unlink(FIFO_PATH);
    if (mkfifo(FIFO_PATH, 0666) == -1) {
        perror("mkfifo");
        return;
    }

    pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child writer */
        int fd = open(FIFO_PATH, O_WRONLY);
        const char *msg = "Message via Named Pipe (FIFO)!";
        printf("[FIFO] Writer (PID %d) writing: \"%s\"\n", getpid(), msg);
        write(fd, msg, strlen(msg));
        close(fd);
        exit(EXIT_SUCCESS);
    } else {
        /* Parent reader */
        char buf[256];
        int fd = open(FIFO_PATH, O_RDONLY);
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("[FIFO] Reader (PID %d) received: \"%s\"\n", getpid(), buf);
        }
        close(fd);
        wait(NULL);
        unlink(FIFO_PATH);
        printf("[FIFO] Named pipe demo complete.\n");
    }
}

/* ──────────────────────────────────────────────
   3. SHARED MEMORY + SEMAPHORE DEMO
   ────────────────────────────────────────────── */
#define SHM_KEY  0x1A2B3C4D
#define SEM_KEY  0x5E6F7A8B
#define SHM_SIZE 1024

typedef struct {
    int    counter;
    char   message[256];
    int    ready;
} SharedData;

union semun {
    int              val;
    struct semid_ds *buf;
    unsigned short  *array;
};

static int sem_op_helper(int semid, int sem_num, int op) {
    struct sembuf sb = { .sem_num = sem_num, .sem_op = op, .sem_flg = 0 };
    return semop(semid, &sb, 1);
}

void sem_wait_op(int semid) { sem_op_helper(semid, 0, -1); }
void sem_signal_op(int semid) { sem_op_helper(semid, 0,  1); }

void demo_shared_memory(void) {
    int shmid, semid;
    SharedData *shdata;
    pid_t pid;
    union semun su;

    printf("\n[SHM] Setting up shared memory (key=0x%X, size=%d bytes)...\n",
           SHM_KEY, SHM_SIZE);

    /* Create shared memory */
    shmid = shmget(SHM_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shmid < 0) { perror("shmget"); return; }

    shdata = (SharedData *)shmat(shmid, NULL, 0);
    if (shdata == (void *)-1) { perror("shmat"); return; }

    /* Create semaphore */
    semid = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (semid < 0) { perror("semget"); return; }
    su.val = 1;
    semctl(semid, 0, SETVAL, su);

    /* Initialise shared data */
    shdata->counter = 0;
    shdata->ready   = 0;
    memset(shdata->message, 0, sizeof(shdata->message));

    pid = fork();
    if (pid < 0) { perror("fork"); return; }

    if (pid == 0) {
        /* Child: write to shared memory under semaphore */
        SharedData *cd = (SharedData *)shmat(shmid, NULL, 0);
        printf("[SHM] Child (PID %d) waiting for semaphore...\n", getpid());
        sem_wait_op(semid);
        printf("[SHM] Child acquired semaphore. Writing data...\n");
        for (int i = 0; i < 5; i++) {
            cd->counter++;
            usleep(50000);
        }
        snprintf(cd->message, sizeof(cd->message),
                 "Shared memory write complete! Counter=%d", cd->counter);
        cd->ready = 1;
        sem_signal_op(semid);
        printf("[SHM] Child released semaphore.\n");
        shmdt(cd);
        exit(EXIT_SUCCESS);
    } else {
        /* Parent: wait for child to finish then read */
        wait(NULL);
        sem_wait_op(semid);
        printf("[SHM] Parent (PID %d) reading shared memory:\n", getpid());
        printf("[SHM]   counter = %d\n", shdata->counter);
        printf("[SHM]   message = \"%s\"\n", shdata->message);
        printf("[SHM]   ready   = %d\n", shdata->ready);
        sem_signal_op(semid);

        /* Cleanup */
        shmdt(shdata);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID);
        printf("[SHM] Shared memory demo complete. Resources cleaned up.\n");
    }
}

/* ──────────────────────────────────────────────
   4. RACE CONDITION DEMO (unsafe vs safe)
   ────────────────────────────────────────────── */
#define RACE_SHM_KEY 0xDEADBEEF
#define RACE_SEM_KEY 0xCAFEBABE
#define NUM_INCREMENTS 10000

void demo_race_condition(int use_semaphore) {
    int shmid, semid = -1;
    int *counter;
    pid_t pids[2];
    union semun su;

    printf("\n[RACE] %s mode: %d increments from 2 processes\n",
           use_semaphore ? "SAFE (semaphore)" : "UNSAFE (no lock)", NUM_INCREMENTS);

    shmid = shmget(RACE_SHM_KEY, sizeof(int), IPC_CREAT | 0666);
    counter = (int *)shmat(shmid, NULL, 0);
    *counter = 0;

    if (use_semaphore) {
        semid = semget(RACE_SEM_KEY, 1, IPC_CREAT | 0666);
        su.val = 1;
        semctl(semid, 0, SETVAL, su);
    }

    for (int p = 0; p < 2; p++) {
        pids[p] = fork();
        if (pids[p] == 0) {
            int *c = (int *)shmat(shmid, NULL, 0);
            for (int i = 0; i < NUM_INCREMENTS; i++) {
                if (use_semaphore) sem_wait_op(semid);
                (*c)++;
                if (use_semaphore) sem_signal_op(semid);
            }
            shmdt(c);
            exit(EXIT_SUCCESS);
        }
    }

    for (int p = 0; p < 2; p++) waitpid(pids[p], NULL, 0);

    int expected = NUM_INCREMENTS * 2;
    printf("[RACE] Expected: %d, Got: %d — %s\n",
           expected, *counter,
           (*counter == expected) ? "CORRECT (no data race)" : "DATA RACE DETECTED!");

    shmdt(counter);
    shmctl(shmid, IPC_RMID, NULL);
    if (semid != -1) semctl(semid, 0, IPC_RMID);
}

/* ──────────────────────────────────────────────
   5. BENCHMARK
   ────────────────────────────────────────────── */
#define BENCH_MSGS   1000
#define BENCH_SIZE   1024

void benchmark_ipc(void) {
    printf("\n[BENCH] Benchmarking IPC mechanisms (%d messages, %d bytes each)\n",
           BENCH_MSGS, BENCH_SIZE);

    /* Pipe benchmark */
    {
        int pfd[2];
        pipe(pfd);
        char *buf = malloc(BENCH_SIZE);
        memset(buf, 'X', BENCH_SIZE);
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        pid_t p = fork();
        if (p == 0) {
            close(pfd[0]);
            for (int i = 0; i < BENCH_MSGS; i++) write(pfd[1], buf, BENCH_SIZE);
            close(pfd[1]);
            free(buf);
            exit(0);
        }
        close(pfd[1]);
        char *rb = malloc(BENCH_SIZE);
        for (int i = 0; i < BENCH_MSGS; i++) {
            ssize_t got = 0;
            while (got < BENCH_SIZE) got += read(pfd[0], rb + got, BENCH_SIZE - got);
        }
        close(pfd[0]);
        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec - t0.tv_sec)*1000.0 + (t1.tv_nsec - t0.tv_nsec)/1e6;
        double mb = (double)BENCH_MSGS * BENCH_SIZE / (1024*1024);
        printf("[BENCH] Pipe:          %.2f ms  |  %.2f MB/s\n", ms, mb/(ms/1000.0));
        free(buf); free(rb);
    }

    /* Shared memory benchmark */
    {
        int sid = shmget(IPC_PRIVATE, BENCH_SIZE, IPC_CREAT | 0666);
        int semid2 = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
        union semun su2; su2.val = 0;
        semctl(semid2, 0, SETVAL, su2);

        char *shm = (char *)shmat(sid, NULL, 0);
        memset(shm, 'Y', BENCH_SIZE);

        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        pid_t p = fork();
        if (p == 0) {
            char *cs = (char *)shmat(sid, NULL, 0);
            for (int i = 0; i < BENCH_MSGS; i++) {
                memset(cs, 'Z', BENCH_SIZE);
                sem_op_helper(semid2, 0, 1);
                sem_op_helper(semid2, 0, -1);
            }
            shmdt(cs);
            exit(0);
        }
        for (int i = 0; i < BENCH_MSGS; i++) {
            sem_op_helper(semid2, 0, -1);
            (void)shm[0];
            sem_op_helper(semid2, 0, 1);
        }
        wait(NULL);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double ms = (t1.tv_sec - t0.tv_sec)*1000.0 + (t1.tv_nsec - t0.tv_nsec)/1e6;
        double mb = (double)BENCH_MSGS * BENCH_SIZE / (1024*1024);
        printf("[BENCH] Shared Memory: %.2f ms  |  %.2f MB/s\n", ms, mb/(ms/1000.0));
        shmdt(shm);
        shmctl(sid, IPC_RMID, NULL);
        semctl(semid2, 0, IPC_RMID);
    }

    printf("[BENCH] Benchmark complete.\n");
}

