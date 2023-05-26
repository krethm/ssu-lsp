#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdbool.h>
#include <time.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h>
#include "linkedlist.h"
#include "dirQueue.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FILE_MAX_LEN 256
#define BUFFER_SIZE 1024 

// log, trash, trash_info 파일 or 디렉토리 이름
const char *path_log = ".duplicate_20181321.log";
const char *path_trash = ".Trash/files";
const char *path_trash_info = ".Trash/info";
// 사용자에 따라 지정될 log, trash, trash_info 파일 or 디렉토리 경로
char logdir[PATH_MAX];
char trashdir[PATH_MAX];
char trashdir_info[PATH_MAX];

typedef struct _trashinfo {
	char trash_name[FILE_MAX_LEN]; // trash에 저장된 파일의 이름 (abcd, abcd.2, abcd.3 ....)
	char filename[PATH_MAX]; // 파일의 삭제되기 전 경로
	char size[20]; // 파일의 사이즈
	char deletion_date[11]; // 파일의 제거 날짜
	char deletion_time[9]; // 파일의 제거 시간
} trashinfo; // trash에 이동된 파일의 info
trashinfo *trash_list = NULL; // trash 리스트 출력을 위해 파일 개수만큼 heap 메모리 할당
int file_count = 0; // trash 파일 개수

bool exitFlag = false; // 파일 탐색 결과 중복 파일이 없을 경우 프롬프트로 복귀하기 위해 사용
List filelist; // linked list
Queue dirlist; // queue (BFS)

// fsha1 명령어 인자들
char file_extension[FILE_MAX_LEN];
double min_size;
double max_size;
char target_directory[PATH_MAX];
int thread_num;

int status; // pthread_join 인자
pthread_t tid[4]; // 생성될 쓰레드 tid
pthread_t maintid; // 메인 쓰레드 tid
pthread_mutex_t add_lock; // add_to_filelist()에서 사용될 mutex

void create_pthread(void); // 쓰레드 생성
void make_userfile(void); // 사용자마다 log, trash 파일 생성

// command마다 인자 검사 -> 명령어 실행 함수 호출
void command_help(void);
void command_fsha1(int argc, char *argv[]);
void command_list(int argc, char *argv[]);
void command_trash(int argc, char *argv[]);
void command_restore(int argc, char *argv[]);

// fsha1 명령어 인자 유효성 검사
int checking_file_extension(char *extension);
int checking_size(char *str_size, double* size);
int checking_target_directory(char *target);
int checking_thread_num(char *thread);

void search_files(void); // command_fsha1 -> search_files -> bfs_directory
void *search(void *arg); // 쓰레드의 파일 탐색 시작점
void bfs_directory(void *path); // bfs
void add_to_filelist(char *path, struct stat *statbuf, int restore); // 파일의 sha1 hash값 추출 -> filelist 추가
void remove_dupfile(void); // 중복 파일 탐색 종료 후 delete 명령어 실행
void test_searching_time(const struct timeval *diff); // 쓰레드 개수별 탐색 시간 측정

int checking_set_idx(char *set_idx);
int checking_list_idx(char *list_idx);
int d_option(int set_idx, char *list_idx);
int i_option(void);
int f_t_option(int set_idx, char op);

void bring_trash_list(char *category, char *order); // trash 파일 리스트 불러옴
void sort_trash_list(char *category, char *order); // trash 명령어 인자에 따른 sort
void print_trash_list(void); // trash 리스트 출력
void move_to_trash(char *file_path); // 파일 trash로 이동
void remove_trash_log(char *trash_name); // trash info에 있는 파일 제거
void record_log(char *command, char *file_path); // log에 기록
void record_trashfile_info(char *file_path, char *rename_path); // trash info에 각 파일의 정보 기록

char *get_path_to_filename(char *path);
char *get_filename_to_extension(char *filename);

int file_precede(LData d1, LData d2) // 파일 리스트 정렬
{
	if (d1.depth < d2.depth)
		return 0;
	else if (d1.depth > d2.depth)
		return 1;
	else {
		int cmp = strcmp(d1.path, d2.path);
		if (cmp < 0)
			return 0;
		else
			return 1;
	}
}

int table_precede(HData d1, HData d2) // 파일 세트 정렬
{
	if (d1.size <= d2.size)
		return 0;
	else
		return 1;
}

