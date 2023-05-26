#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "linkedlist.h"

// linked list 초기화
void ListInit(List *plist, int (*hcomp)(HData d1, HData d2), int (*pcomp)(LData d1, LData d2))
{
	plist->head = (HNode*)malloc(sizeof(HNode)); // head는 dummy node를 가르킴
	if (plist->head == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	plist->head->down = NULL;
	plist->hcomp = hcomp;
	plist->pcomp = pcomp;
}
// 탐색된 파일과 같은 hash값을 갖는 파일 세트가 있는지 확인
int FindSamefile(List *plist, char *hash)
{
	plist->add = plist->head->down;
	if (plist->add == NULL) // 아직 파일 세트가 0개인 경우
		return FALSE;

	while ((plist->add != NULL) && strcmp(plist->add->data.hash, hash) != 0)
		plist->add = plist->add->down;

	if (plist->add == NULL) // 없을 경우
		return FALSE;
	else // 같은 hash값을 갖는 파일 세트가 있을 경우 add는 그 파일 세트를 가르킴
		return TRUE;
}
// 새로운 파일 세트를 생성
void HInsert(List *plist, char *hash, off_t size)
{
	// 새로운 파일 세트를 생성하고 slot에 데이터 저장
	HNode *newNode = (HNode*)malloc(sizeof(HNode));
	if (newNode == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	strcpy(newNode->data.hash, hash);
	newNode->data.size = size;
	newNode->data.numOfData = 0;
	
	if (plist->head->down == NULL) { // 첫 파일 세트인 경우
		plist->head->down = newNode;
		newNode->down = NULL;
		newNode->up = plist->head;
	}
	else { // 첫 파일 세트가 아닌 경우
		HNode *pred = plist->head->down;
		HNode *before = plist->head;
		while (pred != NULL && plist->hcomp(newNode->data, pred->data) != 0) { // 정렬 기준에 따라 파일 세트가 추가될 위치 지정
			pred = pred->down;
			before = before->down;
	    }

		if (pred == NULL) { // 제일 끝(아래)에 추가될 경우
			before->down = newNode;
			newNode->up = before;
			newNode->down = NULL;
		}
		else { // 기존 파일 세트들 중간에 추가될 경우
			newNode->down = pred;
			newNode->up = before;
			before->down = newNode;
			pred->up = newNode;
		}
	}
	plist->add = newNode; // 새로 만든 파일 세트에 파일 저장을 위해 add를 위치시킴
}
// 파일 경로 depth 구하는 함수('/'의 개수)
int get_depth(char *path)
{
	size_t len = strlen(path);
	int cnt = 0;

	for (int i = 0; i < len; i++) {
		if (path[i] == '/')
			cnt++;
	}

	return cnt;
}
// 파일 세트에 첫 파일을 추가
void FInsert(List *plist, char *fpath, time_t atime, time_t mtime)
{
	Node *newNode = (Node*)malloc(sizeof(Node));
	if (newNode == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}

	strcpy(newNode->data.path, fpath);
	newNode->data.depth = get_depth(fpath);
	newNode->data.atime = atime;
	newNode->data.mtime = mtime;

	newNode->next = NULL;
	newNode->prev = NULL;
	plist->add->next = newNode;

	(plist->add->data.numOfData)++;
}
// 파일 세트에 추가될 파일이 처음이 아닌 경우
void NInsert(List *plist, char *fpath, time_t atime, time_t mtime)
{
	/* 새 파일의 node 생성 후 slot에 데이터 저장 */
	Node *newNode = (Node*)malloc(sizeof(Node));
	if (newNode == NULL) {
		fprintf(stderr, "malloc error\n");
		exit(1);
	}
	strcpy(newNode->data.path, fpath);
	newNode->data.depth = get_depth(fpath);
	newNode->data.atime = atime;
	newNode->data.mtime = mtime;

	/* 정렬 기준에 따른 새 파일의 node가 연결될 위치 찾기 */
	Node *pred = plist->add->next;
	Node *before = plist->add->next;

	while (pred != NULL && plist->pcomp(newNode->data, pred->data) != 0) {
		if (pred != plist->add->next)
			before = before->next;
		pred = pred->next;
	}

	if (pred == NULL) { // 맨 마지막인 경우
		before->next = newNode;
		newNode->prev = before;
		newNode->next = NULL;
	}
	else if (pred == plist->add->next) { // 맨 처음인 경우
		newNode->next = pred;
		newNode->prev = NULL;
		pred->prev = newNode;
		plist->add->next = newNode;
	}
	else { // 기존 node들의 사이에 추가될 경우
		newNode->next = pred;
		newNode->prev = before;
		before->next = newNode;
		pred->prev = newNode;
	}

	(plist->add->data.numOfData)++;
}
// 중복 파일이 없는 파일 세트 지우는 함수
void Remove_Nodup(List *plist)
{
	HNode * target = plist->head->down;
	HNode * remove = NULL;
	while (target != NULL) {
		// 중복 파일 세트의 파일 개수가 1개이거나 0개인 경우 -> 삭제
		if (target->data.numOfData == 1 || target->data.numOfData == 0) {
			if (target->data.numOfData == 1) { // 1개인 경우 그 파일의 node free
				free(target->next);
			}
			/* 파일 세트의 hnode 삭제 */
			if (target->down != NULL) { // 맨 마지막(맨 아래)에 위치한 파일 세트인 경우
				target->up->down = target->down;
				target->down->up = target->up;
			}
			else // 파일 세트들 사이에 있는 경우
				target->up->down = NULL;

			remove = target;
			target = target->down;
			free(remove);
			remove = NULL;
		}
		else
			target = target->down; // 다음 타겟
	}
}
// 파일 크기를 천 단위마다 ','로 구분하여 표현하기 위한 함수
char *numbercomma(off_t n)
{
	static char comma_str[100];
	char str[65];
	int idx, len, mod;
	int cidx = 0;
	
	sprintf(str, fm_off_t, n);
	len = strlen(str);
	mod = len % 3;

	for (int i = 0; i < len; i++) {
		if (i != 0 && (i % 3 == mod)) {
			comma_str[cidx++] = ',';
		}
		comma_str[cidx++] = str[i];
	}
	comma_str[cidx] = '\0';
	
	return comma_str;
}
// 중복 파일 리스트를 출력하는 함수
int ListPrint(List *plist)
{
	int tableidx = 1; // 테이블 index
	int fileidx; // 각 테이블에 속한 파일의 index
	char mtime[20]; // 변형된 수정시간
	char atime[20]; // 변형된 접근시간
	struct tm timeinfo;

	if (plist->head->down == NULL) // 중복 파일 세트가 없다면 FALSE return
		return FALSE;
	
	plist->hcur = plist->head->down;
	
	while (plist->hcur != NULL) {
		printf("---- Identical files #%d (%s bytes - %s) ----\n",
				tableidx, numbercomma(plist->hcur->data.size), plist->hcur->data.hash);

		plist->pcur = plist->hcur->next;
		fileidx = 1;
		while (plist->pcur != NULL) {
			strftime(mtime, 20, "%Y-%m-%d %H:%M:%S", localtime_r(&(plist->pcur->data.mtime), &timeinfo));
			strftime(atime, 20, "%Y-%m-%d %H:%M:%S", localtime_r(&(plist->pcur->data.atime), &timeinfo));
			printf("[%d] %s (mtime : %s) (atime : %s)\n", fileidx, plist->pcur->data.path, mtime, atime);
			plist->pcur = plist->pcur->next;
			fileidx++;
		}
		printf("\n");
		plist->hcur = plist->hcur->down;
		tableidx++;
	}
	return TRUE;
}
// 옵션에 따른 해당 파일의 node를 삭제하기 위한 함수
char *Node_Remove(List *plist, Node *delnode)
{	// 삭제될 파일의 경로 저장 -> 리턴
	static char delpath[PATH_MAX];
	strcpy(delpath, delnode->data.path);
	
	if (delnode->next == NULL && delnode->prev == NULL) // 파일 세트에 하나뿐인 파일인 경우
		plist->hcur->next = NULL;
	else if (delnode->next == NULL) // 가장 마지막에 위치한 경우
		delnode->prev->next = NULL;
	else if (delnode->prev == NULL) { // 가장 처음 위치한 경우
		delnode->next->prev = NULL;
		plist->hcur->next = delnode->next;
	}
	else { // 사이에 위치한 경우
		delnode->prev->next = delnode->next;
		delnode->next->prev = delnode->prev;
	}
	free(delnode);
	delnode = NULL;

	return delpath;
}
// 프로그램이 종료되기 전에 모든 linked list를 제거하기 위한 함수
void LRemoveall(List *plist)
{
	HNode *hremove = NULL;
	Node *remove = NULL;
	plist->hcur = plist->head->down;

	while (plist->hcur != NULL) {
		plist->pcur = plist->hcur->next;
		while (plist->pcur != NULL) {
			remove = plist->pcur;
			plist->pcur = plist->pcur->next;
			free(remove);
			remove = NULL;
		}
		hremove = plist->hcur;
		plist->hcur = plist->hcur->down;
		free(hremove);
		hremove = NULL;
	}
	free(plist->head);
	plist->head = NULL;
}