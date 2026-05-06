/*
 * test_ipc.c - Unit & Integration Tests for IPC Simulator
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <sys/shm.h>
#include "../include/ipc_core.h"

static int tests_run    = 0;
static int tests_passed = 0;

#define TEST(name, expr) do { \
    tests_run++; \
    if (expr) { printf("  [PASS] %s\n", name); tests_passed++; } \
    else       { printf("  [FAIL] %s\n", name); } \
} while(0)

/* ── Test 1: Pipe creation ── */
void test_pipe_creation(void) {
    printf("\n[TEST] Pipe creation\n");
    int pfd[2];
    TEST("pipe() succeeds", pipe(pfd) == 0);
    TEST("read fd valid",   pfd[0] > 0);
    TEST("write fd valid",  pfd[1] > 0);
    close(pfd[0]); close(pfd[1]);
}

/* ── Test 2: Pipe data transfer ── */
void test_pipe_transfer(void) {
    printf("[TEST] Pipe data transfer\n");
    int pfd[2];
    pipe(pfd);
    const char *msg = "IPC_TEST_MSG";
    char buf[64] = {0};
    pid_t p = fork();
    if (p == 0) {
        close(pfd[0]);
        write(pfd[1], msg, strlen(msg));
        close(pfd[1]);
        exit(0);
    }
    close(pfd[1]);
    ssize_t n = read(pfd[0], buf, sizeof(buf)-1);
    close(pfd[0]);
    wait(NULL);
    TEST("correct byte count received", n == (ssize_t)strlen(msg));
    TEST("message content matches",     strcmp(buf, msg) == 0);
}

/* ── Test 3: Named pipe (FIFO) ── */
void test_fifo(void) {
    printf("[TEST] Named pipe (FIFO)\n");
    const char *path = "/tmp/test_ipc_fifo";
    unlink(path);
    TEST("mkfifo succeeds", mkfifo(path, 0666) == 0);
    TEST("FIFO file exists", access(path, F_OK) == 0);

    const char *msg = "FIFO_DATA";
    char buf[64] = {0};
    pid_t p = fork();
    if (p == 0) {
        int fd = open(path, O_WRONLY);
        write(fd, msg, strlen(msg));
        close(fd);
        exit(0);
    }
    int fd = open(path, O_RDONLY);
    ssize_t n = read(fd, buf, sizeof(buf)-1);
    close(fd);
    wait(NULL);
    unlink(path);
    TEST("FIFO transfer bytes correct", n == (ssize_t)strlen(msg));
    TEST("FIFO message matches",        strcmp(buf, msg) == 0);
}

/* ── Test 4: Shared memory ── */
void test_shared_memory(void) {
    printf("[TEST] Shared memory\n");
    int shmid = shmget(IPC_PRIVATE, 512, IPC_CREAT | 0666);
    TEST("shmget succeeds", shmid >= 0);
    int *mem = (int *)shmat(shmid, NULL, 0);
    TEST("shmat succeeds",  mem != (void *)-1);
    *mem = 42;
    TEST("write and read value", *mem == 42);
    shmdt(mem);
    TEST("shmctl cleanup", shmctl(shmid, IPC_RMID, NULL) == 0);
}

/* ── Test 5: Semaphore mutual exclusion ── */
void test_semaphore(void) {
    printf("[TEST] Semaphore mutual exclusion\n");
    int semid = semget(IPC_PRIVATE, 1, IPC_CREAT | 0666);
    TEST("semget succeeds", semid >= 0);
    union { int val; struct semid_ds *buf; unsigned short *array; } su;
    su.val = 1;
    semctl(semid, 0, SETVAL, su);

    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    int *counter = (int *)shmat(shmid, NULL, 0);
    *counter = 0;

    const int ITERS = 5000;
    pid_t pids[2];
    for (int p = 0; p < 2; p++) {
        pids[p] = fork();
        if (pids[p] == 0) {
            int *c = (int *)shmat(shmid, NULL, 0);
            for (int i = 0; i < ITERS; i++) {
                sem_wait_op(semid);
                (*c)++;
                sem_signal_op(semid);
            }
            shmdt(c);
            exit(0);
        }
    }
    for (int p = 0; p < 2; p++) waitpid(pids[p], NULL, 0);

    TEST("semaphore prevents race condition", *counter == ITERS * 2);

    shmdt(counter);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
}

/* ── Test 6: Race condition detection ── */
void test_race_condition(void) {
    printf("[TEST] Race condition (unsafe)\n");
    int shmid = shmget(IPC_PRIVATE, sizeof(int), IPC_CREAT | 0666);
    int *counter = (int *)shmat(shmid, NULL, 0);
    *counter = 0;

    const int ITERS = 100000;
    pid_t pids[2];
    for (int p = 0; p < 2; p++) {
        pids[p] = fork();
        if (pids[p] == 0) {
            int *c = (int *)shmat(shmid, NULL, 0);
            for (int i = 0; i < ITERS; i++) (*c)++;
            shmdt(c);
            exit(0);
        }
    }
    for (int p = 0; p < 2; p++) waitpid(pids[p], NULL, 0);

    int expected = ITERS * 2;
    printf("  [INFO] Without lock — expected: %d, got: %d\n", expected, *counter);
    TEST("race condition produces wrong result (expected)", *counter != expected);

    shmdt(counter);
    shmctl(shmid, IPC_RMID, NULL);
}

/* ── Main ── */
int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   IPC Simulator — Test Suite                 ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    test_pipe_creation();
    test_pipe_transfer();
    test_fifo();
    test_shared_memory();
    test_semaphore();
    test_race_condition();

    printf("\n══════════════════════════════════════════════\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("══════════════════════════════════════════════\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
