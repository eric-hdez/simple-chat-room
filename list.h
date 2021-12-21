#ifndef __USER_LIST__
#define __USER_LIST__

typedef struct Node Node;

struct Node {
    int userfd;
    Node *next, *prev;  
};

typedef struct UserList UserList;

struct UserList {
    Node *head, *tail;
    int length;
};

Node *new_node(int userfd);

void delete_node(Node **n);

typedef struct LinkedList LinkedList;

UserList *new_user_list();

void delete_user_list(UserList **l);

int user_list_length(UserList *l);

void insert_user(UserList *l, int userfd);

Node *lookup_user(UserList *l, int userfd);

void delete_user(UserList *l, int userfd);

#endif
