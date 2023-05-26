#ifndef DIRQUEUE
#define DIRQUEUE

#define TRUE    1
#define FALSE   0

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

pthread_mutex_t lock; // enqueue, dequeue시 mutex 사용
pthread_cond_t not_empty; // enqueue하고 dequeue를 대기중인 다른 쓰레드에게 signal 보내기 위해 사용
int thread_counter; // 파일 탐색이 종료되었는지 구분하기 위해 사용

/* 디렉토리를 BFS 탐색을 위한 Queue를 linked list로 구현 */
typedef struct _qnode {
    char dirpath[PATH_MAX]; // 디렉토리 경로
    struct _qnode *next; // 다음 node
} QNode;

typedef struct _Queue {
    QNode *front; // dequeue
    QNode *rear; // enqueue
    QNode *head; // Queue의 맨 앞 노드를 가리킨다. clean_queue()를 위해 사용
    QNode *tail; // Queue의 맨 뒤 노드를 가리킨다. clean_queue()를 위해 사용
} Queue;

void QueueInit(Queue *pq); // 초기화
int QIsEmpty(Queue *pq); // 비어있는지 여부

void Enqueue(Queue *pq, char *dir); // enqueue
char *Dequeue(Queue *pq); // dequeue
void clean_queue(Queue *pq); // clean(free)

#endif
