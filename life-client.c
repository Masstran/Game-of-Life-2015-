#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <ctype.h>
#include <errno.h>

enum
{
	CLIENT = 1,
	SERVER = 2,
	TERMINATE = 0,
	ADD,
	CLEAR,
	START,
	STOP,
	SNAPSHOT,
	NOTHING,
	VISUALIZE,
	QUIT,
	OK = 0,
	ERR_MTL,        //error m too low
	ERR_MTH,        //error m too high
	ERR_NTL,        //error n too low
	ERR_NTH,        //error n too high
	ERR_PTL,        //error p to low
	SNPSHT,         //snapshot
	ERR_VSL,        //error snapshot after visualize
    MAX_BUF = 128,
    MAX_CMD = 16,
};




struct msgbuf
{
	long type;
	int msg[4];
};

int msgid = 0;

void
handler(int sig)
{
    struct msgbuf message = {};
    if (sig == SIGUSR1) {
        if (msgrcv(msgid, &message, sizeof(message) - sizeof(long), CLIENT, IPC_NOWAIT) == -1) {
            printf("For some reason, there was no point on going here..\n\n");
            return;
        }
        switch(message.msg[0])
        {
        case OK:
            {
                printf("OK\n\n");
                break;
            }
        case ERR_MTH:
            {
                printf("Error: first argument for add was too big..\n\n");
                break;
            }
        case ERR_MTL:
            {
                printf("Error: first argument for add was too small..\n\n");
                break;
            }
        case ERR_NTH:
            {
                printf("Error: second argument for add was too big..\n\n");
                break;
            }
        case ERR_NTL:
            {
                printf("Error: second argument for add was too small..\n\n");
                break;
            }
        case ERR_PTL:
            {
                printf("Error: argument for start was too small..\n\n");
                break;
            }
        case ERR_VSL:
            {
                printf("Error: can't snapshot since visualizing already.\n\n");
                break;
            }
        case SNPSHT:
            {
                int m = message.msg[1], n = message.msg[2], num = message.msg[3];
                printf("Generation #%03d\n", num);
                for (int i = 0; i < m; i++) {
                    for (int j = 0; j < n; j++) {
                        msgrcv(msgid, &message, sizeof(message) - sizeof(long), CLIENT, 0);
                        printf("%c", (message.msg[0] == 1) ? '*' : '.');
                    }
                    printf("\n");
                }
                printf("\n");
                break;
            }
        default:
            {
                printf("Unknown message from server.\n\n");
                break;
            }
        }
    }
}

int
main(void)
{
    printf("Good morning! Type 'help' for command list\n\n");
    signal(SIGUSR1, handler);
    char buf[MAX_BUF] = {}, command[MAX_CMD] = {};
    int check;
    int key = ftok("key.txt", 'c');
	msgid = msgget(key, IPC_CREAT | 0666);
    struct msgbuf message = {};
    message.type = SERVER;
    message.msg[0] = getpid();
    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
    printf("Connection suggested.\n");
    msgrcv(msgid, &message, sizeof(message) - sizeof(long), CLIENT,0);
    printf("Connection established.\n\n");
    int term_flag = 0;
    while(1) {
        check = scanf("%s", command);
        if (check != -1) {
            //sscanf(buf, "%s", command);
            if (!strcmp(command, "add")) {
                if (term_flag) {
                    printf("You've terminated server, what are you trying to do?!\n\n");
                    scanf("\n");
                    continue;
                }
                //if (sscanf(buf + 3, "%d%d", &message.msg[1], &message.msg[2]) != 2) {
                //    printf("Two arguments for add - not more nor less.\n");
                //} else {
                scanf("%d%d", &message.msg[1], &message.msg[2]);
                    message.type = SERVER;
                    message.msg[0] = ADD;
                    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                //}
            } else if (!strcmp(command, "clear")) {
                //scanf("\n");
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        continue;
                    }
                message.type = SERVER;
                message.msg[0] = CLEAR;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "start")) {
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        scanf("\n");
                        continue;
                    }
                scanf("%d", &message.msg[1]);
                    message.type = SERVER;
                    message.msg[0] = START;
                    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "stop")) {
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        continue;
                    }
                message.type = SERVER;
                message.msg[0] = STOP;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "snapshot")) {
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        continue;
                    }
                message.type = SERVER;
                message.msg[0] = SNAPSHOT;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "quit")) {
                if (!term_flag) {
                    message.type = SERVER;
                    message.msg[0] = QUIT;
                    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                }
                break;
            } else if (!strcmp(command, "visualize")) {
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        continue;
                    }
                message.type = SERVER;
                message.msg[0] = VISUALIZE;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "terminate")) {
                if (term_flag) {
                        printf("You've terminated server, what are you trying to do?!\n\n");
                        continue;
                    }
                term_flag = 1;
                message.type = SERVER;
                message.msg[0] = TERMINATE;
                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
            } else if (!strcmp(command, "help")) {
                printf("Possible commands:\n\nhelp - show all commands\n\nadd <line[m]> <column[n]> - add life-form in cage (m, n)\n\n");
                printf("clear - clear table\n\nstart <p> - start creating generations until p generation\n\nstop - stop creating process (number of generation defines by 0)\n\n");
                printf("snapshot - look on table itself!\n\nquit - quit\n\nvisualize - watch after creating process\n\nterminate - kill server\n\n");
            }
            else {
                printf("Unknown command, sorry!\n");
            }
        } else {

        }
    }
    printf("Have a nice day!\n");
    return 0;
}

