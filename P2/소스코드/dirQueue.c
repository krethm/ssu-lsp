#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dirQueue.h"
// 초기화
void QueueInit(Queue *pq)
{
    pq->front = NULL;
    pq->rear = NULL;
}
// 비어있는지 여부
int QIsEmpty(Queue *pq)
{
    return (pq->front == NULL) ? TRUE : FALSE;
}
// enqueue
void Enqueue(Queue *pq, char *dir)
{
    QNode *newNode = (QNode*)malloc(sizeof(QNode));
    if (newNode == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    newNode->next = NULL;
    strcpy(newNode->dirpath, dir);

    if (QIsEmpty(pq)) { // 비어있던 경우 front, rear 모두 새 node 포인터 참조
        pq->front = newNode;
        pq->rear = newNode;
    }
    else { // 비어있던 경우X
        pq->rear->next = newNode;
        pq->rear = newNode;
    }
}
// dequeue
char *Dequeue(Queue *pq)
{
    QNode *delNode = NULL;
    static char dir[PATH_MAX]; // dequeue 될 디렉토리 경로

    if (QIsEmpty(pq)) {
        printf("Queue Memory Error!\n");
        exit(1);
    }

    delNode = pq->front;
    strcpy(dir, delNode->dirpath);
    pq->front = pq->front->next;

    free(delNode);
    return dir; // 경로 return
}