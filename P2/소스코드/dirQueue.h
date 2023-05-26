#ifndef DIRQUEUE
#define DIRQUEUE

#define TRUE    1
#define FALSE   0

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
/* 디렉토리를 BFS 탐색을 위한 Queue를 linked list로 구현 */
typedef struct _qnode {
    char dirpath[PATH_MAX]; // 디렉토리 경로
    struct _qnode *next; // 다음 node
} QNode;

typedef struct _Queue {
    QNode *front; // dequeue
    QNode *rear; // enqueue
} Queue;

void QueueInit(Queue *pq); // 초기화
int QIsEmpty(Queue *pq); // 비어있는지 여부

void Enqueue(Queue *pq, char *dir); // enqueue
char *Dequeue(Queue *pq); // dequeue

#endif