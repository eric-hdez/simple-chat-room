#include <err.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#define BLOCK         4096
#define KB            1024
#define CONN_LOST_MSG ">> Connection to the server has been lost.\n"
#define CONN_IMP_MSG  ">> Could not establish a connection to the server"
#define FAILED_MSG    ">> Failed to send your last message.\n"
#define LOST_MSG      ">> A message has been lost.\n"
#define EXIT          "exit"

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_args_t {
    int clientfd;
} thread_args_t;

bool flag = false;

// converts an integer in string represention to an unsigned 
// 16 bit integer
//
// number : integer in string representation
//
uint16_t strtouint16(char *number) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }

    return num;
}

// creates & connects a client connection socket on the provided 
// port
//
// port : port number to establish a connection
// 
int create_client_socket(uint16_t port) {
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientfd < 0) {
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);  //inet_addr(ip_addr);
    addr.sin_port = htons(port);

    if (connect(clientfd, (struct sockaddr *) &addr, sizeof addr)) {
        return -1;
    }
    return clientfd;
}

// thread function that handles sending a client's messages
// to the server listening on the port
//
// args : thread function arguments
//
void *handle_send_msg(void *args) {
    thread_args_t *targs = args;
    char msgin[BLOCK] = { 0 };
    int nbytes = 0;

    while(true) {
        fgets(msgin, BLOCK, stdin);
        nbytes = send(targs->clientfd, msgin, strlen(msgin), 0);
        if (nbytes == 0) {
            write(STDOUT_FILENO, CONN_LOST_MSG, strlen(CONN_LOST_MSG));
            flag = true;
            pthread_cond_signal(&cond);
            break;
        }

        if (nbytes == -1) {
            write(STDOUT_FILENO, FAILED_MSG, strlen(FAILED_MSG));
            continue;
        }

        if (memcmp(msgin, EXIT, strlen(EXIT)) == 0) {
            flag = true;
            pthread_cond_signal(&cond);
            break;
        }
        memset(msgin, 0, sizeof(msgin));
    }

    return (void *) 0;
}

// thread function that handles recieving other client's messages
// from the server
//
// args : thread function arguments
//
void *handle_recv_msg(void *args) {
    thread_args_t *targs = args;
    char msgin[KB + BLOCK] = { 0 };
    int nbytes = 0;

    while(true) {
        nbytes = recv(targs->clientfd, msgin, KB + BLOCK, 0);
        if (nbytes == 0) {
            write(STDOUT_FILENO, CONN_LOST_MSG, strlen(CONN_LOST_MSG));
            flag = true;
            pthread_cond_signal(&cond);
            break;
        }

        if (nbytes == -1) {
            write(STDOUT_FILENO, LOST_MSG, strlen(LOST_MSG));
            continue;
        }

        write(STDOUT_FILENO, msgin, strlen(msgin));
        memset(msgin, 0, sizeof(msgin));
    }

    return (void *) 0;
}

// outputs the program usage to a specified file stream
//
// stream : file stream
// exec   : program's exec name
//
void program_usage(FILE *stream, char *exec) {
    fprintf(stream, 
        "SYNOPSIS\n"
        "A client that connects to a chat room that runs on localhost\n"
        "\n"
        "USAGE\n"
        "   %s [-h] [-u username] [-p portnumber]\n"
        "\n"
        "OPTIONS\n"
        "   -h               displays a help menu for users\n"
        "   -u username      username for chat room\n"
        "   -p portnumber    connecting port for server connection\n",
        exec);
}

int main(int argc, char *argv[]) {
    int opt, portnumber;
    char *username = NULL;
    int nbytes;

    while ((opt = getopt(argc, argv, "h:p:u:")) != -1) {
        switch (opt) {
        case 'h': program_usage(stdout, argv[0]); exit(EXIT_SUCCESS);
        case 'p': portnumber = strtouint16(optarg); break;
        case 'u': username = optarg; break;
        default: program_usage(stderr, argv[0]); exit(EXIT_FAILURE);
        }
    }

    if (!username) {
        errx(EXIT_FAILURE, "no username was provided");
    }

    int clientfd = create_client_socket(portnumber);
    if (clientfd < 0) {
        errx(EXIT_FAILURE, CONN_IMP_MSG);
    }

    // send user's name to server
    nbytes = send(clientfd, username, strlen(username), 0);
    if (nbytes <= 0) {
        close(clientfd);
        errx(EXIT_FAILURE, CONN_LOST_MSG);
    }

    // recieve the welcome message for the server
    char welcomemsg[KB] = { 0 };
    nbytes = recv(clientfd, welcomemsg, KB, 0);
    if (nbytes <= 0) {
        close(clientfd);
        errx(EXIT_FAILURE, CONN_LOST_MSG);
    }
    write(STDOUT_FILENO, welcomemsg, strlen(welcomemsg));

    // create the threads for recieving and sending messages
    thread_args_t *args = calloc(1, sizeof(thread_args_t));
    args->clientfd = clientfd;

    pthread_t client_tids[2] = { 0 };
    pthread_create(&client_tids[0], NULL, handle_send_msg, (void *) args);
    pthread_create(&client_tids[1], NULL, handle_recv_msg, (void *) args);

    // mutex and condition wait used for purpose of reducing CPU usage
    while(true) {
        pthread_mutex_lock(&mutex);
        if (!flag) {
            pthread_cond_wait(&cond, &mutex);
            if (flag) {
                break;
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    close(clientfd);
    return EXIT_SUCCESS;
}