int main(void)
{
	int argc;
	char *argv[BUFFER_SIZE];
	char buf[BUFFER_SIZE];

	maintid = pthread_self();
	ListInit(&filelist, table_precede, file_precede); // linked list 초기화
	QueueInit(&dirlist); // queue 초기화

	sprintf(logdir, "%s/%s", getenv("HOME"), path_log); // 사용자별로 log, trash 경로 지정
	sprintf(trashdir, "%s/%s", getenv("HOME"), path_trash);
	sprintf(trashdir_info, "%s/%s", getenv("HOME"), path_trash_info);
	make_userfile(); // 지정된 log, trash 파일, 디렉토리 생성
	
	while (1) { // 프롬프트 시작
		argc = 0;
		memset(argv, 0, BUFFER_SIZE);
		optind = 1;

		printf("20181321> ");
		fgets(buf, BUFFER_SIZE, stdin); // 명령어 입력
		if (buf[0] == '\n') continue;
		buf[strlen(buf) - 1] = '\0';

		argv[argc++] = strtok(buf, " "); // 각 인자들 split -> argv
		while ((argv[argc++] = strtok(NULL, " ")) != NULL);
		argc--;
		// 첫 인자 = 명령어의 종류 -> 각 명령어에 따른 함수 호출
		if (!strcmp(argv[0], "exit")) // exit -> 프롬프트 종료
			break;
		else if (!strcmp(argv[0], "fsha1")) {
			command_fsha1(argc, argv);
			continue;
		}
		else if (!strcmp(argv[0], "list")) {
			command_list(argc, argv);
			continue;
		}
		else if (!strcmp(argv[0], "trash")) {
			command_trash(argc, argv);
			continue;
		}
		else if (!strcmp(argv[0], "restore")) {
			command_restore(argc, argv);
			continue;
		}
		else {
			command_help();
			continue;
		}
	}
	// 프로그램 종료 전 routine
	if (trash_list != NULL)
		free(trash_list);

	LRemoveall(&filelist);
	clean_queue(&dirlist);
	puts("Prompt End");
	exit(0);
}
// fsha1
void command_fsha1(int argc, char *argv[])
{
	bool e_op, l_op, h_op, d_op; // 각 인자 입력 여부
	int opt;

	e_op = l_op = h_op = d_op = false;
	exitFlag = false; // 중복 파일이 없을 시 exitFlag는 true가 되고 프롬프트로 복귀
	thread_num = 1; // 기본 쓰레드 개수는 1개
	thread_counter = 0; // 쓰레드 bfs 탐색시 사용될 변수
	// getopt
	while ((opt = getopt(argc, argv, "e:l:h:d:t:")) != -1) {
		switch (opt) {
			case 'e' :
				if (!checking_file_extension(optarg)) { // extension 인자 검사
					fprintf(stderr, "file_extension error\n");
					return;
				}
				e_op = true;
				break;
			case 'l' :
				if (!checking_size(optarg, &min_size)) { // minsize 인자 검사
					fprintf(stderr, "minsize error\n");
					return;
				}
				l_op = true;
				break;
			case 'h' :
				if (!checking_size(optarg, &max_size)) { // maxsize 인자 검사
					fprintf(stderr, "maxsize error\n");
					return;
				}
				h_op = true;
				break;
			case 'd' :
				if (!checking_target_directory(optarg)) { // directory 인자 검사
					fprintf(stderr, "target_directory error\n");
					return;
				}
				d_op = true;
				break;
			case 't' :
				if (!checking_thread_num(optarg)) { // thread 개수 인자 검사
					fprintf(stderr, "thread_num error\n");
					return;
				}
				break;
			case '?' :
				return;
		}
	}
	if (!e_op | !l_op | !h_op | !d_op) { // t 옵션을 제외한 모든 옵션은 필수 입력
		fprintf(stderr, "input error\n");
		return;
	}

	if (filelist.head->down != NULL) { // 파일 탐색 전에 이미 중복 파일 리스트가 존재할 시 제거
		LRemoveall(&filelist);
		ListInit(&filelist, table_precede, file_precede);
	}

	search_files(); // 파일 탐색

	if (exitFlag) // 중복 파일 X -> 프롬프트 복귀
		return;
	remove_dupfile(); // delete 명령어
}
// 파일 탐색 종료되고 실행되는 핸들러
void usr1_handler(int signo) {
	pthread_t current_thread = pthread_self();

	for (int i = 0; i < thread_num - 1; i++) { // 쓰레드 cancel
		if (current_thread != tid[i])
			pthread_cancel(tid[i]);
	}
	pthread_mutex_destroy(&lock);
	pthread_mutex_destroy(&add_lock);
	pthread_cond_destroy(&not_empty);
	
	if (pthread_self() != maintid) // main쓰레드는 exit하지 않음
		pthread_exit(NULL); // 자신 쓰레드 종료
}
// 쓰레드 생성
void create_pthread(void) {
	struct sigaction sigact;

	sigemptyset(&sigact.sa_mask);
	sigact.sa_handler = usr1_handler;
	sigact.sa_flags = 0;
	
	if (sigaction(SIGUSR1, &sigact, NULL) != 0) {
		fprintf(stderr, "sigaction() error\n");
		exit(1);
	}; // SIGUSR1 <- usr1_handler 핸들러 지정

	Enqueue(&dirlist, target_directory); // 탐색 시작 디렉토리 enqueue

	pthread_mutex_init(&lock, NULL); // mutex, cond init
	pthread_mutex_init(&add_lock, NULL);
	pthread_cond_init(&not_empty, NULL);

	if (thread_num == 1) // 쓰레드 개수가 1개 -> 메인 쓰레드만 탐색
		search(NULL);
	else {
		for (int i = 0; i < thread_num - 1; i++) { // 쓰레드 생성 -> search() -> bfs_directory()
			if (pthread_create(&tid[i], NULL, search, NULL) != 0) {
				fprintf(stderr, "pthread_create error\n");
				exit(1);
			}
		}
		for (int i = 0; i < thread_num - 1; i++) // 메인 쓰레드는 대기
			pthread_join(tid[i], NULL);
	}
}
// 파일 탐색
void search_files(void)
{
	struct timeval start, end, diff;
	
	gettimeofday(&start, NULL); // 탐색 시작
	create_pthread();
	gettimeofday(&end, NULL); // 탐색 종료

	diff.tv_sec = end.tv_sec - start.tv_sec;
	diff.tv_usec = end.tv_usec - start.tv_usec;
	if (diff.tv_usec < 0) {
		diff.tv_sec--;
		diff.tv_usec += 1000000;
	}

	Remove_Nodup(&filelist); // 중복이 아닌 파일은 중복 파일 리스트에서 제거
	if (!ListPrint(&filelist)) { // 중복 파일 리스트 출력, 비어 있을 경우 프롬프트로 복귀
		printf("No duplicates in %s\n", target_directory);
		exitFlag = true;
	}
	printf("Searching time: %ld:%06ld(sec:usec)\n\n", diff.tv_sec, diff.tv_usec); // 탐색 시간 출력
	
	clean_queue(&dirlist);
	QueueInit(&dirlist);
	
	test_searching_time(&diff); // 쓰레드 개수에 따른 파일 탐색 시간 측정, 사용하지 않을 시 해당코드만 주석처리해도 됨.
}
// 파일 탐색에 사용되는 쓰레드 개수에 따른 파일 탐색 시간 측정
void test_searching_time(const struct timeval *diff)
{
	struct timeval start, end;
	struct timeval diff_test;
	int tn = thread_num;

	printf("testing searching time by number of threads...\n");
	thread_num = 1;
	while (thread_num <= 5) {
		if (tn == thread_num) { // 전에 측정한 탐색 시간은 그대로 출력
			printf("%d threads : %ld:%06ld(sec:usec)\n", tn, diff->tv_sec, diff->tv_usec);
			thread_num++;
			continue;
		}

		LRemoveall(&filelist);
		ListInit(&filelist, table_precede, file_precede);

		gettimeofday(&start, NULL);
		create_pthread();
		gettimeofday(&end, NULL);

		diff_test.tv_sec = end.tv_sec - start.tv_sec;
		diff_test.tv_usec = end.tv_usec - start.tv_usec;
		if (diff_test.tv_usec < 0) {
			diff_test.tv_sec--;
			diff_test.tv_usec += 1000000;
		}

		printf("%d threads : %ld:%06ld(sec:usec)\n", thread_num, diff_test.tv_sec, diff_test.tv_usec);

		clean_queue(&dirlist);
		QueueInit(&dirlist);
		thread_num++;
	}

	printf("\n");
	thread_num = tn;
	Remove_Nodup(&filelist);
}
// 쓰레드가 cancel될때 실행되는 routine
void cancel_handler(void *arg) {
	pthread_mutex_t *lock = (pthread_mutex_t*)arg;
	pthread_mutex_unlock(lock);
}
// 쓰레드의 파일 탐색 시작점
void *search(void *arg)
{
	pthread_cleanup_push(cancel_handler, &lock); // push routine
	// 쓰레드의 데이터 dequeue : 데이터가 enqueue되고 cond signal을 받고 나서만 실행됨
	bfs_directory(Dequeue(&dirlist)); // 디렉토리 bfs 탐색 함수 호출
	pthread_cleanup_pop(1); // 정상 종료된 경우에도 cancel_handler 실행
}
// 디렉토리 bfs 탐색
void bfs_directory(void *arg)
{
	struct stat statbuf;
	DIR *dirp = NULL;
	struct dirent *dentry = NULL;
	char *tmp = (char*)arg; // dequeue한 디렉토리 형변환
	char path[PATH_MAX];
	
	if ((dirp = opendir(tmp)) == NULL) {
		fprintf(stderr, "open error for %s\n", tmp);
		exit(1);
	}

	while ((dentry = readdir(dirp)) != NULL) {
		if (!strcmp(dentry->d_name, ".") || !strcmp(dentry->d_name, ".."))
			continue;
		
		strcpy(path, tmp);
		if (!strcmp(path, "/"))
			strcat(path, dentry->d_name);
		else {
			strcat(path, "/");
			strcat(path, dentry->d_name);
		} // path : 탐색 대상 파일의 경로

		if (!strcmp(path, "/proc") || !strcmp(path, "/run") || !strcmp(path, "/sys"))
			continue;

		if (lstat(path, &statbuf) < 0) {
			fprintf(stderr, "lstat error for %s\n", path);
			exit(1);
		}

		if (S_ISDIR(statbuf.st_mode)) { // 디렉토리는 enqueue
			Enqueue(&dirlist, path);
			continue;
		}
		if (!S_ISREG(statbuf.st_mode)) // 정규 파일만 탐색 대상
			continue;

		if (strcmp(file_extension, "*") != 0) { // 특정 확장자를 대상으로 탐색하는 경우
			if (strchr(dentry->d_name, '.') == NULL) // 파일의 확장자가 없는 경우 제외
				continue;

			if (strcmp(file_extension, get_filename_to_extension(dentry->d_name)) != 0)
				continue; // 확장자가 다른 경우 제외
		}
		if (statbuf.st_size == 0) // size 0인 파일 제외
			continue;
		
		if (max_size == -1.0) { // -1 == '~'
			if ((double)statbuf.st_size < min_size)
				continue; // min_size보다 작으면 제외
		}
		else { // minsize <= x <= maxsize만 탐색
			if (((double)statbuf.st_size < min_size) ||	((double)statbuf.st_size > max_size))
				continue;
		} // 모든 조건 만족 -> 중복 파일 리스트에 추가
		add_to_filelist(path, &statbuf, 0);
	}
	closedir(dirp);
	pthread_testcancel(); // 쓰레드 cancle 지점 설정

	pthread_mutex_lock(&lock);
	thread_counter--; // thread_counter는 dequeue할때마다 +1, 디렉토리 탐색 한번 할때마다 -1
	pthread_mutex_unlock(&lock);
	// 탐색을 모두 마친 경우 thread_counter는 0이 됨.
	if (QIsEmpty(&dirlist) && !thread_counter)
		raise(SIGUSR1); // SIGSUR1 시그널 발생 -> 모든 THREAD는 cancel
	else
		bfs_directory(Dequeue(&dirlist)); // 쓰레드의 데이터 dequeue : 데이터가 enqueue되고 cond signal을 받고 나서만 실행됨
}
// 중복 파일 리스트에 추가, restore 파라미터는 restore된 파일인지 여부
void add_to_filelist(char *path, struct stat *statbuf, int restore)
{
	pthread_mutex_lock(&add_lock); // mutex lock
	int fd;
	int length;
	SHA_CTX c;
	unsigned char buf[BUFFER_SIZE];
	unsigned char sha[SHA_DIGEST_LENGTH];
	static char shastr[41];

	SHA1_Init(&c);

	if ((fd = open(path, O_RDONLY)) < 0) {
		fprintf(stderr, "open error for %s\n", path);
		exit(1);
	}
	// 파일의 sha1 해시 값 추출
	while ((length = read(fd, buf, BUFFER_SIZE)) > 0)
		SHA1_Update(&c, buf, (unsigned long)length);
	SHA1_Final(&(sha[0]), &c);

	close(fd);
	// 16진수 형태의 문자열로 변형
	for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(&shastr[i*2], "%02x", (unsigned int)sha[i]);
	// 중복 파일 리스트에 추가
	if (!FindSamefile(&filelist, shastr)) { // 해당 파일 세트가 존재하지 않는 경우
		if (!restore) { // 복원된 파일인 경우 해당 파일 세트가 없으면 추가하지 않음
			HInsert(&filelist, shastr, statbuf->st_size); // 파일 세트 생성
			FInsert(&filelist, path, statbuf); // 해당 파일 세트에 파일 리스트로 추가
		}
	}
	else // 해당 파일 세트가 존재
		NInsert(&filelist, path, statbuf); // 해당 파일 세트에 추가
	pthread_mutex_unlock(&add_lock); // mutex unlock
}
// delete 명령어
void remove_dupfile(void)
{
	char buf[BUFFER_SIZE];
	int argc;
	char *argv[BUFFER_SIZE];
	int opt;
	int set_idx;
	char *list_idx;
	bool breakflag; // 입력 에러 시 getopt 함수 종료하고 다시 입력
	bool l_op; // l 옵션 입력 여부
	bool exec, d_op, i_op, f_op, t_op; // 4가지중 하나가 입력되면 exec은 true (중복 방지)

	while (1) {
		argc = 0;
		memset(argv, 0, BUFFER_SIZE);
		optind = 1;
		breakflag = false;
		l_op = false;
		exec = d_op = i_op = f_op = d_op = false;

		printf(">> ");
		fgets(buf, BUFFER_SIZE, stdin); // 명령어 입력

		if (buf[0] == '\n')
			continue;
		buf[strlen(buf) - 1] = '\0';

		argv[argc++] = strtok(buf, " "); // 각 인자 split -> argv
		while ((argv[argc++] = strtok(NULL, " ")) != NULL);
		argc--;

		if (!strcmp(argv[0], "exit")) // exit -> 프롬프트 복귀
			break;
		else if (strcmp(argv[0], "delete")) { // delete 명령어만 가능
			fprintf(stderr, "input error\n");
			continue;
		}
		// getopt
		while ((opt = getopt(argc, argv, "l:d:ift")) != -1) {
			switch (opt) {
				case 'l':
					if (!checking_set_idx(optarg)) { // set_idx 검사
						fprintf(stderr, "set_idx error\n");
						breakflag = true; // 에러 시 getopt함수 종료 -> 다시 입력
					}
					else {
						l_op = true;
						set_idx = atoi(optarg);
					}
					break;
				// 각 옵션은 하나만 입력 가능. 중복은 에러
				case 'd':
					if (!l_op | exec) { // l 옵션이 먼저 입력 되어야함 (옵션 수행될 파일 세트가 먼저 결정되어야함)
						fprintf(stderr, "input error\n");
						breakflag = true;
					}
					else {
						exec = d_op = true;
						list_idx = optarg;
					}
					break;
				case 'i':
					if (!l_op | exec) {
						fprintf(stderr, "input error\n");
						breakflag = true;
					}
					else
						exec = i_op = true;
					break;
				case 'f':
					if (!l_op | exec) {
						fprintf(stderr, "input error\n");
						breakflag = true;
					}
					else
						exec = f_op = true;
					break;
				case 't':
					if (!l_op | exec) {
						fprintf(stderr, "input error\n");
						breakflag = true;
					}
					else
						exec = t_op = true;
					break;
				case '?':
					breakflag = true;
					break;
			}
			if (breakflag)
				break;
		}
		if (breakflag) continue; // true -> 다시 입력
		
		if (!exec) { // d, i, f, t 중 한 개도 입력이 없다면 에러
			fprintf(stderr, "input error\n");
			continue;
		}
		
		if (d_op) { // d 옵션 실행
			if (!d_option(set_idx, list_idx)) {
				fprintf(stderr, "d_option error\n");
				continue;
			}
		}
		else if (i_op) { // i 옵션 실행
			if (!i_option()) {
				fprintf(stderr, "i_option error\n");
				continue;
			}
		}
		else if (f_op) { // f 옵션 실행
			if (!f_t_option(set_idx, 'f')) {
				fprintf(stderr, "f_option error\n");
				continue;
			}
		}
		else if (t_op) { // t 옵션 실행
			if (!f_t_option(set_idx, 't')) {
				fprintf(stderr, "t_option error\n");
				continue;
			}
		}

		if (!ListPrint(&filelist)) // 중복 파일 리스트가 비었다면 프롬프트로 복귀
			break;
	}
}
// list
void command_list(int argc, char *argv[])
{
	if (filelist.head->down == NULL) { // 중복 파일 리스트가 없을 시 프롬프트 복귀
		fprintf(stderr, "file list is empty\n");
		return;
	}

	int opt;
	char list_type[9] = "fileset"; // 기본 값
	char category[9] = "size";
	char order[3] = "1";
	// getopt
	while ((opt = getopt(argc, argv, "l:c:o:")) != -1) {
		switch (opt) {
			case 'l': // "fileset" or "filelist"
				if (strcmp(optarg, "fileset") && strcmp(optarg, "filelist")) {
					fprintf(stderr, "-l option error\n");
					return; // 에러 시 프롬프트 리턴
				}
				strcpy(list_type, optarg);
				break;
			case 'c': // 정렬 기준
				if (strcmp(optarg, "filename") && strcmp(optarg, "size") &&
					strcmp(optarg, "uid") && strcmp(optarg, "gid") && strcmp(optarg, "mode")) {
					fprintf(stderr, "-c option error\n");
					return; // 에러 시 프롬프트 리턴
				}
				strcpy(category, optarg);
				break;
			case 'o': // 1 : 오름차순, -1 : 내림차순
				if (strcmp(optarg, "1") && strcmp(optarg, "-1")) {
					fprintf(stderr, "-o option error\n");
					return; // 에러 시 프롬프트 리턴
				}
				strcpy(order, optarg);
				break;
			case '?':
				return; // 에러 시 프롬프트 리턴
		}
	}
	
	ListSort(&filelist, list_type, category, order); // 중복 파일 리스트 정렬 함수(linkedlist.c)
	ListPrint(&filelist); // 리스트 출력 -> 프롬프트 복귀
}
// trash
void command_trash(int argc, char *argv[])
{
	int opt;
	char category[9] = "filename"; // 기본 값
	char order[3] = "1";
	// getopt
	while ((opt = getopt(argc, argv, "c:o:")) != -1) {
		switch (opt) {
			case 'c': // 정렬 기준
				if (strcmp(optarg, "filename") && strcmp(optarg, "size") &&
					strcmp(optarg, "date") && strcmp(optarg, "time")) {
					fprintf(stderr, "-c option error\n");
					return; // 에러 시 프롬프트 복귀
				}
				strcpy(category, optarg);
				break;
			case 'o': // 1: 오름차순, -1: 내림차순
				if (strcmp(optarg, "1") && strcmp(optarg, "-1")) {
					fprintf(stderr, "-o option error\n");
					return ; // 에러 시 프롬프트 복귀
				}
				strcpy(order, optarg);
				break;
			case '?':
				return; // 에러 시 프롬프트 복귀
		}
	}
	bring_trash_list(category, order);
}
// trash list 가져오는 함수
void bring_trash_list(char *category, char *order)
{
	int idx = 0;
	int fd;
	DIR *dirp;
	struct dirent *dentry;
	char current_dir[PATH_MAX];
	char *buf;
	int length;
	char *ptr;
	file_count = 0;

	getcwd(current_dir, PATH_MAX); // 현재 작업 디렉토리 저장
	chdir(trashdir_info); // ~/.Trash/info 작업 디렉토리 이동

	if ((dirp = opendir(trashdir_info)) == NULL) {
		fprintf(stderr, "open error for %s\n", trashdir_info);
		exit(1);
	}
	while ((dentry = readdir(dirp)) != NULL) {
		if (dentry->d_type == DT_REG) // trash에 제거된 파일 개수 세기
			file_count++;
	}
	closedir(dirp);

	if (file_count == 0) { // 개수가 0인 경우 
		printf("Trash bin is empty\n");
		chdir(current_dir); // 원래 작업 디렉토리로 재 설정하고 프롬프트로 이동
		return;
	}

	if (trash_list != NULL) { // 이미 trash list가 존재하는 경우 제거
		free(trash_list);
		trash_list = NULL;
	}
	trash_list = (trashinfo*)malloc(sizeof(trashinfo) * file_count); // 파일 개수만큼 malloc

	dirp = opendir(trashdir_info); // 각 trashinfo 파일을 하나씩 오픈 -> trash_list에 정보 저장
	while ((dentry = readdir(dirp)) != NULL) {
		if (dentry->d_type != DT_REG)
			continue;

		if ((fd = open(dentry->d_name, O_RDONLY)) < 0) { // trashinfo 파일 오픈
			fprintf(stderr, "open error for %s/%s\n", trashdir_info, dentry->d_name);
			exit(1);
		}

		length = lseek(fd, 0L, SEEK_END); // 내용 길이만큼 buf 메모리 할당
		lseek(fd, 0L, SEEK_SET);

		buf = (char*)malloc(sizeof(char) * length);
		read(fd, buf, length); // 파일 내용 buf에 저장
		
		ptr = get_filename_to_extension(dentry->d_name);
		*(--ptr) = '\0'; // ex) asdf.png.tashinfo의 trash에 제거된 파일 이름은 asdf.png

		strcpy(trash_list[idx].trash_name, dentry->d_name); // trash에 제거되어 있는 파일의 이름
		strcpy(trash_list[idx].filename, strtok(buf, "\n")); // 제거되기 전 파일의 경로
		strcpy(trash_list[idx].size, strtok(NULL, "\n")); // 파일의 사이즈
		strcpy(trash_list[idx].deletion_date, strtok(NULL, "T")); // 제거된 날짜
		strcpy(trash_list[idx].deletion_time, strtok(NULL, "\n")); // 제거된 시간

		free(buf);
		close(fd);
		idx++;
	}
	chdir(current_dir); // 작업 디렉토리 원래 위치로 이동
	sort_trash_list(category, order); // trash list 정렬 함수 호출
}
// trash list sort
void sort_trash_list(char *category, char *order)
{
	int i, j;
	int change;
	trashinfo tmp;

	if (!strcmp(order, "1")) { // 오름차순
		for (i = 0; i < file_count - 1; i++) {
			change = i;
			for (j = i + 1; j < file_count; j++) {
				if (!strcmp(category, "filename")) { // 제거되기 전 파일의 경로 기준
					if (strcmp(trash_list[change].filename, trash_list[j].filename) > 0)
						change = j;
				}
				else if (!strcmp(category, "size")) { // 사이즈 기준
					if (atoi(trash_list[change].size) > atoi(trash_list[j].size))
						change = j;
				}
				else if (!strcmp(category, "date")) { // 제거된 날짜 기준
					if (strcmp(trash_list[change].deletion_date, trash_list[j].deletion_date) > 0)
						change = j;
				}
				else if (!strcmp(category, "time")) { // 제거된 시간 기준
					if (strcmp(trash_list[change].deletion_time, trash_list[j].deletion_time) > 0)
						change = j;
				}
			}
			tmp = trash_list[i]; // trash list 배열 요소 위치 변경
			trash_list[i] = trash_list[change];
			trash_list[change] = tmp;
		}
	}
	else if (!strcmp(order, "-1")) { // 내림차순
		for (i = 0; i < file_count - 1; i++) {
			change = i;
			for (j = i + 1; j < file_count; j++) {
				if (!strcmp(category, "filename")) { // 제거되기 전 파일의 경로 기준
					if (strcmp(trash_list[change].filename, trash_list[j].filename) < 0)
						change = j;
				}
				else if (!strcmp(category, "size")) { // 사이즈 기준
					if (atoi(trash_list[change].size) < atoi(trash_list[j].size))
						change = j;
				}
				else if (!strcmp(category, "date")) { // 제거된 날짜 기준
					if (strcmp(trash_list[change].deletion_date, trash_list[j].deletion_date) < 0)
						change = j;
				}
				else if (!strcmp(category, "time")) { // 제거된 시간 기준
					if (strcmp(trash_list[change].deletion_time, trash_list[j].deletion_time) < 0)
						change = j;
				}
			}
			tmp = trash_list[i]; // trash list 배열 요소 위치 변경
			trash_list[i] = trash_list[change];
			trash_list[change] = tmp;
		}
	}
	print_trash_list(); // trash list 출력
}
// trash list 출력
void print_trash_list(void)
{
	if (file_count == 0) // 비어있을 경우
		printf("Trash bin is empty\n");
	else {
		printf("     FILENAME  \t \t \t \t \tSIZE \t DELETION DATA \t DELETION TIME\n");
		for (int i = 0; i < file_count; i++) {
			printf("[ %d] %-40s", i + 1, trash_list[i].filename);
			printf("\t%-3s \t%-10s \t%-10s\n", trash_list[i].size,
					trash_list[i].deletion_date, trash_list[i].deletion_time);
		}
	}
}
// restore
void command_restore(int argc, char *argv[])
{
	if (argc != 2) { // 입력된 인자는 2개인 경우만 허용
		fprintf(stderr, "usage : resotre [RESTORE_INDEX]\n");
		return; // 프롬프트 복귀
	}

	size_t len = strlen(argv[1]);
	int restore_idx; // argv[1] - 1
	char trashfile_dir[PATH_MAX];
	char *filename = NULL;
	char *ptr = NULL;
	struct stat statbuf;

	for (int i = 0; i < len; i++) { // restore_index가 숫자인지 검사
		if (!isdigit(argv[1][i])) {
			fprintf(stderr, "RESTORE_INDEX error\n");
			return;
		}
	}

	restore_idx = atoi(argv[1]) - 1;
	if (restore_idx < 0 || restore_idx >= file_count) { // restore_idx가 유효한 범위가 아닐 시 에러
		fprintf(stderr, "RESTORE_INDEX error\n");
		return;
	}
	
	filename = get_path_to_filename(trash_list[restore_idx].filename); // 제거되기 전 파일의 이름만 추출
	// trash에 제거된 파일의 경로 = "~/.Trash/files" + "/" + "파일 이름"
	strcpy(trashfile_dir, trashdir);
	ptr = trashfile_dir + strlen(trashfile_dir);
	*ptr++ = '/';
	strcpy(ptr, trash_list[restore_idx].trash_name);

	// a. 해당 파일이 trash에 존재하지 않고 ~/.Trash/info에 기록만 남아있는 경우
	if (access(trashfile_dir, F_OK) < 0)
		fprintf(stderr, "%s is not in trash\n", trash_list[restore_idx].filename);
	else { // b. 해당 파일이 존재
		// b.1 일반적인 상황의 파일 복구
		if (access(trash_list[restore_idx].filename, F_OK) < 0) {
			if (rename(trashfile_dir, trash_list[restore_idx].filename) < 0) {
				fprintf(stderr, "restore error for %s\n", trashfile_dir);
				exit(1);
			}
			if (stat(trash_list[restore_idx].filename, &statbuf) < 0) {
				fprintf(stderr, "stat error for %s\n", trash_list[restore_idx].filename);
				exit(1);
			} // 중복 파일 리스트에 다시 추가 (해당 파일 세트가 존재하는 경우만)
			add_to_filelist(trash_list[restore_idx].filename, &statbuf, 1);
		}
		else { // b.2 동일한 path를 가진 파일이 이미 존재
			// 파일 이름 뒤에 인덱스를 붙힌다. 1. abcd_1, abcd_2, ... 
	        //                           2. abcd.1.PNG, abcd.2.PNG, ...
			int idx = 1;
			char idx_char[20];
			char rename_path[PATH_MAX];
			char *extension = get_filename_to_extension(filename);

			while (1) { // 인덱스를 하나씩 증가시킴
				idx++;
				sprintf(idx_char, "%d", idx);
				strcpy(rename_path, trash_list[restore_idx].filename);

				if (extension == NULL) { // 파일의 확장자가 없는 경우
					ptr = rename_path + strlen(rename_path);
					*ptr++ = '_'; // 파일 이름 뒤에 "_idx"
					strcpy(ptr, idx_char);
				}
				else { // 파일의 확장자가 존재
					ptr = get_filename_to_extension(rename_path);
					strcpy(ptr, idx_char);
					ptr = ptr + strlen(idx_char);
					*ptr++ = '.'; // 파일 이름과 확장자 사이에 ".idx"
					strcpy(ptr, extension);
				}
	
				if (access(rename_path, F_OK) < 0) { // 이미 동일 path의 파일 존재 여부 확인
					if (rename(trashfile_dir, rename_path) < 0) {
						fprintf(stderr, "restore error for %s\n", trashfile_dir);
						exit(1);
					}
					break;
				}
				// 동일 파일 존재할 경우 idx++, 같은 과정 반복
			}

			if (stat(rename_path, &statbuf) < 0) {
				fprintf(stderr, "stat error for %s\n", rename_path);
				exit(1);
			} // 중복 파일 리스트에 다시 추가 (해당 파일 세트가 존재하는 경우만)
			add_to_filelist(rename_path, &statbuf, 1);
		}
		
		printf("[RESTORE] success for %s\n", trash_list[restore_idx].filename);
		record_log("RESTORE", trash_list[restore_idx].filename); // log에 기록
	}

	remove_trash_log(trash_list[restore_idx].trash_name); // 복원한 파일의 정보가 기록된 파일 삭제
	// trash list update
	for (int i = restore_idx + 1; i < file_count; i++)
		trash_list[i-1] = trash_list[i];
	
	file_count--;
	// trash list 출력
	print_trash_list();
}
// 복원한 파일의 정보가 기록된 파일 삭제
void remove_trash_log(char *trash_name)
{
	char current_dir[PATH_MAX];
	char infofile[FILE_MAX_LEN];
	// 작업 디렉토리 "~/.Trash/info"로 이동
	getcwd(current_dir, PATH_MAX);
	chdir(trashdir_info);
	// 해당 파일 삭제
	sprintf(infofile, "%s.%s", trash_name, "trashinfo");

	if (remove(infofile) < 0) {
		fprintf(stderr, "remove error for %s/%s\n", trashdir_info, infofile);
		exit(1);
	}

	chdir(current_dir); // 작업 디렉토리 리턴
}

