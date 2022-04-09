#ifndef _CETI_LIST_H
#define _CETI_LIST_H
#include <stdbool.h>
#include <pthread.h>

struct listItem {
    void * data;
    struct listItem * next;
};

static struct listItem * listHead = NULL;
static struct listItem * listTail = NULL;
pthread_mutex_t list_mutex;

bool listEmpty() {
    return listHead == NULL;
}

void listPushBack(void * data) {
    struct listItem * newItem = (struct listItem *) malloc(sizeof(struct listItem));
    newItem->data = data;
    newItem->next = NULL;

    pthread_mutex_lock(&list_mutex);
    if (!listEmpty()) {
        listHead->next = newItem;
    } else {
        listTail = newItem;
    }
    listHead = newItem;
    pthread_mutex_unlock(&list_mutex);
}

void * listPopFront() {
    void * data = NULL;
    pthread_mutex_lock(&list_mutex);
    struct listItem * currentItem = listTail;
    if (listTail) {
        data = listTail->data;
    }
    if (listTail == listHead) {
        listHead = listHead->next;
    }
    listTail = listTail->next;
    pthread_mutex_unlock(&list_mutex);
    free(currentItem);
    return data;
}

void debugPrintList() {
    pthread_mutex_lock(&list_mutex);
    struct listItem * curr = listTail;
    fprintf(stderr, "=============\naudioFileList\n{\n");
    while (curr) {
        fprintf(stderr, "0x%x %s,\n", (int)curr, (char*)curr->data);
        curr = curr->next;
    }
    fprintf(stderr, "}\n=============\n");
    pthread_mutex_unlock(&list_mutex);
}

#endif
