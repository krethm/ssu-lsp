#ifndef LINKEDLIST
#define LINKEDLIST

#define TRUE	1
#define FALSE	0
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#if _FILE_OFFSET_BITS == 64
#define fm_off_t "%lld"
#else
#define fm_off_t "%ld"
#endif

/* 중복 파일 리스트를 2차원 linked list로 구현
   세로 방향으로 HNode : 각 중복 파일 세트
   가로 방향으로 Node : 한 중복 파일 세트의 각 파일들 */
typedef struct _slot { // 각 파일의 정보 Node slot
	char path[PATH_MAX]; // 경로
	int depth; // root로 부터의 depth
	time_t atime; // 접근 시간
	time_t mtime; // 수정 시간
	uid_t uid; // uid
	gid_t gid; // gid
	mode_t mode; // mode
} Slot;
typedef Slot LData;

typedef struct _hslot { // 각 중복 파일 세트의 정보 HNode slot
	unsigned char hash[41]; // sha1
	off_t size; // 파일 크기
	int numOfData; // 각 세트의 중복 파일 개수
} HSlot;
typedef HSlot HData;

typedef struct _node { // 각 파일의 Node
	LData data; //data slot
	struct _node *prev; // 이전 Node 포인터
	struct _node *next; // 다음 Node 포인터
} Node;

typedef struct _hnode { // 각 중복 파일 세트의 정보 HNode 
	HData data; // data slot
	struct _hnode *up; // 윗 번호의 중복 파일 세트
	struct _hnode *down; // 아래 번호의 중복 파일 세트
	struct _node *next; // 가로 방향으로 중복 파일들의 첫 파일
} HNode;

typedef struct _linkedList { // linked list
	HNode *head; // head -> dummy node
	HNode *hcur; // 세로 방향, 각 파일 세트 대상으로 하는 포인터
	HNode *add; // 중복 파일을 추가할 파일 세트 포인터
	Node *pcur; // 세로방향, 파일 세트에 속하느 파일들을 대상으로 하는 포인터
	int (*hcomp)(HData d1, HData d2); // 중복 파일 세트 정렬 함수
	int (*pcomp)(LData d1, LData d2); // 각 중복 파일 세트에 속하는 파일들 정렬 함수
} List;

void ListInit(List *plist, int (*hcomp)(HData d1, HData d2),int (*pcomp)(LData d1, LData d2)); // 초기화
int FindSamefile(List *plist, char *hash); // 탐색된 파일과 같은 hash값을 갖는 파일 세트가 있는지 확인
void HInsert(List *plist, char *hash, off_t size); // 다른 hash를 갖는 파일을 저장하기 위한 파일 세트 추가
void FInsert(List *plist, char *fpath, struct stat *statbuf); // 각 파일 세트에 첫 파일 저장
void NInsert(List *plist, char *fpath, struct stat *statbuf); // 각 파일 세트에 두번째 이후의 파일 저장
char *Node_Remove(List *plist, Node *delnode); // Node 삭제
void Remove_Nodup(List *plist); // 중복 파일이 없는 파일 세트 삭제
void ListSort(List *plist, char *list_type, char *category, char *order); // 파일 리스트 정렬
int ListPrint(List *plist); // 중복 파일 리스트 출력
void LRemoveall(List *plist); // 모든 node free

#endif
