#include "list.h"

#include <stdlib.h>

Node *new_node(int userfd) {
    Node *n = (Node *) malloc(sizeof(Node));
    if (!n) {
        return (Node *) 0;
    }

    n->userfd = userfd;
    n->next = n->prev = (Node *) 0;

    return n;
}

void delete_node(Node **n) {
    if (n && *n) {
        free(*n);
        *n = (Node *) 0;
    }
}

UserList *new_user_list() {
    UserList *ul = (UserList *) malloc(sizeof(UserList));
    if (!ul) {
        return (UserList *) 0;
    }

    ul->head = new_node(-1);
    ul->tail = new_node(-1);
    if (!ul->head || !ul->tail) {
        return (UserList *) 0;
    }

    ul->head->next = ul->tail;
    ul->tail->prev = ul->head;
    ul->length = 0;
    return ul;
}

void delete_user_list(UserList **ul) {
    if (ul && *ul) {
        Node *curr = (*ul)->head;
        while (curr) {
            Node *delnext = curr->next;
            delete_node(&curr);
            curr = delnext;
        }

        free(*ul);
        *ul = (UserList *) 0;
    }
}

int user_list_length(UserList *ul) {
    return ul->length;
}
    
void insert_user(UserList * ul, int connfd) {
    Node *n = new_node(connfd);

    n->prev = ul->head;
    n->next = ul->head->next;
    n->prev->next = n;
    n->next->prev = n;

    ul->length++;
}

Node *lookup_user(UserList *ul, int connfd) {
    for (Node *curr = ul->head->next; curr != ul->tail; curr = curr->next) {
        if (curr->userfd == connfd) {
            return curr;
        }
    }

    return (Node *) 0;
}

void delete_user(UserList *ul, int connfd) {
    for (Node *curr = ul->head->next; curr != ul->tail; curr = curr->next) {
        if (curr->userfd == connfd) {
            curr->prev->next = curr->next;
            curr->next->prev = curr->prev;
            delete_node(&curr);
            ul->length--;
            return;
        }
    }
}
