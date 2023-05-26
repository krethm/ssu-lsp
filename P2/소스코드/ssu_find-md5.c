#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <openssl/md5.h>
#include "linkedlist.h"
#include "dirQueue.h"

#define BUFSIZE 1024
#define FILE_MAX_LEN 255 // 파일명 최대 길이
#define TRASH_DIR ".local/share/Trash/files" // 휴지통 경로

bool exitFlag = FALSE; // 프로그램 종료 flag
char target_directory[PATH_MAX];  // 탐색할 디렉토리
char file_extension[FILE_MAX_LEN]; // 탐색할 파일 확장자

List filelist; // 중복 파일 리스트 생성
Queue dirlist; // BFS 탐색 위한 디렉토리 Queue 생성

void SearchFiles(int argc, char *argv[], char *target_directory); // 파일 탐색
void Addlist(char *path, off_t size, time_t atime, time_t mtime); // 중복 파일 리스트에 파일 추가
void Remove_dup(void); // 옵션에 따른 파일 삭제

int checking_index(char *set_index); // index 정상 입력 여부 확인
int checking_option(char *op); // option 정상 입력 여부 확인
int checking_list_idx(char *list_idx); // d or m 옵션의 list idx 정상 입력 여부 확인

int d_option(char *set_index, char *list_idx); // d 옵션 실행
int m_option(char *set_index, char *list_idx, char *pRename_path); // m 옵션 실행
void i_option(void); // i 옵션 실행
void f_t_option(char *set_index, char *op); // f, t 옵션 실행
void e_option(char *set_index); // e 옵션 실행
void p_option(char *set_index); // p 옵션 실행
void size_option(void); // size 옵션

char *get_path_to_filename(char *path); // 경로에서 파일명 추출
char *get_extension(char *file_name); // 파일의 확장자 추출

/* 중복 파일들 정렬 함수 */
int file_precede(LData d1, LData d2)
{
	if (d1.depth < d2.depth) // depth 먼저 비교
		return 0;
	else if (d1.depth > d2.depth)
		return 1;
	else { // depth가 동일할 시 ASCII 비교
		int cmp = strcmp(d1.path, d2.path);
		if (cmp < 0)
			return 0;
		else
			return 1;
	}
}
/* 파일 세트 정렬 함수 */
int table_precede(HData d1, HData d2)
{
	if (d1.size <= d2.size) // 파일 사이즈 비교
		return 0;
	else
		return 1;
}

