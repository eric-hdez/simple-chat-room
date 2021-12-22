#include "list.h"

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

#define USER_LIMIT    10
#define KB            1024
#define BLOCK         4096
#define MSG_FORMAT    "%s: %s"
#define BEGIN_MSG     "============== Chat Room ==============\n"
#define CONN_MSG      ">> %s has connected!\n"
#define WELCOME_MSG   "Welcome to the chat room!\n"
#define CONN_FAIL_MSG "Could not connect. User limit has been met.\n"
#define FAIL_MSG      "Your message failed to send.\n"
#define LEFT_MSG      ">> %s has disconnected.\n"
#define EXIT_MSG      "exit"

pthread_mutex_t users_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_args_t { 
    int connfd; 
    UserList *users;
} thread_args_t;

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

// creates and binds a socket to a port for connection
// listening
//
// port : portnumber to bind socket
//
int create_listen_socket(uint16_t port) {
    struct sockaddr_in addr;
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1) {
        err(EXIT_FAILURE, "socket error");
    }

    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        err(EXIT_FAILURE, "setsockopt error");
    }

    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htons(INADDR_ANY);  //inet_addr(ip_addr);
    addr.sin_port = htons(port);
    if (bind(listenfd, (struct sockaddr *) &addr, sizeof addr) == -1) {
        err(EXIT_FAILURE, "bind error");
    }

    if (listen(listenfd, 1000) == -1) {
        err(EXIT_FAILURE, "listen error");
    }

    return listenfd;
}

// broadcasts a user's message to all the clients in the chatroom
//
// userfd : descriptor of sending user's socket connection
// users  : a list of users connected to the server
// msg    : the message to be broadcasted
//
void broadcast_msg(int userfd, UserList *users, char *msg) {
    pthread_mutex_lock(&users_mutex);
    for (Node *n = users->head->next; n != users->tail; n = n->next) {
        if (n->userfd != userfd) {
            int nbytes = send(n->userfd, msg, strlen(msg), 0);
            if (nbytes == 0) {
                Node *last = n->prev;
                delete_user(users, n->userfd);
                n = last;
            }
        }  
    }
    pthread_mutex_unlock(&users_mutex);
}

// thread function where a thread handles the connection of a 
// client to this server
//
// args : thread function arguments
//
void *handle_user_conn(void *args) {
    thread_args_t *targs = args;
    char msgbuf[BLOCK] = { 0 };
    char username[KB] = { 0 };
    char msgout[KB + BLOCK] = { 0 };
    int nbytes = 0;

    // receive the user's username
    nbytes = recv(targs->connfd, username, KB, 0);
    if (nbytes == 0 || nbytes == -1) {
        pthread_mutex_lock(&users_mutex);
        delete_user(targs->users, targs->connfd);
        pthread_mutex_unlock(&users_mutex);
        close(targs->connfd);
        return (void *) 0;
    }

    // print a connected message for the user and welcome them
    sprintf(msgout, CONN_MSG, username);
    write(STDOUT_FILENO, msgout, strlen(msgout));
    memset(msgout, 0, sizeof(msgout));
    sprintf(msgout, "%s%s", BEGIN_MSG, WELCOME_MSG);
    send(targs->connfd, msgout, strlen(msgout), 0);

    // recieve messages and broadcast them until the user disconnects
    do {
        nbytes = recv(targs->connfd, msgbuf, BLOCK, 0);
        if (nbytes == 0 || memcmp(msgbuf, EXIT_MSG, strlen(EXIT_MSG)) == 0) {
            pthread_mutex_lock(&users_mutex);
            delete_user(targs->users, targs->connfd);
            pthread_mutex_unlock(&users_mutex);
            break;
        }

        if (nbytes == -1) {
            send(targs->connfd, FAIL_MSG, strlen(FAIL_MSG), 0);
            continue;
        }
        
        sprintf(msgout, MSG_FORMAT, username, msgbuf);
        write(STDOUT_FILENO, msgout, strlen(msgout));
        broadcast_msg(targs->connfd, targs->users, msgout);
        memset(msgbuf, 0, sizeof(msgbuf));
        memset(msgout, 0, sizeof(msgout));
    } while (true);

    // user is disconnecting
    sprintf(msgout, LEFT_MSG, username);
    write(STDOUT_FILENO, msgout, strlen(msgout));
    close(targs->connfd);
    free(targs);
    return (void *) 0;
}

void program_usage(FILE *stream, char *exec) {
    fprintf(stream, 
        "SYNOPSIS\n"
        "Simple Chat room in C that runs on localhost\n"
        "\n"
        "USAGE\n"
        "   %s [-p portnumber]\n"
        "\n"
        "OPTIONS\n"
        "   -h               displays a help menu for users\n"
        "   -p portnumber    listening port for client connections\n",
        exec);
}

int main(int argc, char *argv[]) {
    int opt, listenfd;
    uint16_t portnumber;

    while((opt = getopt(argc, argv, "h:p:")) != -1) {
        switch (opt) {
        case 'h': program_usage(stdout, argv[0]); exit(EXIT_SUCCESS);
        case 'p': portnumber = strtouint16(optarg); break;
        default: program_usage(stderr, argv[0]); exit(EXIT_FAILURE);
        }
    }

    if (argc < 2) {
        program_usage(stderr, argv[0]);
        exit(EXIT_FAILURE);
    }

    if (portnumber == 0) {
        errx(EXIT_FAILURE, "invalid portnumber");
    }

    listenfd = create_listen_socket(portnumber);
    UserList *users = new_user_list();
    write(STDOUT_FILENO, BEGIN_MSG, strlen(BEGIN_MSG));

    while (true) {
        int connfd = accept(listenfd, NULL, NULL);
        if (connfd == -1) {
            warn("error accepting user connection");
            continue;
        }

        if (user_list_length(users) == USER_LIMIT) {
            send(connfd, CONN_FAIL_MSG, strlen(CONN_FAIL_MSG), 0);
            close(connfd);
            continue;
        }

        insert_user(users, connfd);
        thread_args_t *args = calloc(1, sizeof(thread_args_t));
        args->connfd = connfd;
        args->users = users;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_user_conn, (void *) args);
    }

    return EXIT_SUCCESS;
}
