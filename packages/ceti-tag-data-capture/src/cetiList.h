#ifndef _CETI_LIST_H
#define _CETI_LIST_H
#include <stdbool.h>

struct listItem {
    void * data;
    struct listItem * next;
};

static struct listItem * listHead = NULL;
static struct listItem * listTail = NULL;

bool listEmpty() {
    return listHead == NULL;
}

void listPushBack(void * data) {
    struct listItem * newItem = (struct listItem *) malloc(sizeof(struct listItem));
    newItem->data = data;

    if (!listEmpty()) {
        listHead->next = newItem;
    } else {
        listTail = newItem;
    }
    listHead = newItem;
}

void * listPopFront() {
    void * data = NULL;
    struct listItem * currentItem = listTail;
    if (listTail) {
        data = listTail->data;
    }
    listTail = listTail->next;
    free(currentItem);
    return data;
}

void debugPrintList() {
    struct listItem * curr = listTail;
    fprintf(stderr, "=============\naudioFileList {");
    while (curr) {
        fprintf(stderr, "%s,\n", (char*)curr->data);
        curr = curr->next;
    }
    fprintf(stderr, "}\n=============\n");
}

#endif