int main(int argc, char *argv[])
{
	struct timeval startTime, endTime;
	struct timeval timeDiff;

	if (argc != 5) {
		fprintf(stderr, "usage : %s <FILE_EXTENSION> <MINSIZE> <MAXSIZE> <TARGET_DIRECTORY>\n", argv[0]);
		exit(1);
	}
	strcpy(target_directory, argv[4]); // 입력받은 탐색 첫 타겟 -> target_directory

	if (strcmp(argv[1], "*") != 0) // 특정 확장자를 탐색하는 경우
		strcpy(file_extension, get_extension(argv[1])); // '*'을 제외한 확장자만 추출

	ListInit(&filelist, table_precede, file_precede); // linked list 초기화
	QueueInit(&dirlist); // queue 초기화
	
	gettimeofday(&startTime, NULL); // 탐색 시간 측정 시작
	while (1) {
        SearchFiles(argc, argv, target_directory); // 파일 탐색
		if (!QIsEmpty(&dirlist)) // queue에 디렉토리가 있다면,
			strcpy(target_directory, Dequeue(&dirlist)); // dequeue -> 타겟 재설정
		else // queue가 비어있다면 탈출
			break;
	}
	gettimeofday(&endTime, NULL); // 탐색 종료
	
	timeDiff.tv_sec = endTime.tv_sec - startTime.tv_sec;
	timeDiff.tv_usec = endTime.tv_usec - startTime.tv_usec;
	if (timeDiff.tv_usec < 0) {
		timeDiff.tv_sec--;
		timeDiff.tv_usec += 1000000;
	}

	Remove_Nodup(&filelist); // 중복 파일이 없는 파일 세트 모두 제거
	if (!ListPrint(&filelist)) { // 중복 파일 세트가 한 개도 없다면
        printf("No duplicates in %s\n", argv[4]);
		exitFlag = TRUE; // 프로그램 종료, 프롬프트로 복귀
	}
	printf("Searching time: %ld:%06ld(sec:usec)\n\n", timeDiff.tv_sec, timeDiff.tv_usec); // 탐색 시간 출력

	while (!exitFlag) { // exit 플래그가 FALSE인 동안 계속 반복
		Remove_dup(); // 옵션에 따른 파일 제거 함수 호출
		if (!exitFlag) {
			Remove_Nodup(&filelist); // 옵션 실행 후 중복 파일 리스트 업데이트
			if (filelist.head->down == NULL) { // 중복 파일 세트가 한 개도 남지 않았다면
				exitFlag = TRUE; // 프로그램 종료, 프롬프트 복귀
				printf("\n");
			}
			else { // 남아 있다면
				printf("\n");
				ListPrint(&filelist); // 중복 파일 리스트 재출력
			}
		}
	}

	LRemoveall(&filelist); // 모든 linked list의 node 제거(free)
	exit(0); // 종료, 프롬프트 복귀
}
/* 옵션에 따른 파일 제거 함수 */
void Remove_dup(void)
{
	char input[BUFSIZE];
	char *ptr;
	int argc;

	while (1) {
		char *argv[5] = {NULL, };
		ptr = NULL;
		argc = 0;

		printf(">> ");
		fgets(input, BUFSIZE, stdin);

		if (input[0] == '\n') { // 입력이 없다면 에러 처리 후 ">> "
			fprintf(stderr, "input error\n");
			continue;
		}

		input[strlen(input) - 1] = '\0'; // '\n' -> '\0'
		/* 모든 인자 split -> argv[] */
		ptr = strtok(input, " ");
		while (ptr != NULL) {
			argv[argc++] = ptr;
			ptr = strtok(NULL, " ");
		}
		/* 첫 인자가 "exit" -> 프롬프트 */
		if (!strcmp(argv[0], "exit")) {
			puts(">> Back to Prompt");
			exitFlag = TRUE;
			break;
		} /* 첫 인자가 "size" -> size option */
		else if (!strcmp(argv[0], "size")) {
			size_option();
			continue;
		}
		/* index 정상 입력 여부 확인 */
		if (!checking_index(argv[0])) {
			fprintf(stderr, "index error\n");
			continue;
		}
		/* option 정상 입력 여부 확인 */
		if (!checking_option(argv[1])) {
			fprintf(stderr, "option error\n");
			continue;
		}
		/* d, m 옵션이 아닌데 불필요한 인자가 입력되었을 경우 -> 에러 */
		if (((argv[1][0] != 'd') && (argv[1][0] != 'm')) && (argv[2] != NULL)) {
			fprintf(stderr, "input error\n");
			continue;
		} // 불필요한 인자 -> 에러
		else if (argv[1][0] == 'd' && argv[3] != NULL) {
			fprintf(stderr, "input error\n");
			continue;
		} // 불필요한 인자 -> 에러
		else if (argv[1][0] == 'm' && argv[4] != NULL) {
			fprintf(stderr, "input error\n");
			continue;
		} // 정상 입력
		else {
			if (argv[1][0] == 'd') { // d 옵션 실행
				if(!d_option(argv[0], argv[2])) {
					fprintf(stderr, "list index error\n"); // 실패 시 에러(list index 값이 유효x)
					continue;
				}
				else
					break;
			}
			else if (argv[1][0] == 'i') { // i 옵션 실행
				i_option();
				break;
			}
			else if (argv[1][0] == 'e') { // e 옵션 실행
				e_option(argv[0]);
				break;
			}
			else if (argv[1][0] == 'p') { // p 옵션 실행
				p_option(argv[0]);
				break;
			}
			else if (argv[1][0] == 'm') { // m 옵션 실행
				if(!m_option(argv[0], argv[2], argv[3])) {
					fprintf(stderr, "list index or rename error\n"); // 실패 시 에러
					continue;
				}
				else
					break;
			}
			else {
				f_t_option(argv[0], argv[1]); // f, t 옵션 실행
				break;
			}
		}
	}
}
/* 타겟 디렉토리 탐색 */
void SearchFiles(int argc, char *argv[], char *target_directory)
{
	int name_count = 0;
	char repath[PATH_MAX];
	char *extension = NULL;

	struct stat statbuf;
	struct dirent** name_list = NULL;

	name_count = scandir(target_directory, &name_list, NULL, alphasort);

	if (name_count < 0) {
		fprintf(stderr, "scandir error for %s\n", target_directory);
		exit(1);
	}

	for (int i = 0; i < name_count; i++) {
		/* '.', '..' 제외 */
		if (!strcmp(name_list[i]->d_name, ".") || !strcmp(name_list[i]->d_name, ".."))
			continue;
		/* 타겟 디렉토리에 파일 이름 combine */
		if (!strcmp(target_directory, "/"))
			sprintf(repath, "%s%s", target_directory, name_list[i]->d_name);
		else
			sprintf(repath, "%s/%s", target_directory, name_list[i]->d_name);
		/* /proc /run /sys 제외 */
		if (!strcmp(repath, "/proc") || !strcmp(repath, "/run") ||  !strcmp(repath, "/sys"))
			continue;
		
		if (lstat(repath, &statbuf) < 0) {
			fprintf(stderr, "lstat error for %s\n", repath);
			exit(1);
		}
		/* 디렉토리 -> enqueue */
		if (S_ISDIR(statbuf.st_mode)) {
			Enqueue(&dirlist, repath);
			continue;
		}
		/* 정규파일만 탐색 */
		if (!S_ISREG(statbuf.st_mode))
			continue;
		/* 사용자가 확장자를 지정한 경우 */
		if (strcmp(argv[1], "*") != 0) {
			if (strchr(name_list[i]->d_name, '.') == NULL) // 파일에 확장자가 없으면 제외
				continue;

			extension = get_extension(name_list[i]->d_name); // 확장자 추출
			if (strcmp(file_extension, extension) != 0) // 비교
				continue;
		}
		/* size가 0인 파일 제외 */
		if (statbuf.st_size == 0)
			continue;
		/* maxsize가 '~' 인 경우 */
		if (!strcmp(argv[3], "~")) {
			if ((double)statbuf.st_size < atof(argv[2])) // minsize보다 작으면 제외
				continue;
		}
		else { // maxsize가 지정된 경우
			if (((double)statbuf.st_size < atof(argv[2])) || ((double)statbuf.st_size > atof(argv[3])))
				continue;
		}
		/* linked list에 추가 */
		Addlist(repath, statbuf.st_size, statbuf.st_atime, statbuf.st_mtime);

	}
	for (int i = 0; i < name_count; i++) {
		free(name_list[i]);
		name_list[i] = NULL;
	}

	free(name_list);
	name_list = NULL;
}
/* 파일을 파일 리스트에 추가하는 함수 */
void Addlist(char *path, off_t size, time_t atime, time_t mtime)
{
	int fd;
	int length;
	MD5_CTX c;
	unsigned char buf[BUFSIZE];
	unsigned char md5[MD5_DIGEST_LENGTH];
	static char md5str[33];
	/* 파일의 md5 hash값을 구하는 과정 */
	MD5_Init(&c);

	if ((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "open error for %s\n", path);
		exit(1);
	}

	while ((length = read(fd, buf, BUFSIZE)) > 0)
		MD5_Update(&c, buf, (unsigned long)length);
	MD5_Final(&(md5[0]), &c);

	close(fd);
	/* string으로 변환 */
	for (int i = 0; i < MD5_DIGEST_LENGTH; i++)
		sprintf(&md5str[i*2], "%02x", (unsigned int)md5[i]);
	/* 파일 리스트에 추가 */
	if (!FindSamefile(&filelist, md5str)) { // 파일 리스트에 처음 추가되는 hash값을 가진 파일
			HInsert(&filelist, md5str, size);
			FInsert(&filelist, path, atime, mtime);
		}
	else // 파일 리스트에 동일 파일이 존재하는 경우
		NInsert(&filelist, path, atime, mtime);
}
/* index 정상 입력 여부 확인하는 함수 */
int checking_index(char *set_index)
{
	size_t len = strlen(set_index);
	int idx;
	/* '0'이거나 '0'으로 시작하는 index는 에러 처리 */
	if (set_index[0] == '0')
		return FALSE;
	/* index는 숫자여야함 */
	for (int i = 0; i < len; i++) {
		if (set_index[i] < '0' || set_index[i] > '9')
			return FALSE;
	}
	idx = atoi(set_index); // 숫자 변환
	filelist.hcur = filelist.head->down;
	/* index가 범위를 넘어서는지 확인 
	   index만큼 linked list의 head 아래부터 down 시킨다. */
	while (filelist.hcur != NULL) {
		idx--;
		if (idx == 0)
			break;
		filelist.hcur = filelist.hcur->down;
	}

	if (idx != 0) // 범위를 넘어서는 경우 FALSE return
		return FALSE;
	else
		return TRUE;
}
/* option 정상 입력 여부 확인하는 함수 */
int checking_option(char *op)
{
	if (op == NULL || strlen(op) != 1)
		return FALSE;
	
	if (op[0] != 'd' && op[0] != 'i' &&
		op[0] != 'f' && op[0] != 't' &&
		op[0] != 'e' && op[0] != 'p' &&
		op[0] != 'm')
		return FALSE;
	else
		return TRUE;
}
/* list_idx 정상 입력 여부 확인 */
int checking_list_idx(char *list_idx)
{
	if (list_idx == NULL) // 입력 없을 시 FALSE return
		return FALSE;

	size_t len = strlen(list_idx);
	int idx;
	/* '0' 또는 '0'으로 시작하면 FALSE, 숫자가 아니면 FALSE */
	if (list_idx[0] == '0')
		return FALSE;
	for (int i = 0; i < len; i++) {
		if (list_idx[i] < '0' || list_idx[i] > '9')
			return FALSE;
	}

	idx = atoi(list_idx);
	filelist.pcur = filelist.hcur->next;
	/* index checking할때 위치 시킨 hcur에 해당하는 파일 세트의 파일들에 대해 index만큼 next */
	while (filelist.pcur != NULL) {
		idx--;
		if (idx == 0)
			break;
		filelist.pcur = filelist.pcur->next;
	}

	if (idx != 0) // 범위를 넘어서면 FALSE return
		return FALSE;
	else
		return TRUE;
}
/* d 옵션 실행 */
int d_option(char *set_index, char *list_idx)
{
	if (!checking_list_idx(list_idx)) // list_idx 정상 입력X -> FALSE
		return FALSE;

	char *rmFile_path = NULL; // 삭제될 파일의 경로
	rmFile_path = Node_Remove(&filelist, filelist.pcur);
	(filelist.hcur->data.numOfData)--; // 파일 세트의 파일 개수 -1
	
	if (remove(rmFile_path) < 0) { // 파일 제거
		fprintf(stderr, "remove error for %s\n", rmFile_path);
		exit(1);
	}
	else {
		printf("\"%s\" has been deleted in #%s\n", rmFile_path, set_index);
		return TRUE;
	}
}
/* i 옵션 실행 */
void i_option(void)
{
	char input[BUFSIZE];
	char *rmFile_path = NULL; // 삭제 파일 경로
	filelist.pcur = filelist.hcur->next;
	Node *nextNode = NULL;
	
	/* index로 입력된 파일 세트의 파일들을 하나씩 삭제 여부 확인 */
	while (filelist.pcur != NULL) {
		printf("Delete \"%s\"? [y/n] ", filelist.pcur->data.path);
		fgets(input, BUFSIZE, stdin);
		input[strlen(input) - 1] = '\0';
		// 'y', 'Y', 'n', 'N' 의 정상 입력될 때까지 재입력 요구
		while (strcasecmp(input, "y") && strcasecmp(input, "n"))  {
			fprintf(stderr, "input error. input again.\n");
			printf(">> ");
			fgets(input, BUFSIZE, stdin);
			input[strlen(input) - 1] = '\0';
		} // 'y'라면 파일 제거
		if (!strcasecmp(input, "y")) {
			nextNode = filelist.pcur->next; // 다음 파일의 node 저장
			rmFile_path = Node_Remove(&filelist, filelist.pcur);
			if (remove(rmFile_path) < 0) {
				fprintf(stderr, "remove error\n");
				exit(1);
			}
			filelist.pcur = nextNode; // 다음 파일의 node에 pcur을 위치시킴
			(filelist.hcur->data.numOfData)--;
		}
		else {
			filelist.pcur = filelist.pcur->next;
		}
	}
}
/* f, t 옵션 실행 */
void f_t_option(char *set_index, char *op)
{
	Node *nwFile = filelist.hcur->next; // 가장 최근 수정된 파일 -> 남길 파일
	Node *nextNode = NULL;
	filelist.pcur = filelist.hcur->next;

	char *nwFile_path = NULL; // 남길 파일의 경로
	char *rmFile_path = NULL; // 제거될 파일의 경로
	char nwFile_time[20]; // 남길 파일 마지막 수정 시간
	struct tm timeinfo;

	char *homedir = getenv("HOME");
	char trashdir[PATH_MAX]; // 휴지통 경로
	/* 가장 최근 수정된 파일 탐색 -> nwFile을 위치 그 파일에 시킨다. */
	while (filelist.pcur->next != NULL) {
		if (nwFile->data.mtime < filelist.pcur->next->data.mtime)
			nwFile = filelist.pcur->next;
		filelist.pcur = filelist.pcur->next;
	}

	nwFile_path = nwFile->data.path; // 남길 파일 경로 가져온다.
	// st_mtime을 변형시킴
	strftime(nwFile_time, 20, "%Y-%m-%d %H:%M:%S", localtime_r(&nwFile->data.mtime, &timeinfo));
	/* 나머지 파일 제거 */
	filelist.pcur = filelist.hcur->next;
	while (filelist.pcur != NULL) {
		if (filelist.pcur == nwFile) { // 남길 파일은 제거 대상X
			filelist.pcur = filelist.pcur->next;
			continue;
		}
		nextNode = filelist.pcur->next;
		rmFile_path = Node_Remove(&filelist, filelist.pcur); // linked list에서 제거, 경로 return받음
		if (op[0] == 'f') { // f 옵션이라면
			if (remove(rmFile_path) < 0) { // 파일 제거
				fprintf(stderr, "remove error for %s\n", rmFile_path);
				exit(1);
			}
		}
		else if (op[0] == 't') { // t 옵션이라면
			sprintf(trashdir, "%s/%s/%s", homedir, TRASH_DIR, get_path_to_filename(rmFile_path));
			if (rename(rmFile_path, trashdir) < 0) { // 휴지통으로 이동
				fprintf(stderr, "rename error for %s\n", rmFile_path);
				exit(1);
			}
		}
		filelist.pcur = nextNode;
		(filelist.hcur->data.numOfData)--;
	}
	
	if (filelist.hcur->data.numOfData != 1) { // 남은 파일이 1개가 아니라면 에러 -> 프로그램 종료
		fprintf(stderr, "linked list error\n");
		exit(1);
	}
	else { // 옵션에 따른 알림 출력
		if (op[0] == 'f')
			printf("Left file in #%s : %s (%s)\n", set_index, nwFile_path, nwFile_time);
		else if (op[0] == 't')
			printf("All files in #%s have moved to Trash except \"%s\" (%s)\n", set_index, nwFile_path, nwFile_time);
	}
}
/* e 옵션 실행 */
void e_option(char *set_index)
{
	Node *nextNode = NULL;
	/* 파일은 제거하지 않고 중복 파일 리스트에서만 제거한다. */
	filelist.pcur = filelist.hcur->next;
	while (filelist.pcur != NULL) {
		nextNode = filelist.pcur->next;
		Node_Remove(&filelist, filelist.pcur);
		filelist.pcur = nextNode;
		(filelist.hcur->data.numOfData)--;
	}

	if (filelist.hcur->data.numOfData != 0) {
		fprintf(stderr, "linked list error\n");
		exit(1);
	}
	else
		printf("#%s is excepted\n", set_index);
}
/* p 옵션 실행 */
void p_option(char *set_index)
{
	char *fname = filelist.hcur->next->data.path;
	char buf[BUFSIZE];
	int fd, length;
	/* 파일의 내용을 출력한다. */
	if ((fd = open(fname, O_RDONLY)) < 0) {
		fprintf(stderr, "open error for %s\n", fname);
		exit(1);
	}

	while ((length = read(fd, buf, BUFSIZE)) > 0)
		write(1, buf, length);

	close(fd);
	printf("\n\n#%s file content is printed\n", set_index);
}
/* m 옵션 실행 */
int m_option(char *set_index, char *list_idx, char *pRename_path)
{
	char rename_path[PATH_MAX]; // 파일 이동 시킬 경로
	char origin_path[PATH_MAX];
	char file_name[FILE_MAX_LEN];

	if (!checking_list_idx(list_idx)) // list_idx 정상 입력X -> FALSE
		return FALSE;
	else if (pRename_path[0] == '~') // '~' -> home directory
		sprintf(rename_path, "%s%s", getenv("HOME"), pRename_path + 1);
	else if (realpath(pRename_path, rename_path) == NULL) // 정상 입력 -> 절대경로 변환
		return FALSE;

	strcpy(origin_path, filelist.pcur->data.path); // 이전 경로
	strcpy(file_name, get_path_to_filename(origin_path)); // 파일 이름만 추출

	if (strcmp(rename_path, "/")) // 이동 시킬 경로가 '/'가 아니라면 '/(파일이름)' combine
		strcat(rename_path, "/");
	strcat(rename_path, file_name);
	
	if (rename(origin_path, rename_path) < 0) { // 파일 이동
		fprintf(stderr, "rename error for %s\n", origin_path);
		exit(1);
	}
	else {
		strcpy(filelist.pcur->data.path, rename_path); // 해당 node slot의 path data 업데이트
		printf("\nmove %s to %s in #%s\n", origin_path, rename_path, set_index); // 완료 알림
		return TRUE;
	}
}
/* 중복 파일들의 사이즈를 출력해주는 함수 */
void size_option(void)
{
	double dup_size;

	filelist.hcur = filelist.head->down;

	while (filelist.hcur != NULL) {
		dup_size += (double)filelist.hcur->data.size * (double)(filelist.hcur->data.numOfData - 1);
		filelist.hcur = filelist.hcur->down;
	}
	
	if (dup_size < 1024.0)
		printf("duplication size : %.2fB\n", dup_size);
	else if (dup_size < 1024.0 * 1024.0)
		printf("duplication size : %.2fKB\n", dup_size / 1024.0);
	else if (dup_size < 1024.0 * 1024.0 * 1024.0)
		printf("duplication size : %.2fMB\n", dup_size / (1024.0 * 1024.0));
	else
		printf("duplication size : %.2fGB\n", dup_size / (1024.0 * 1024.0 * 1024.0));
	printf("\n");
}
/* 경로 -> 파일 이름 추출 */
char *get_path_to_filename(char *path)
{
	return strrchr(path, '/') + 1; // 가장 마지막 '/' 뒤의 포인터 반환
}
/* 파일 이름 -> 확장자 추출 */
char *get_extension(char *file_name)
{
	return strrchr(file_name, '.') + 1; // 가장 마지막 '.' 뒤의 포인터 반환
}