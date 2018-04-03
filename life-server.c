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


void
waiting_client(int sig)
{
	if (sig == SIGALRM) {
		printf("Waiting for client to appear..\n");
		alarm(10);
	}
}

void
waiting_command(int sig)
{
    if (sig == SIGALRM) {
        printf("Waiting for command..\n");
        alarm(300);
    }
}

struct msgbuf
{
	long type;
	int msg[4];
};

int
get_line_width(int argc, char *argv[])
{
    if (argc != 4) {
		printf("ERROR: Initialization - argument amount error.\n");
		exit (-1);
	}
	int m = atoi(argv[1]), n = atoi(argv[2]), k = atoi(argv[3]);
	if (k > n) {
		printf("ERROR: Initialization - K should be not more than N.\n");
		exit (-1);
	} else if (k <= 0) {
		printf("ERROR: Initialization - K should be positive.\n");
		exit (-1);
	}
	if ((m <= 0) || (n <= 0)){
		printf("ERROR: Initialization - M and N should be positive.\n");
		exit (-1);
	}
	return ((!(n % k)) || (!(n % (k - 1)))) ? n / k : n / (k - 1);
}

int
get_workerid_by_line(int column, int line_width)
{
    return (column - 1) / line_width;
}

int
main(int argc, char *argv[])
{
	//Initializtion
	printf("Initialization.\n");
	int m = 0, n  = 0, worker_amount = 0, worker_id = 0, *worker_pid, line_width = 0;
	int key = ftok("key.txt", 's');
	line_width = get_line_width(argc, argv);
	worker_pid = calloc(worker_amount, sizeof(*worker_pid));
	m = atoi(argv[1]); n = atoi(argv[2]); worker_amount = atoi(argv[3]);

	if (worker_amount > 3) {
        worker_amount++;
	}

    int shmid; //= shmget(key, 2 * (worker_amount - 1) * (m + 2) * sizeof(char), IPC_CREAT | 0666);  //Shared memory for workers + special field for cmd START <N>
    if (worker_amount != 1)
        if ((shmid = shmget(key, 2 * (worker_amount - 1) * (m + 2) * sizeof(char), IPC_CREAT | 0666)) < 0) { //Shared memory for workers + special field for cmd START <N>
            printf("SOMETHING WENT WRONG WITH SHMID!!\n");
            printf("%s\n", strerror(errno));
        }
    int semid; //= semget(key, worker_amount + 1, IPC_CREAT | 0666);                 //Semaphores for synchronized memory access. One for each line. One sem for
    if  ((semid = semget(key, worker_amount + 1, IPC_CREAT | 0666)) < 0) {                //Semaphores for synchronized memory access. One for each line. One sem for
        printf("SOMETHING WENT WRONG WITH SEMID!!\n");
    }

    int wmsgid ;//= msgget(key, IPC_CREAT | 0666);
    if ((wmsgid = msgget(key, IPC_CREAT | 0666)) < 0) {
        printf("SOMETHING WENT WRONG WITH WMSGID!!\n");
    }

    key = ftok("key.txt", 'w');
    int snpsemid;//
    if ((snpsemid = semget(key, worker_amount, IPC_CREAT | 0666)) < 0) {
        printf("SOMETHING WENT WRONG WITH SNPSEMID!!\n");
    }
    int smsgid;
    if ((smsgid = msgget(key, IPC_CREAT | 0666)) < 0) {
        printf("SOMETHING WENT WRONG WITH SMSGID!!\n");
    }
    for (int i = 0; i < worker_amount; i++) {
        semctl(snpsemid, i, SETVAL, 0);
    }                                            //On every iteration of START, worker will wait for last semaphore to become 0
    semctl(semid, worker_amount, SETVAL, worker_amount);
    for (int i = 0; i < worker_amount; i++) {
        semctl(semid, i, SETVAL, 1);
    }

	for (worker_id = 0; worker_id < worker_amount; worker_id++) {               //Workers.
		if ((worker_pid[worker_id] = fork()) == 0) {
			break;
		}
	}

	if (worker_id != worker_amount) {
		//Worker.
        if (worker_amount > 3) {
            worker_amount--;
        }
        char *table = (char *) shmat(shmid, NULL, 0);
        //Initialization
        struct sembuf buf = {};
        struct sembuf *sync_buf;
        if (worker_id != 3) {
            sync_buf = calloc(worker_amount, sizeof(*sync_buf)); // Here will be костыль
        }
        if (worker_id == 3) {
            return 0;
        }
        if (worker_id == worker_amount) {
            worker_id = 3;
        }
        int my_line_width = line_width, my_pid = getpid();
		if (worker_id == (worker_amount - 1)) {
            my_line_width = n - (worker_amount - 1) * line_width;
        }
        struct sembuf w_buf = {};
        buf.sem_op = -1; buf.sem_num = worker_amount;
        w_buf.sem_op = -1; w_buf.sem_num = worker_id;
        for (int i = 0; i < worker_amount; i++) {
            sync_buf[i].sem_op = 0;
            sync_buf[i].sem_num = i;
        }
        char *my_table = calloc(my_line_width * (m + 2), sizeof(*my_table));
        char *my_table_cur = calloc(my_line_width * (m + 2), sizeof(*my_table));
        semop(semid, &w_buf, 1);
        semop(semid, sync_buf, worker_amount);
        semop(semid, &buf, 1);

        //Receiving commands
        struct msgbuf message = {};
        while (1) {
            msgrcv(wmsgid, &message, sizeof(message) - sizeof(long), my_pid, 0);
            w_buf.sem_op = worker_amount + 1;
            semop(semid, &w_buf, 1);
            for (int i = 0; i < worker_amount; i++) {
                sync_buf[i].sem_op = -1;
            }
            semop(semid, sync_buf, worker_amount);
            switch(message.msg[0])
            {
            case NOTHING:
                {
                    break;
                }
            case ADD:
                {
                    int column_num = (message.msg[2] > (worker_amount - 1) * line_width) ? message.msg[2] - (worker_amount - 1) * line_width - 1 : (message.msg[2] - 1) % line_width;
                    my_table[(m + 2) * (column_num) + message.msg[1]] = 1; //!!!!
                    break;
                }
            case CLEAR:
                {
                    for (int i = 0; i < my_line_width; i++) {
                        for (int j = 0; j < m + 2; j++) {
                            my_table[i * (m + 2) + j] = 0;
                        }
                    }
                    break;
                }
            case SNAPSHOT:
                {

                    for (int i = 1; i <= m; i++) {
                        for (int j = 0; j < my_line_width; j++) {
                            message.msg[0] = my_table[j * (m + 2) + i];
                            message.type = my_pid;
                            msgsnd(smsgid, &message, sizeof(message) - sizeof(long), 0);
                        }
                    }
                    break;
                }
            case START:
                {
                    for (int i = 1; i <= m; i++) {
                        for (int j = 0; j < my_line_width; j++) {
                            int counter = 0;
                            if ((j == 0)) {
                                if (worker_id != 0) {
                                    counter += table[2 * (worker_id - 1) * (m + 2) + i - 1];
                                    counter += table[2 * (worker_id - 1) * (m + 2) + i];
                                    counter += table[2 * (worker_id - 1) * (m + 2) + i + 1];
                                }
                            } else {
                                counter += my_table[(j - 1) * (m + 2) + i - 1];
                                counter += my_table[(j - 1) * (m + 2) + i];
                                counter += my_table[(j - 1) * (m + 2) + i + 1];
                            }
                            if (j == my_line_width - 1) {
                                if (worker_id != (worker_amount - 1)) {
                                    counter += table[(2 * (worker_id + 1) - 1) * (m + 2) + i - 1];
                                    counter += table[(2 * (worker_id + 1) - 1) * (m + 2) + i];
                                    counter += table[(2 * (worker_id + 1) - 1) * (m + 2) + i + 1];
                                }
                            } else {
                                counter += my_table[(j + 1) * (m + 2) + i - 1];
                                counter += my_table[(j + 1) * (m + 2) + i];
                                counter += my_table[(j + 1) * (m + 2) + i + 1];
                            }
                            counter += my_table[j * (m + 2) + i - 1];
                            counter += my_table[j * (m + 2) + i + 1];
                            if (my_table[j * (m + 2) + i] == 1) {
                                if ((counter == 2) || (counter == 3)) {
                                    my_table_cur[j * (m + 2) + i] = 1;
                                } else {
                                    my_table_cur[j * (m + 2) + i] = 0;
                                }
                            } else {
                                if (counter == 3) {
                                    my_table_cur[j * (m + 2) + i] = 1;
                                } else {
                                    my_table_cur[j * (m + 2) + i] = 0;
                                }
                            }

                        }
                    }
                    for (int i = 0; i < m + 2; i++) {
                        for (int j = 0; j < my_line_width; j++) {
                            my_table[j * (m + 2) + i] = my_table_cur[j * (m + 2) + i];
                        }
                    }
                    break;
                }
            case TERMINATE:
                {
                    free(my_table);
                    free(my_table_cur);
                    free(sync_buf);
                    fflush(stdout);
                    return 0;
                }
            default:
                {
                    break;
                }
            }
            w_buf.sem_op = -1;
            semop(semid, &w_buf, 1);
            for (int i = 0; i < worker_amount; i++) {
                sync_buf[i].sem_op = 0;
            }
            semop(semid, sync_buf, worker_amount);
            if (worker_id != 0) {
                for (int i = 0; i < m + 2; i++) {
                    table[(m + 2) * (2 * worker_id - 1) + i] = my_table[0 * (m + 2) + i];
                }
            }
            if (worker_id != worker_amount - 1) {
                for (int i = 0; i < m + 2; i++) {
                    table[(m + 2) * (2 * worker_id) + i] = my_table[(my_line_width - 1) * (m + 2) + i];
                }
            }
            semop(semid, &buf, 1);
        }

		return 0;
	}
	if (worker_amount > 3) {
        worker_amount--;
        int tmp = worker_pid[3];
        worker_pid[3] = worker_pid[worker_amount];
        worker_pid[worker_amount] = tmp;
	}

	//Beginning of work with a Client
	int term_flag = 0;
	key = ftok("key.txt", 'c');
	int msgid = 0;
	struct msgbuf message = {};
	while (!term_flag) {
        msgid = msgget(key, IPC_CREAT | 0666);
        signal(SIGALRM, waiting_client);
        int client_pid = 0;
        printf("Ready to work with client.\n");
        alarm(10);
        int check_size = 0;
        do {
            check_size = msgrcv(msgid, &message, sizeof(message) - sizeof(long), SERVER, 0);
        } while(check_size == -1);  //while there is no client...
        signal(SIGALRM, SIG_IGN);
        client_pid = message.msg[0]; //SIGUSR1 = OK; SIGUSR2 = ERROR + MSG EXPLANATION or SIGUSR1 = something happened + MSG EXPLANATION. EVERYTHING IS TLEN.
        message.type = CLIENT;
        msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
        printf("Connection established.\n");

        int m_add = 0, n_add = 0, p_current = 0, p_max = 0, visual_flag = 0, snapshot_flag = 0, add_flag = 0, clear_flag = 1, itsok_flag = 1, stop_flag = 0, client_flag = 1;
        struct sembuf buf = {};
        buf.sem_num = worker_amount;
        while (client_flag) {
                //GET_COMMAND
                itsok_flag = !(stop_flag || snapshot_flag || add_flag || clear_flag);
                if(itsok_flag)
                    check_size = msgrcv(msgid, &message, sizeof(message) - sizeof(long), SERVER, IPC_NOWAIT);
                if (itsok_flag && (check_size > 0)){
                    if (message.msg[0] == TERMINATE) {
                        term_flag = 1;
                        message.type = CLIENT;
                        message.msg[0] = OK;
                        msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                        kill(client_pid, SIGUSR1);
                        break;
                    }
                    int err_flag = 0;

                    switch (message.msg[0]) {                                                      //Create for every command its function.
                    case ADD:
                        {
                            printf("Received ADD command.\n");
                            m_add = message.msg[1];
                            n_add = message.msg[2];
                            if (m_add <= 0) {
                                message.type = CLIENT;
                                message.msg[0] = ERR_MTL;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                err_flag = 1;
                                printf("Rejected ADD command.\n");
                                break;
                            } else if (m_add > m) {
                                message.type = CLIENT;
                                message.msg[0] = ERR_MTH;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                err_flag = 1;
                                printf("Rejected ADD command.\n");
                                break;
                            } else if (n_add <= 0) {
                                message.type = CLIENT;
                                message.msg[0] = ERR_NTL;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                err_flag = 1;
                                printf("Rejected ADD command.\n");
                                break;
                            } else if (n_add > n) {
                                message.type = CLIENT;
                                message.msg[0] = ERR_NTH;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                err_flag = 1;
                                printf("Rejected ADD command.\n");
                                break;
                            }

                            add_flag = 1;
                            break;

                        }
                    case CLEAR:
                        {
                            printf("Received CLEAR command.\n");
                            clear_flag = 1;
                            break;
                        }
                    case START:
                        {
                            printf("Received START command.\n");
                            if (message.msg[1] <= 0) {
                                message.type = CLIENT;
                                message.msg[1] = ERR_PTL;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                p_max = 0;
                                err_flag = 1;
                                printf("Rejected START command.\n");
                                break;
                            }
                            p_max = message.msg[1];
                            break;
                        }
                    case STOP:
                        {
                            printf("Received STOP command.\n");
                            stop_flag = 1;
                            break;
                        }
                    case SNAPSHOT:
                        {
                            printf("Received SNAPSHOT command.\n");
                            if (visual_flag) {
                                message.type = CLIENT;
                                message.msg[1] = ERR_VSL;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                                kill(client_pid, SIGUSR1);
                                p_max = 0;
                                err_flag = 1;
                                printf("Rejected SNAPSHOT command.\n");
                            }
                            snapshot_flag = 1;
                            break;
                        }
                    case VISUALIZE:
                        {
                            printf("Received VISUALIZE command.\n");
                            visual_flag = 1;
                            printf("Took in use VISUALIZE command.\n");
                            break;
                        }
                    case QUIT:
                        {
                            printf("Client is shutting down.\n");
                            client_flag = 0;
                            msgctl(msgid, IPC_RMID, 0);
                            break;
                        }
                    default:
                        {
                            printf("Received some kind of strange command, I don't know what to do!..\nSOMEBODY, PLEASE, HELP ME! D:\n");
                            err_flag = 1;
                            break;
                        }
                    }
                    if (err_flag) {
                        continue;
                    }
                    message.type = CLIENT;
                    message.msg[0] = OK;
                    msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                    kill(client_pid, SIGUSR1);
                } else {
                    if (add_flag) {
                        if (visual_flag) {
                            snapshot_flag = 1;
                        }
                        add_flag = 0;
                        buf.sem_op = 0;
                        semop(semid, &buf, 1);
                        int worker_num = (n_add > (worker_amount - 1) * line_width) ? worker_amount - 1 : (n_add - 1) / line_width;
                        for (int i = 0; i < worker_amount; i++) {
                            if (i != worker_num) {
                                message.msg[0] = NOTHING;
                            } else {
                                message.msg[0] = ADD;
                                message.msg[1] = m_add;
                                message.msg[2] = n_add;
                            }
                            message.type = worker_pid[i];
                            msgsnd(wmsgid, &message, sizeof(message) - sizeof(long), 0);
                        }
                        buf.sem_op = worker_amount;
                        semop(semid, &buf, 1);
                        printf("Completed ADD command.\n");
                    } else if (clear_flag) {
                        clear_flag = 0;
                        buf.sem_op = 0;
                        if (visual_flag) {
                            snapshot_flag = 1;
                        }
                        semop(semid, &buf, 1);
                        for (int i = 0; i < worker_amount; i++) {
                            message.type = worker_pid[i];
                            message.msg[0] = CLEAR;
                            msgsnd(wmsgid, &message, sizeof(message) - sizeof(long), 0);
                        }
                        buf.sem_op = worker_amount;
                        semop(semid, &buf, 1);
                        printf("Completed CLEAR command.\n");
                    } else if (stop_flag) {
                        p_max = 0;
                        p_current = 0;
                        stop_flag = 0;
                        buf.sem_op = 0;
                        semop(semid, &buf, 1);
                        printf("Completed STOP command.\n");
                    } else if (snapshot_flag) {                 //Some trouble here..
                        if (visual_flag)
                        usleep(100000);
                        snapshot_flag = 0;
                        buf.sem_op = 0;
                        semop(semid, &buf, 1);
                        for (int i = 0; i < worker_amount; i++) {
                            message.type = worker_pid[i];
                            message.msg[0] = SNAPSHOT;
                            msgsnd(wmsgid, &message, sizeof(message) - sizeof(long), 0);
                        }
                        buf.sem_op = worker_amount;
                        semop(semid, &buf, 1);
                        message.type = CLIENT;
                        message.msg[0] = SNPSHT;
                        message.msg[1] = m;
                        message.msg[2] = n;
                        message.msg[3] = p_current;
                        msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                        kill(client_pid, SIGUSR1);
                        for (int i = 1; i <= m; i++) {
                            for (int j = 1; j <= n; j++) {
                                int worker_num = (j > (worker_amount - 1) * line_width) ? worker_amount - 1 : (j - 1) / line_width;
                                msgrcv(smsgid, &message, sizeof(message) - sizeof(long), worker_pid[worker_num], 0);
                                message.type = CLIENT;
                                msgsnd(msgid, &message, sizeof(message) - sizeof(long), 0);
                            }
                        }
                        printf("Completed SNAPSHOT command.\n");
                    } else if (p_current < p_max) {
                        p_current++;
                        if (visual_flag) {
                            snapshot_flag = 1;
                        }
                        buf.sem_op = 0;
                        semop(semid, &buf, 1);
                        for (int i = 0; i < worker_amount; i++) {
                            message.type = worker_pid[i];
                            message.msg[0] = START;
                            msgsnd(wmsgid, &message, sizeof(message) - sizeof(long), 0);
                        }
                        buf.sem_op = worker_amount;
                        semop(semid, &buf, 1);
                        if (p_current == 1) {
                            printf("Completed START command.\n");
                        }
                        printf("Created generation #%03d.\n", p_current);
                    }
                }
        }
    }
    //Shutting down.
    printf("Shutting down..\n");
	for (int i = 0; i < worker_amount; i++) {
		/*message.type = worker_pid[i];
        message.msg[0] = TERMINATE;
        msgsnd(wmsgid, &message, sizeof(message) - sizeof(long), 0);*/
        kill(worker_pid[i], SIGTERM);
	}
	while (wait(NULL) > 0)
    {
    }
	free(worker_pid);

	msgctl(msgid, IPC_RMID, 0);
	msgctl(wmsgid, IPC_RMID, 0);
	msgctl(smsgid, IPC_RMID, 0);
	shmctl(shmid, IPC_RMID, 0);
	semctl(semid, 0, IPC_RMID, 0);
	semctl(snpsemid, 0, IPC_RMID, 0);
	printf("Ended successfully.\n");
	return 0;
}
