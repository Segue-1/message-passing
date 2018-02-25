#include <stdio.h>
#include <sys/wait.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <string.h>

#include "global_constants.h"
#include "helpers.h"
#include "message_queue.h"

void wait_for_all_children();
char* get_msgqid(int msgqid);
void add_signal_handlers();
void handle_sigint(int sig);
void cleanup_and_exit();

// Globals used in signal handler
int msgqid;
pid_t* childpids;

int main (int argc, char *argv[]) {

    add_signal_handlers();

    int n = parse_cmd_line_args(argc, argv);
    if (n == 0) {
        n = 1;
    }
    int proc_limit = 5;
    int proc_count = 0;                // Number of concurrent children
    childpids = malloc(sizeof(pid_t) * proc_limit);
    int num_procs_spawned = 0;

    struct msgbuf sbuf = {.mtype = 1, .clock.seconds = 0, .clock.nanoseconds = 0};

    // need total processes generated
    // need total time elapsed (timer)

    msgqid = get_message_queue();

    char* execv_arr[EXECV_SIZE];
    execv_arr[0] = "./user";
    execv_arr[EXECV_SIZE - 1] = NULL;

    int i;
    for (i = 0; i < proc_limit; i++) {

        if (proc_count == proc_limit) {
            // Wait for one child to finish and decrement proc_count
            wait(NULL);
            proc_count -= 1;
        }

        if ((childpids[i] = fork()) == 0) {
            // Child so...
            char msgq_id[10];
            sprintf(msgq_id, "%d", msgqid);
            execv_arr[MSGQ_ID_IDX] = msgq_id;

            execvp(execv_arr[0], execv_arr);

            perror("Child failed to execvp the command!");
            return 1;
        }

        if (childpids[i] == -1) {
            perror("Child failed to fork!\n");
            return 1;
        }

        // Increment because we forked
        proc_count += 1;
        num_procs_spawned += 1;

        if (waitpid(-1, NULL, WNOHANG) > 0) {
            // A child has finished executing
            proc_count -= 1;
        }

        if (msgsnd(msgqid, &sbuf, sizeof(sbuf.clock), IPC_NOWAIT) < 0) {
            printf("%d, %ld, %d:%d, %lu\n", msgqid, sbuf.mtype, sbuf.clock.seconds, sbuf.clock.nanoseconds, sizeof(sbuf.clock));
            perror("msgsnd");
            exit(1);
        }
       else {
           printf("oss : wrote to clock: %d:%d \n", sbuf.clock.seconds, sbuf.clock.nanoseconds);
       }

    }

    sleep(5);

    cleanup_and_exit();

    return 0;

}

void wait_for_all_children() {
    pid_t childpid;
    while  ( (childpid = wait(NULL) ) > 0) {
        printf("Child exited. pid: %d\n", childpid);
    }
}

void terminate_children() {
    int length = sizeof(childpids)/sizeof(childpids[0]);
    int i;
    for (i = 0; i < length; i++) {
        if (childpids[i] > 0) {
            kill(childpids[i], SIGTERM);
        }
    }
    free(childpids);
}

void add_signal_handlers() {
    struct sigaction act;
    act.sa_handler = handle_sigint; // Signal handler
    sigemptyset(&act.sa_mask);      // No other signals should be blocked
    act.sa_flags = 0;               // 0 so do not modify behavior
    if (sigaction(SIGINT, &act, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
}

void handle_sigint(int sig) {
    printf("\nCaught signal %d\n", sig);
    cleanup_and_exit();
}

void cleanup_and_exit() {
    terminate_children();
    remove_message_queue(msgqid);
    exit(0);
}
