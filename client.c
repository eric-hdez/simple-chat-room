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

#define BLOCK 4096
#define KB    1024
#define EXIT  "exit"

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_args_t {
    int clientfd;
} thread_args_t;

bool flag = false;

uint16_t strtouint16(char *number) {
    char *last;
    long num = strtol(number, &last, 10);
    if (num <= 0 || num > UINT16_MAX || *last != '\0') {
        return 0;
    }

    return num;
}

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

void *handle_send_msg(void *args) {
    thread_args_t *targs = args;
    char msgin[BLOCK] = { 0 };

    while(true) {
        fgets(msgin, BLOCK, stdin);
        send(targs->clientfd, msgin, strlen(msgin), 0);
        if (memcmp(msgin, EXIT, strlen(EXIT)) == 0) {
            pthread_mutex_lock(&mutex);
            pthread_cond_signal(&cond);
            flag = true;
            pthread_mutex_unlock(&mutex);
            break;
        }
        memset(msgin, 0, sizeof(msgin));
    }

    return (void *) 0;
}

void *handle_recv_msg(void *args) {
    thread_args_t *targs = args;
    char msgin[KB + BLOCK] = { 0 };

    while(true) {
        recv(targs->clientfd, msgin, KB + BLOCK, 0);
        write(STDOUT_FILENO, msgin, strlen(msgin));
        memset(msgin, 0, sizeof(msgin));
    }

    return (void *) 0;
}

int main(int argc, char *argv[]) {
    int option, portnumber;
    char *username = NULL;

    while ((option = getopt(argc, argv, "p:u:")) != -1) {
        switch (option) {
        case 'p': portnumber = strtouint16(optarg); break;
        case 'u': username = optarg; break;
        default: exit(EXIT_FAILURE);
        }
    }

    if (!username) {
        errx(EXIT_FAILURE, "no username was provided");
    }

    int clientfd = create_client_socket(portnumber);

    int nbytes = send(clientfd, username, strlen(username), 0);
    if (nbytes == 0 || nbytes == -1) {
        close(clientfd);
        return EXIT_SUCCESS;
    }

    char buf[KB] = { 0 };
    recv(clientfd, buf, KB, 0);
    write(STDOUT_FILENO, buf, strlen(buf));

    thread_args_t *args = calloc(1, sizeof(thread_args_t));
    args->clientfd = clientfd;

    pthread_t client_tids[2] = { 0 };
    pthread_create(&client_tids[0], NULL, handle_send_msg, (void *) args);
    pthread_create(&client_tids[1], NULL, handle_recv_msg, (void *) args);

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
