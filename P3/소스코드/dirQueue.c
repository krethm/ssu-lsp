#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "dirQueue.h"
// 초기화
void QueueInit(Queue *pq)
{
    pq->head = NULL;
    pq->tail = NULL;
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
    /* 새 node 생성 */
    QNode *newNode = (QNode*)malloc(sizeof(QNode));
    if (newNode == NULL) {
        fprintf(stderr, "malloc error\n");
        exit(1);
    }
    newNode->next = NULL;
    strcpy(newNode->dirpath, dir);
    /* node 연결 */
    pthread_mutex_lock(&lock); // mutex lock
    if (QIsEmpty(pq)) { // 비어있던 경우 front, rear 모두 새 node 포인터 참조
        pq->front = newNode;
        pq->rear = newNode;
        if (pq->tail != NULL) // 이전에 Queue의 제일 끝 node를 가리키던 tail은 새 node와 연결된다.
            pq->tail->next = pq->front;
        else // Queue가 초기화되고 처음 Enqueue 되었을 때
            pq->head = pq->front; // head를 Queue의 첫 node를 가리키게 한다.
        pq->tail = pq->front; // tail은 다시 제일 끝 node를 가리킨다.
    }
    else { // 비어있던 경우X
        pq->rear->next = newNode;
        pq->rear = newNode;
        pq->tail = pq->rear;
    }
    pthread_cond_signal(&not_empty); // 새 데이터 queue에 들어옴 -> dequeue 대기하는 쓰레드에 cond signal
    pthread_mutex_unlock(&lock); // mutex unlock
}
// dequeue
char *Dequeue(Queue *pq)
{
    pthread_mutex_lock(&lock); // mutex lock
    while (QIsEmpty(pq)) { // 비어있는 경우 쓰레드 대기
        pthread_cond_wait(&not_empty, &lock);
        pthread_testcancel(); // 쓰레드 cancel 시점 지정
    }
    pthread_testcancel(); // 쓰레드 cancel 시점 지정
    thread_counter++; // counter + 1
    QNode *delNode = NULL;

    delNode = pq->front;
    pq->front = pq->front->next;

    pthread_mutex_unlock(&lock); // mutex unlock
    char *tmp = delNode->dirpath;
    return tmp; // 경로 return
}
// queue heap 메모리 사용 해제
void clean_queue(Queue *pq)
{
    QNode *remove;
    while (pq->head != NULL) {
        remove = pq->head;
        pq->head = pq->head->next;
        free(remove);
    }
}