// fsha1 명령어의 file extension 인자 유효성 검사
int checking_file_extension(char *extension)
{
	if (extension[0] != '*')
		return false; // '*'으로 시작하지 않으면 에러
	else if (extension[1] != '\0' && extension[1] != '.')
		return false; // '*'이거나 '*.(확장자)'만 가능

	if (extension[1] == '.') { // '*.'으로 시작하면 확장자는 반드시 지정해주어야함
		if (extension[2] == '\0')
			return false;
		else if (strchr(&extension[2], '.') != NULL)
			return false;
	}
	// file_extension 전역 변수에 저장
	if (!strcmp(extension, "*"))
		strcpy(file_extension, extension);
	else
		strcpy(file_extension, extension + 2);
	
	return true;
}
// fsha1 명령어의 size 인자 유효성 검사
int checking_size(char *str_size, double *size)
{
	char *ptr = NULL;
	char conv_size[BUFFER_SIZE];
	int i;

	if ((strcmp(str_size, "~") != 0) && !isdigit(str_size[0]))
		return false; // "~" 이거나 숫자로 시작해야함 (음수 당연히 에러)
	
	if (strcmp(str_size, "~") == 0) // "~"이라면 전역 변수에 -1.0으로 저장
		*size = -1.0;
	else { // 숫자로 시작 -> 단위 검사
		for (i = 0; str_size[i] != '\0'; i++)
			if (!isdigit(str_size[i]) && (str_size[i] != '.')) {
				ptr = &str_size[i];
				break;
			} // ptr은 '.'을 제외한 첫 문자를 가리킴 (단위)

		if (str_size[i] == '\0') { // 단위가 없는 경우
			sprintf(conv_size, "%.0lf", atof(str_size));
			*size = atof(conv_size);
		}
		else if (strcasecmp(ptr, "B") == 0) { // B
			sprintf(conv_size, "%.0lf", atof(str_size));
			*size = atof(conv_size);
		}
		else if (strcasecmp(ptr, "KB") == 0) { // KB
			sprintf(conv_size, "%.3lf", atof(str_size) * 1024.0);
			*size = atof(conv_size);
		}
		else if (strcasecmp(ptr, "MB") == 0) { // MB
			sprintf(conv_size, "%.6lf", atof(str_size) * 1024.0 * 1024.0);
			*size = atof(conv_size) ;
		}
		else if (strcasecmp(ptr, "GB") == 0) { // GB
			sprintf(conv_size, "%.9lf", atof(str_size) * 1024.0 * 1024.0 * 1024.0);
			*size = atof(conv_size);
		}
		else
			return false;
	}
	return true;
}
// fsha1 명령어의 target directory 인자 유효성 검사
int checking_target_directory(char *target)
{
	char *home_path = getenv("HOME");
	struct stat statbuf;

	if (target[0] == '~') // '~'으로 시작
		sprintf(target_directory, "%s%s", home_path, target + 1);
	else if (realpath(target, target_directory) == NULL)
		return false; // '~'으로 시작x, 절대 경로 변환 -> 해당 디렉토리가 존재x -> 에러

	if (stat(target_directory, &statbuf) < 0)
		return false; // '~'으로 시작하는 경우도 해당 디렉토리가 존재하지 않으면 stat함수 에러 발생
	if (!S_ISDIR(statbuf.st_mode)) // 디렉토리가 아닌 경우 에러
		return false;
	
	return true;
}
// fsha1 명령어의 thread 개수 인자 유효성 검사
int checking_thread_num(char *thread)
{
	size_t len = strlen(thread);

	for (int i = 0; i < len; i++) {
		if (!isdigit(thread[i]))
		return false;
	} // 모두 숫자여야함

	if (atoi(thread) > 5 || atoi(thread) < 1)
		return false; // 최대 개수는 5개

	thread_num = atoi(thread); // 전역 변수에 저장
	
	return true;
}
// delete 명령어의 set_idx 인자 유효성 검사
int checking_set_idx(char *set_idx)
{
	size_t len = strlen(set_idx);
	int idx;
	/* '0'이거나 '0'으로 시작하는 index 에러 */
	if (set_idx[0] == '0')
		return false;
	/* index는 숫자여야함 */
	for (int i = 0; i < len; i++) {
		if (!isdigit(set_idx[i]))
			return false;
	}

	idx = atoi(set_idx); // 숫자 변환
	filelist.hcur = filelist.head->down; // hcur은 첫 파일 세트에 위치
	/* index가 범위를 넘어서는 지 확인
	   index만큼 hcur은 차례대로 다음 파일 세트를 가리킴 */
	while (filelist.hcur != NULL) {
		if (filelist.hcur->data.numOfData >= 2) { // 파일이 2개 이하인 파일 세트는 제외
			idx--;
			if (idx == 0)
				break;
		}
		filelist.hcur = filelist.hcur->down;
	}

	if (idx != 0) // 범위를 넘어서는 경우 에러
		return false;
	else // hcur은 set_idx 인자 유효성 검사와 동시에 해당 파일 세트를 가리킴
		return true;
}
// delete 명령어의 list_idx 인자 유효성 검사
int checking_list_idx(char *list_idx)
{
	size_t len = strlen(list_idx);
	int idx;
	/* '0'이거나 '0'으로 시작하는 경우 에러 */
	if (list_idx[0] == '0')
		return false;
	/* 모두 숫자여야함 */
	for (int i = 0; i < len; i++) {
		if (!isdigit(list_idx[i]))
			return false;
	}

	idx = atoi(list_idx); // 숫자 변환
	// pcur은 set_idx 인자 검사 함수에서 위치된 파일 세트의 첫 파일 리스트를 가리킴
	filelist.pcur = filelist.hcur->next;
	/* index가 범위를 넘어서는 지 확인
	   index만큼 pcur은 차례대로 다음 파일을 가리킨다. */
	while (filelist.pcur != NULL) {
		idx--;
		if (idx == 0)
			break;
		filelist.pcur = filelist.pcur->next;
	}

	if (idx != 0) // 범위를 넘어서면 에러
		return false;
	else // pcur은 d 옵션 실행 대상 파일을 가리키게된다.
		return true;
}
// d 옵션 실행
int d_option(int set_idx, char *list_idx)
{
	if (!checking_list_idx(list_idx)) // list_idx 정상 입력X -> 에러
		return false;

	char *rmFile_path = NULL; // 삭제될 파일의 경로
	rmFile_path = Node_Remove(&filelist, filelist.pcur);
	
	if (remove(rmFile_path) < 0) { // 파일 제거
		fprintf(stderr, "remove error for %s\n", rmFile_path);
		exit(1);
	}
	else { // 제거 알림 출력 후 로그에 기록
		printf("\"%s\" has been deleted in #%d\n\n", rmFile_path, set_idx);
		record_log("DELETE", rmFile_path);
		return true;
	}
}
// i 옵션 실행
int i_option(void)
{
	char buf[BUFFER_SIZE];
	char *rmFile_path = NULL; // 삭제될 파일 경로
	filelist.pcur = filelist.hcur->next; // pcur은 파일 세트의 첫 파일에 위치
	Node *nextNode = NULL;
	/* index로 입력된 파일 세트의 파일들을 하나씩 삭제 여부 확인 */
	while (filelist.pcur != NULL) {
		printf("Delete \"%s\"? [y/n] ", filelist.pcur->data.path);
		fgets(buf, BUFFER_SIZE, stdin);
		buf[strlen(buf) - 1] = '\0';
		// 'y', 'Y', 'n', 'N'의 정상 입력될 때까지 재 입력 요구
		while (strcasecmp(buf, "y") && strcasecmp(buf, "n")) {
			fprintf(stderr, "please input Y/y or N/n\n");
			printf(">> ");
			fgets(buf, BUFFER_SIZE, stdin);
			buf[strlen(buf) - 1] = '\0';
		} // 'y'라면 파일 제거
		if (!strcasecmp(buf, "y")) {
			nextNode = filelist.pcur->next; // nextNode는 다음 파일에 위치
			rmFile_path = Node_Remove(&filelist, filelist.pcur); // linked list에서 제거
			if (remove(rmFile_path) < 0) { // 파일 제거
				fprintf(stderr, "remove error\n");
				exit(1);
			}
			record_log("DELETE", rmFile_path); // 로그에 기록
			filelist.pcur = nextNode; // pcur은 다음 파일에 위치
		}
		else // 'n'라면 제거하지 않고 다음 파일에 위치
			filelist.pcur = filelist.pcur->next;
	}
	printf("\n");
	return true;
}
// f, t 옵션 실행
int f_t_option(int set_idx, char op)
{
	Node *nwFile = filelist.hcur->next; // 가장 최근 수정된 파일 -> 남길 파일
	Node *nextNode = NULL;

	char *nwFile_path = NULL; // 남길 파일의 경로
	char *rmFile_path = NULL; // 제거될 파일의 경로
	char nwFile_time[20]; // 남길 파일 마지막 수정 시간
	struct tm timeinfo;
	/* 가장 최근 수정된 파일 탐색 -> nwFile을 그 파일에 위치시킨다. */
	filelist.pcur = filelist.hcur->next;
	while (filelist.pcur->next != NULL) {
		if (nwFile->data.mtime < filelist.pcur->next->data.mtime)
			nwFile = filelist.pcur->next;
		filelist.pcur = filelist.pcur->next;
	}

	nwFile_path = nwFile->data.path; // 남길 파일의 경로

	strftime(nwFile_time, 20, "%Y-%m-%d %H:%M:%S", 
			localtime_r(&nwFile->data.mtime, &timeinfo)); // 남길 파일의 mtime 출력 형태 변형
	/* 나머지 파일 제거 */
	filelist.pcur = filelist.hcur->next;
	while (filelist.pcur != NULL) {
		if (filelist.pcur == nwFile) { // 남길 파일은 제거 대상x
			filelist.pcur = filelist.pcur->next;
			continue;
		}
		nextNode = filelist.pcur->next;
		rmFile_path = Node_Remove(&filelist, filelist.pcur); // linked list에서 제거, 경로 return받음

		if (op == 'f') { // f 옵션
			if (remove(rmFile_path) < 0) { // 파일 제거
				fprintf(stderr, "remove error for %s\n", rmFile_path);
				exit(1);
			}
			record_log("DELETE", rmFile_path); //  로그에 기록
		}
		else if (op == 't') { // t 옵션
			move_to_trash(rmFile_path); // trash로 이동
			record_log("REMOVE", rmFile_path); // 로그에 기록
		}
		filelist.pcur = nextNode;
	}
	/* 옵션에 따른 알림 출력 */
	if (op == 'f')
		printf("Left file in #%d : %s (%s)\n\n", set_idx, nwFile_path, nwFile_time);
	else if (op == 't')
		printf("All files in #%d have moved to Trash except \"%s\" (%s)\n\n",
				set_idx, nwFile_path, nwFile_time);
	return true;
}
// trash로 파일 이동
void move_to_trash(char *file_path)
{
	int idx = 1; // 동일한 이름의 파일이 존재 -> index를 붙힘
	char idx_char[20];
	char filename[FILE_MAX_LEN];
	char *extension = NULL;
	char rename_path[PATH_MAX];
	char *ptr = NULL;

	strcpy(filename, get_path_to_filename(file_path)); // 제거될 파일 이름만 추출
	/* 파일의 제거될 경로 지정 (trashdir + '/' + 파일 이름) */
	strcpy(rename_path, trashdir);
	
	ptr = rename_path + strlen(rename_path);
	*ptr++ = '/';
	strcpy(ptr, filename);
	/* 파일 trash로 이동 */
	if (access(rename_path, F_OK) < 0) { // 동일 파일 없을 경우 정상 제거
		if (rename(file_path, rename_path) < 0) {
			fprintf(stderr, "rename error for %s\n", file_path);
			exit(1);
		}
		record_trashfile_info(file_path, rename_path); // 해당 파일에 대한 정보 기록 파일 생성
	}
	else { // 동일 파일 존재
		char rename_path2[PATH_MAX];
		extension = get_filename_to_extension(filename);

		while (1) {

			idx++; // index 하나씩 증가시킴
			sprintf(idx_char, "%d", idx);
			strcpy(rename_path2, rename_path);

			if  (extension == NULL) { // 파일의 확장자가 없는 경우 -> 파일이름.idx
				ptr = rename_path2 + strlen(rename_path2);
				*ptr++ = '.';
				strcpy(ptr, idx_char);
			}
			else { // 파일의 확장자가 있는 경우 -> 파일이름.idx.(확장자)
				ptr = get_filename_to_extension(rename_path2);
				strcpy(ptr, idx_char);
				ptr = ptr + strlen(idx_char);
				*ptr++ = '.';
				strcpy(ptr, extension);
			}

			if (access(rename_path2, F_OK) < 0) { // 동일 파일 없을 경우 정상 제거
				if (rename(file_path, rename_path2) < 0) {
					fprintf(stderr, "rename error for %s\n", file_path);
					exit(1);
				}
				break;
			}
			else continue; // 동일 파일 존재 -> 같은 과정 반복
		}
		record_trashfile_info(file_path, rename_path2); // 해당 파일에 대한 정보 기록 파일 생성
	}
}
// 로그 기록
void record_log(char *command, char *file_path)
{
	int fd;
	char buf[BUFFER_SIZE];
	char record_time[25];
	time_t curTime = time(NULL); // 제거 시간
	struct tm ptm;
	struct passwd *passwd = getpwuid(getuid()); // 사용자 이름

	if ((fd = open(logdir, O_WRONLY | O_APPEND)) < 0) {
		fprintf(stderr, "open error for %s\n", logdir);
		exit(1);
	}
	/* 로그에 기록 */
	localtime_r(&curTime, &ptm);
	strftime(record_time, 25, "%Y-%m-%d %H:%M:%S", &ptm);

	sprintf(buf, "[%s] %s %s %s\n", command, file_path, record_time, passwd->pw_name);
	write(fd, buf, strlen(buf));

	close(fd);
}
// trash에 제거된 파일의 정보가 기록된 파일 생성
void record_trashfile_info(char *file_path, char *rename_path)
{
	char infofile[FILE_MAX_LEN]; // 정보 기록 파일 이름
	char current_path[PATH_MAX]; // 현재 작업 디렉토리
	char delete_time[20];
	char buf[40];
	struct stat statbuf;
	struct tm tm;
	int fd;
	time_t curTime = time(NULL); // 제거된 날짜와 시간
	
	/* 정보 기록 파일 이름 : 파일이름.trashinfo 
	   기록 내용 : (파일의 원래 경로)\n(파일 사이즈)\n(제거된 날짜)T(제거된 시간)\n */
	strcpy(infofile, get_path_to_filename(rename_path));
	strcat(infofile, ".trashinfo");

	getcwd(current_path, PATH_MAX);
	chdir(trashdir_info); // 정보 관리 디렉토리로 작업 디렉토리 변경

	if ((fd = open(infofile, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0) { // 파일 생성
		fprintf(stderr, "open error for %s/%s\n", trashdir_info, infofile);
		exit(1);
	}
	
	if (stat(rename_path, &statbuf) < 0) { // stat 함수
		fprintf(stderr, "stat error for %s\n", rename_path);
		exit(1);
	}

	write(fd, file_path, strlen(file_path)); // 파일의 원래 경로 기록
	write(fd, "\n", 1);
	
	strftime(delete_time, 20, "%Y-%m-%dT%H:%M:%S", localtime_r(&curTime, &tm));
	sprintf(buf, fm_off_t"\n%s\n", statbuf.st_size, delete_time);

	write(fd, buf, strlen(buf)); // 파일 사이즈와 제거된 날짜, 시간 기록
	
	chdir(current_path); // 작업 디렉토리 복귀
	close(fd);
}
// 사용자별로 log 파일, trash 디렉토리 생성
void make_userfile(void)
{
	int fd;
	char current_dir[PATH_MAX];

	if (getcwd(current_dir, PATH_MAX) == NULL) { // 현재 작업 디렉토리 저장
		fprintf(stderr, "getcwd error\n");
		exit(1);
	}

	if (access(logdir, F_OK) < 0) { // 로그 파일 존재하지 않는다면 새로 생성
		if ((fd = open(logdir, O_RDONLY | O_CREAT | O_TRUNC, 0644)) < 0) {
			fprintf(stderr, "open error for %s\n", logdir);
			exit(1);
		}
	}
	
	if (chdir(getenv("HOME")) < 0) { // 홈 디렉토리로 작업 디렉토리 변경
		fprintf(stderr, "chdir error\n");
		exit(1);
	}
	if (access(".Trash", F_OK) < 0) { // .Trash 디렉토리 존재하지 않는다면 새로 생성
		if (mkdir(".Trash", 0755) < 0) {
			fprintf(stderr, "mkdir error for %s\n", ".Trash");
			exit(1);
		}
	}

	if (chdir(".Trash") < 0) { // .Trash로 작업 디렉토리 변경
		fprintf(stderr, "chdir error\n");
		exit(1);
	}
	if (access("files", F_OK) < 0) { // files 디렉토리 존재하지 않으면 새로 생성
		if (mkdir("files", 0755) < 0) {
			fprintf(stderr, "mkdir error for %s\n", "files");
			exit(1);
		}
	}
	if (access("info", F_OK) < 0) { // info 디렉토리 존재하지 않으면 새로 생성
		if (mkdir("info", 0755) < 0) {
			fprintf(stderr, "mkdir error for %s\n", "info");
			exit(1);
		}
	}
	chdir(current_dir); // 작업 디렉토리 복귀
}
// 파일 경로 -> 파일 이름
char *get_path_to_filename(char *path)
{
	char *filename = strrchr(path, '/');
	return (filename == NULL) ? NULL : filename + 1;
}
// 파일 이름 -> 확장자
char *get_filename_to_extension(char *filename)
{
	char *extension = strrchr(filename, '.');
	return (extension == NULL) ? NULL : extension + 1;
}
// help 명령어
void command_help(void)
{
	puts("Usage:");
	puts("  >  fsha1 -e [FILE_EXTENSION] -l [MINSIZE] -h [MAXSIZE] \
-d [TARGET_DIRECTORY] -t [THREAD_NUM]");
	puts("      >> delete -l [SET_INDEX] -d [OPTARG] -i -f -t");
	puts("  > trash -c [CATEGORY] -o [ORDER]");
	puts("  > restore [RESTORE_INDEX]");
	puts("  > help");
	puts("  > exit");
	puts("");
}
