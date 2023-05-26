#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <stdbool.h>
#include <ctype.h>

#define CLASS_NUM 20181321

#define TRUE	1
#define FALSE	0

#define ARR_LEN 1024 // 탐색 file 최대 개수
#define CHAR_MAX 1023 // 비교할 파일의 최대 문자 개수

/* option */
#define OP_NUM 8 // 옵션 개수

bool qOp = FALSE;
bool sOp = FALSE;
bool iOp = FALSE;
bool rOp = FALSE;
bool vOp = FALSE;
bool cOp = FALSE;
bool eOp = FALSE;
bool wOp = FALSE;

/* max 매크로 */
#ifndef max
#define max(a,b) (((a) > (b)) ? (a) : (b))
#endif

// 파일에 대한 stat, path 정보를 담는 구조체
typedef struct stat Stat;

struct _fileInfo {
	Stat stif; // 파일 stat정보
	char path[PATH_MAX]; // 파일의 PATH
};
typedef struct _fileInfo FData;

// 이름과 크기가 같은 여러 파일들을 담을 배열
struct _fileList {
	FData* arr[ARR_LEN];
	char filename[255]; // find 인자로 입력받는 파일 이름
	char inputPath[PATH_MAX]; // find 명령어의 realpath 인자
	int numOfData; // 파일 개수
};

typedef struct _fileList fileList;

// 함수
void ListInit(fileList* fl); // 구조체 초기화
int AddList(fileList* fl, char* path); // 탐색된 파일을 Array에 추가
void StatSorting(FData* arr[], int num); // 파일의 PATH를 ASCII 기준 정렬
void ListClean(fileList* fl); // Array 초기화

char* modeFormat(mode_t st_mode); // stat 구조체의 st_mode 멤버 포맷 변환
void printStat(fileList* fl); // 탐색된 파일들 출력
void help (void); // help 명령어

int sizeOfDirectory(char* path); // 디렉토리의 size 계산
int directorySearch(fileList* fl, char* path); // 탐색 대상 파일을 재귀적으로 탐색
int compareFiles(char* fpath, char* ipath, int printDiff); // 파일 비교, 반환값 : 오류 -> -1, same -> 1, diff -> 0
void find (fileList* fl); // find 명령어 입력받고 처리
void prompt(void); // 프롬프트
void idxOption(char* fpath, char* ipath, char op[]); // 입력받은 index와 option에 따라 처리

/* scandir() 함수의 filter 함수 */
int file_select(const struct dirent *entry)
{
	return (strcmp(entry->d_name, ".") && strcmp(entry->d_name, "..")); // '.' , '..' 이름의 directory는 제외
}

/* i, w옵션에 따른 string을 비교하는 함수 */
int op_strcmp(const char *str1, const char *str2)  
{
	unsigned char a, b;
	int idx1 = 0, idx2 = 0;
	int ch;

	if (iOp && !wOp) { // i옵션만 TRUE
		while (1) {
			// 둘 다 소문자로 변환해서 비교
			a = tolower((unsigned char)*(str1 + idx1));
			b = tolower((unsigned char)*(str2 + idx2));
			ch = a - b;

			if (ch != 0 || !*(str1 + idx1))
				return ch;
			idx1++, idx2++;
		}
	}
	else if (!iOp && wOp) { // w옵션만 TRUE
		while (1) {
			// 공백은 무시하고 한 글자 건너뛴다.
			while (*(str1 + idx1) == ' ')
				idx1++;
			while (*(str2 + idx2) == ' ')
				idx2++;
			// 두 문자 비교
			a = (unsigned char)*(str1 + idx1);
			b = (unsigned char)*(str2 + idx2);
			ch = a - b;

			if (ch != 0 || !*(str1 + idx1))
				return ch;
			idx1++, idx2++;
		}
	}
	else { // i, w옵션 둘 다 TRUE
		while (1) {
			// 공백은 무시하고 한 글자 건너뛴다.
			while (*(str1 + idx1) == ' ')
				idx1++;
			while (*(str2 + idx2) == ' ')
				idx2++;
			// 둘 다 소문자로 변환해서 비교
			a = tolower((unsigned char)*(str1 + idx1));
			b = tolower((unsigned char)*(str2 + idx2));
			ch = a - b;

			if (ch != 0 || !*(str1 + idx1))
				return ch;
			idx1++, idx2++;
		}		
	}

}

int main()
{
	struct timeval startTime, endTime;
	struct timeval timeDiff;
	gettimeofday(&startTime, NULL); // 시간 측정 시작

	prompt(); // 시간 측정 시작 후 프롬프트 출력
	 
	// exit -> 프롬프트 종료
	puts("Prompt End");
	gettimeofday(&endTime, NULL); // 시간 측정 종료

	/* 실행 시간 계산 */
	timeDiff.tv_sec = endTime.tv_sec - startTime.tv_sec; 
	timeDiff.tv_usec = endTime.tv_usec - startTime.tv_usec; 
	if (timeDiff.tv_usec < 0) { // usec가 음수인 경우 시간 계산
		timeDiff.tv_sec--;
		timeDiff.tv_usec += 1000000;
	}

	printf("Runtime: %ld:%ld(sec:usec)\n", 
			timeDiff.tv_sec, timeDiff.tv_usec);
		
	exit(0); // 프로그램 종료
}

/* 프롬프트 - find 명령어를 처리하고 같은 이름과 크기를 같는 파일을 탐색하는 find함수 호출 */
void prompt(void)
{
	char* filename = NULL;
	char filename_rp[PATH_MAX];
	char* inputPath = NULL;
	char inputPath_rp[PATH_MAX];
	char* inst = NULL;
	char input[1024];

	fileList filelist; // 구조체 선언
	ListInit(&filelist); // 초기화

	while (1)
	{
		printf("%d> ", CLASS_NUM); // "학번>" 으로 프롬프트 시작, find 명령어 입력 대기
		
		fgets(input, 1024, stdin);	// 입력

		if (strcmp(input,"\n") == 0) // 엔터키만 입력시 프롬프트 재시작
			continue;

		char* ptr = strchr(input, '\n'); // 입력받은 문자열 끝의 '\n'문자 지움
		if (ptr) {
			*ptr = '\0';
		}

		if ((inst = strtok(input, " ")) == NULL) // inst에 첫번째 인자 저장, NULL값이라면 다시 " "로 쪼개서 저장 (명령어 앞의 " " 삭제)
			inst = strtok(NULL, " ");
		
		if (strcmp(inst, "exit") == 0) { // exit -> 구조체 데이터 남아있다면 모두 지우고 프롬프트 탈출
			if (filelist.numOfData != 0)
				ListClean(&filelist);
			return;
		}

		else if (strcmp(inst, "find") != 0) { // 첫 번째 인자가 "find"가 아니라면 help() 함수 호출 -> 프롬프트 재시작
			help();
			continue;
		}
		
		/* 입력받은 find 문자열 처리 */
		else {
		
			filename = strtok(NULL, " "); // 다음 인자인 filename 분리
			if (filename == NULL) { // filename 인자에 입력이 없다면 에러 처리 후 프롬프트 재시작
				fprintf(stderr, "Input Error\n");
				continue;
			}

			inputPath = strtok(NULL, " "); // 다음 인자인 path 분리
			if (inputPath == NULL) { // path 인자에 입력이 없다면 에러 처리 후 프롬프트 재시작
				fprintf(stderr, "Input Error\n");
				continue;
			}

			char* tmp = strtok(NULL, " "); // 뒤에 문자가 더 남아 있다면 에러 처리 후 프롬프트 재시작
			if(tmp != NULL) {
				fprintf(stderr, "Input Error\n");
				continue;
			}
		}

		if (realpath(filename, filename_rp) == NULL) {  // 입력받은 파일 이름을 절대경로로 변환
			fprintf(stderr, "can't find %s\n", filename); // 실패 시 존재하지 않는 파일
			continue;
	    }

		/* file의 name만을 추출 */
		char* tmp = strrchr(filename, '/'); // [filename] 칸에 입력된 문자열중 가장 마지막의 '/'가 있는 포인터를 찾는다.			
		if (tmp != NULL) { // '/'가 존재한다면,
			while (*tmp == '/' && *(tmp + 1) == '\0') { // filename 뒤에 '/'가 있다면 모두 지움 (ex. dir1//// -> dir1)
				*tmp = '\0';
				tmp--;
			}
			if ((tmp = strrchr(filename, '/')) != NULL) // 다시 '/'의 포인터 찾는다. 그리고 존재한다면,
				filename = tmp + 1; // 그 바로 뒤의 포인터를 filename에 대입 (ex. ./dir1 -> dir1)
		}

		strcpy(filelist.filename, filename); // filelist 구조체의 filename 멤버에 파일 이름 저장

		if (!AddList(&filelist, filename_rp)) { // Array에 [FILENAME] 정보 저장, 실패 시 프롬프트 재시작
			ListClean(&filelist);
			continue;
		}

		realpath(inputPath, inputPath_rp); // [PATH] 인자에 입력받은 것을 절대경로로 변환

		strcpy(filelist.inputPath, inputPath_rp); // filelist 구조체의 inputPath 멤버에 저장

		find(&filelist); // 파일을 탐색하는 함수 호출

		/* 모든 작업을 마치고 구조체 초기화 후 프롬프트 재시작 */
		ListClean(&filelist);

		if (eOp) // e Option TRUE -> 프롬프트 종료
			return;
	}
}

/* 파일 탐색, 출력, index와 option을 처리하고 파일을 비교하는 함수를 호출한다. */
void find (fileList* fl)
{
	char input[1024]; // index, option들의 string을 입력받기 위한 변수
	char *pInput; // index와 각 option들을 쪼개기 위한 변수
	char op[OP_NUM]; // 입력받은 option들
	int idx; // 입력받은 index
	int idxLen; // 입력받은 index 길이
	bool continueFlag; // option들 저장하는 for문 탈출 후 while문 continue 여부

	if (directorySearch(fl, fl->inputPath) == FALSE) // 파일 검색 실패시 프롬프트로 리턴
		return;
	
	StatSorting(fl->arr, fl->numOfData); // 탐색되어 Array에 저장된 파일 stat 정보들을 path를 기준으로 정렬
	printStat(fl); // 탐색된 파일들의 Array 리스트 출력

	/* 이름, 크기가 같은 파일이 없을 경우 (None)출력후 프롬프트로 리턴 */
	if(fl->numOfData == 1) {
		puts("(None)");
		return;
	}
    
	/* 파일 리스트 출력 후 index와 option 입력 받음 */
	while(1) 
	{
		continueFlag = FALSE; // Flag 초기화
		for (int i = 0; i < OP_NUM; i++) // op 구성 초기화
			op[i] = '\0';
		
		/* 옵션 초기화 */
		qOp = FALSE;
		sOp = FALSE;
		iOp = FALSE;
		rOp = FALSE;
		vOp = FALSE;
		cOp = FALSE;
		eOp = FALSE;
		wOp = FALSE;

		printf(">> "); // 입력 대기
		fgets(input, 1024, stdin); // input 요구

		/* Nothing input -> 재입력 요구 */
		if (strcmp(input,"\n") == 0) {
			fprintf(stderr, "Nothing Input\n");
			continue;
		}

		/* 입력받은 string의 마지막 문자의 Enter키 제거 */
		char* ptr = strchr(input, '\n');
		if (ptr) {
			*ptr = '\0';
		}

		/* 
		   index 부분만 쪼개서,
		   1. 숫자인지 확인
		   2. 유효한 숫자인지 확인
		*/
		if ((pInput = strtok(input, " ")) == NULL)
			pInput = strtok(NULL, " "); // index부분만 취함

		idxLen = strlen(pInput); // index의 길이

		/* 1. index의 각각의 문자들이 숫자인지 판단 */
		for (int i = 0; i < idxLen; i++) {
			if (pInput[i] < '0' || pInput[i] > '9') {
				fprintf(stderr, "Index Error\n");
				continueFlag = TRUE; // 숫자가 아닐 시, Flag -> TRUE
				break;
			}
		}
		if (continueFlag)// 에러 처리 후 continue로 돌아가서 재 입력 요구
			continue;

		/* 2. index가 유효한지 확인 */
		idx = atoi(pInput);
		if (idx <= 0 || idx > fl->numOfData - 1) { // 0을 제외한 자연수이면서, 존재하는 index여야함
			fprintf(stderr, "Index Error\n");
			continue;
		}

		/* option 처리 */
		while ((pInput = strtok(NULL, " ")) != NULL) {
			
			if (strcmp(pInput, "q") == 0)
				qOp = TRUE;
			else if (strcmp(pInput, "s") == 0)
				sOp = TRUE;
			else if (strcmp(pInput, "i") == 0)
				iOp = TRUE;
			else if (strcmp(pInput, "r") == 0)
				rOp = TRUE;
			else if (strcmp(pInput, "v") == 0)
				vOp = TRUE;
			else if (strcmp(pInput, "c") == 0)
				cOp = TRUE;
			else if (strcmp(pInput, "e") == 0)
				eOp = TRUE;
			else if (strcmp(pInput, "w") == 0)
				wOp = TRUE;
			else {
 				fprintf(stderr, "Option Error\n");
				continueFlag = TRUE; // 유효하지 않은 option, Flag -> TRUE
				break;
			}

			if (cOp && eOp) { // c와 e가 중복된 경우 에러
				fprintf(stderr, "Option 'c' and 'e' can't be used at the same time.\n");
				continueFlag = TRUE;
				break;
			}
		}

		if (continueFlag) // 에러 처리 후 continue로 돌아가서 재 입력 요구
			continue;

		/* 각 option들의 입력 여부에 따라 FALSE -> TRUE 전환 */
		int i = 0;

		if (qOp)
			op[i++] = 'q';
		if (sOp)
			op[i++] = 's';
		if (iOp)
			op[i++] = 'i';
		if (rOp)
			op[i++] = 'r';
		if (vOp)
			op[i++] = 'v';
		if (cOp)
			op[i++] = 'c';
		if (eOp)
			op[i++] = 'e';
		if (wOp)
			op[i++] = 'w';

		/* 주어진 index, option들에 따라 파일 비교 시작 (idxOption() 함수 호출)*/
		if (vOp) // v옵션 TRUE -> 비교 기준(index 0)과 대상(input index)을 바꿈
			idxOption(fl->arr[idx]->path, fl->arr[0]->path, op);
		else
			idxOption(fl->arr[0]->path, fl->arr[idx]->path, op);

		/* 비교를 마치고 프롬프트 복귀 */
		if (cOp) { // c옵션이 TRUE -> 프롬프트 복귀X, 파일 Array 목록 재출력 후 다시 index, option 입력 받음 
			printStat(fl);
			continue;
		}

		return;
	}
}

/* 종합적으로 두 파일을 비교하는 함수 */
void idxOption(char* fpath, char* ipath, char op[])
{	
	/* 기준 파일과 대상 파일의 각 PATH에 대한 상대 PATH를 추출한다.
	   EX) 대상 파일의 index가 1인 경우의 상대 PATH 추출 과정 
	   	   Index 0의 path : /home/OSLAB/P1/dir1
	   	   Index 1의 path : /HOME/oslab/p1/dir2/dir1
			
		   --> comp_Fpath : dir1
		   --> comp_Ipath : dir2/dir1
	*/

	/* 각각의 절대 PATH로 초기화 */
	char* comp_Fpath = fpath;
	char* comp_Ipath = ipath;

	/* 상대 PATH 추출 */
	while(1) {
		bool Outerbreak = FALSE; // 2중 반복문 탈출을 위한 변수 선언

		if (*comp_Fpath == '/' && *comp_Ipath == '/') {
			comp_Fpath++;
			comp_Ipath++;
		}

		else {
			while (*comp_Fpath != '/' || *comp_Ipath != '/') {
				if (*comp_Fpath != *comp_Ipath) {
					if (*comp_Fpath == '/' || *comp_Ipath == '/' ) {
						comp_Fpath--;
						comp_Ipath--;
					}
					Outerbreak = TRUE;
					break;
				}
				comp_Fpath++;
				comp_Ipath++;
			}
		}

		if(Outerbreak)
			break;
	}

	while (*(comp_Fpath - 1) != '/') {
		comp_Fpath--;
	}
	while (*(comp_Ipath - 1) != '/') {
		comp_Ipath--;
	}

	/* 비교될 두 파일 stat 정보 가져옴 */
	Stat fstat, istat;
	stat(fpath, &fstat);
	stat(ipath, &istat);

	/* 
		탐색된 파일이 Directory인 경우와 Regular file인 경우에 따른 비교 
	*/

	/* Directory */
	if (S_ISDIR(fstat.st_mode) && S_ISDIR(istat.st_mode)) {

		int name_countF = 0, name_countI = 0; // 각 디렉토리의 파일 개수

		Stat new_Fstat, new_Istat; // 디렉토리 내부 파일들의 stat

		char new_Fpath[PATH_MAX], new_Ipath[PATH_MAX]; // 디렉토리 내부 파일들의 PATH
		
		struct dirent** name_listF = NULL; // 기준 디렉토리 내부 파일 list
		struct dirent** name_listI = NULL; // 대상 디렉토리 내부 파일 list

		name_countF = scandir(fpath, &name_listF, file_select, alphasort); // scandir 실패 시 오류 처리 후 리턴
		if (name_countF < 0) {
			fprintf(stderr, "scandir() function error for %s\n", fpath);
			return;
		}

		name_countI = scandir(ipath, &name_listI, file_select, alphasort); // scandir 실패 시 오류 처리 후 리턴
		if (name_countI < 0) {
			fprintf(stderr, "scandir() function error for %s\n", ipath);
			return;
		}

		if (name_countF == 0 && name_countI == 0) // 두 디렉토리 모두 비어있을 시 차이X -> 출력없이 리턴
			return;

		int compareASCII[name_countI]; // ASCII순으로 비교하기 위해 활용될 Array
		int sameIdx; // 기준 디렉토리 내부 파일과 같은 이름의 대상 디렉토리 파일 index

		int fnum, inum = 0;
		int ii;

		/* 
			ex) 기준 디렉토리 name_listF의 인덱스가 fnum = 0 (filename : 'c')이고 대상 디렉토리 인덱스 0부터 비교 시작
			   -> 대상 디렉토리의 list의 filename이 {'a', 'b', 'c', 'd', 'e'} 라면, 첫 비교 반복문에서
			   	  compareASCII = {1, 1, 0, 0, 0}이고 compareASCII[2] 에서 filename이 'c'로 같으므로 0이되고 break.
			   -> sameIdx = 2의 결과값이 나옴.
			   -> compareASCII[0], compareASCII[1] 은 1이므로 ASCII코드 우선순위가 앞서므로 대상 디렉토리에만 존재하는 파일임.
			   -> 조건에 따른 결과값을 출력하고 fnum = 1, inum = sameIdx = 2 부터 다시 비교 시작
			   -> 하나의 디렉토리가 모두 비교가 끝난다면 모든 반복문을 끝내고 아직 비교가 끝나지 않은 디렉토리 파일에 대해 "Only in ~" 출력  
		*/
		for (fnum = 0; fnum < name_countF; fnum++) {
			memset(compareASCII, 0, sizeof(compareASCII)); // compareASCII 0으로 초기화
			sameIdx = -1; // sameIdx = -1로 초기화

			for (ii = inum; ii < name_countI; ii++) { // 기준 디렉토리 fnum번째 파일과 대상 디렉토리 파일들과의 이름 비교

				/* 이름이 같으면 sameIdx에 대상 디렉토리 index 저장 후 break */
				if ((compareASCII[ii] = strcmp(name_listF[fnum]->d_name, name_listI[ii]->d_name)) == 0) {
					sameIdx = ii;
					break;
				}
			}
			
			if (sameIdx == -1) { // 기준 디렉토리 f번째 파일과 이름이 같은 파일이 없는 경우 ex) compareASCII = {1, 1, -1, -1, ...}
				for (ii = inum; ii < name_countI; ii++) {
					if (compareASCII[ii] < 0) // 기준 디렉토리 f번째 파일보다 ASCII 순이 나중인 경우 for문 빠져나옴.
						break;

					printf("Only in %s: %s\n", comp_Ipath, name_listI[ii]->d_name); // 아스키코드 순 앞선 경우
				}
				printf("Only in %s: %s\n", comp_Fpath, name_listF[fnum]->d_name); // 기준 디렉토리에만 존재하는 f번째 파일
				inum = ii; // 대상 디렉토리의 파일 중, 아직 비교되지 않은 인덱스 저장 
			}
			else { // 기준 디렉토리 f번째 파일과 이름이 같은 파일이 대상 디렉토리에 존재
				for (ii = inum; ii < sameIdx; ii++)
					printf("Only in %s: %s\n", comp_Ipath, name_listI[ii]->d_name); // sameIdx 전까지는 대상 디렉토리에만 존재하는 파일
				
				sprintf(new_Fpath, "%s/%s", fpath, name_listF[fnum]->d_name); // sameIdx에 해당하는 기준 디렉토리 파일 경로 재설정, stat정보
				stat(new_Fpath, &new_Fstat);

				sprintf(new_Ipath, "%s/%s", ipath, name_listI[sameIdx]->d_name); // sameIdx에 해당하는 대상 디렉토리 파일 경로 재설정, stat정보
				stat(new_Ipath, &new_Istat);

				if (S_ISREG(new_Fstat.st_mode) && S_ISREG(new_Istat.st_mode)) { // 이름이 같은 두 파일이 모두 정규파일인 경우
					int retS;
					retS = compareFiles(new_Fpath, new_Ipath, FALSE);

					if (retS == 0) { // 반환값 0 -> 내용이 다른 파일
						if (qOp) // 차이점 출력X
							printf("Files %s/%s and %s/%s differ\n", comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);
						else { // 차이점 출력
							printf("diff ");
							for (int i = 0; i < OP_NUM; i++) { // "diff -[option ...] comp_Fpath/filename, comp_Ipath/filename"
								if (op[i] == '\0')
									break;
								printf("-%c ", op[i]);
							}
							printf("%s/%s %s/%s\n", comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);
							compareFiles(new_Fpath, new_Ipath, TRUE); // 파일의 내용 차이점 비교하고 출력
						}
					}
					else if (retS == 1) { // 내용이 같은 파일
						if (sOp) // s Option -> 내용이 동일한 경우 알림
							printf("Files %s/%s and %s/%s are identical\n", comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);
					}
				}
				else if (S_ISDIR(new_Fstat.st_mode) && S_ISDIR(new_Istat.st_mode)) { // 이름이 같은 두 파일이 모두 디렉토리
					if (rOp) // r Option -> 두 디렉토리 비교 시 재귀탐색
						idxOption(new_Fpath, new_Ipath, op);
					else // r Option FALSE
						printf("Common subdirectories: %s/%s and %s/%s\n", comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);
				}
				else if (S_ISREG(new_Fstat.st_mode)) // 기준 파일만 정규파일
					printf("File %s/%s is a regular file while file %s/%s is a directory\n", 
							comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);
				else // 대상 파일만 정규파일
					printf("File %s/%s is a directory while file %s/%s is a regular file\n", 
							comp_Fpath, name_listF[fnum]->d_name, comp_Ipath, name_listI[ii]->d_name);

				inum = sameIdx + 1; // sameIdx 다음 index부터 fnum과 비교 반복문 시작
			}

			if (inum >= name_countI) // inum, 대상 디렉토리 파일들의 비교가 모두 끝났다면 반복문 빠져나옴.
				break;
		}

		if (fnum < name_countF) { // fnum, 기준 디렉토리에서 아직 비교되지 않은 파일이 있는 경우 -> 기준 디렉토리에만 존재하는 파일
			for (int i = fnum + 1; i < name_countF; i++)
				printf("Only in %s: %s\n", comp_Fpath, name_listF[i]->d_name);
		}
		else if (inum < name_countI) { // inum, 대상 디렉토리에서 아직 비교되지 않은 파일이 있는 경우 -> 대상 디렉토리에만 존재하는 파일
			for (int i = inum; i < name_countI; i++)
				printf("Only in %s: %s\n", comp_Ipath, name_listI[i]->d_name);
		}
	}
	/* Regular Files */
	else if (S_ISREG(fstat.st_mode) && S_ISREG(istat.st_mode)){
		int retS;
		retS = compareFiles(fpath, ipath, FALSE);

		if (retS == 0) { // 내용이 다른 파일이라면,
			if (qOp) // q Option -> 차이점 출력 X
				printf("Files %s and %s differ\n", comp_Fpath, comp_Ipath);
			else { // 차이점 출력
				compareFiles(fpath, ipath, TRUE);
			}
		}
		else if (retS == 1) { // 내용이 같은 파일
			if (sOp) // s Option -> 내용 동일한 경우 알림
				printf("Files %s and %s are identical\n", comp_Fpath, comp_Ipath);
		}
	}
}

/* 같은 이름과 크기를 갖는 파일을 찾기 위해, 입력된 [PATH]인자부터 시작해서 재귀적으로 탐색하는 함수 */
int directorySearch(fileList* fl, char* path)
{
	int name_count = 0;
	char repath[PATH_MAX]; // 재귀 탐색을 위한 New path

	Stat fstat = fl->arr[0]->stif; // 기준 index 0의 stat
	Stat nstat; // 비교 대상 stat

	struct dirent** name_list = NULL;

	name_count = scandir(path, &name_list, file_select, alphasort);

	if (name_count < 0) {
		fprintf(stderr, "scandir() fuction Error: %s\n", path);
		return FALSE; // 첫 실행에 대해서만 error -> FALSE(=0) 프롬프트 리턴
	}                 // 탐색 중간에는 실패해도 에러 메시지만 출력

	for (int i = 0; i < name_count; i++) {

		if (!strcmp(path, "/")) // 해당 파일의 new path를 repath에 저장
			sprintf(repath, "%s%s", path, name_list[i]->d_name);
		else
			sprintf(repath, "%s/%s", path, name_list[i]->d_name);
		
		if (lstat(repath, &nstat) != 0) // 해당 파일 lstat 정보
			continue;
		
		/* Directory OR Regular Files가 아닌경우 탐색 대상 제외 */
		if (!S_ISDIR(nstat.st_mode) && !S_ISREG(nstat.st_mode))
			continue;
		
		/* Directory의 경우 재귀 호출 */
		if (S_ISDIR(nstat.st_mode)) {
			directorySearch(fl, repath);
		}

		/* index 0의 PATH와 같은 PATH를 가진 경우, 탐색에서 제외(같은 파일임) */
		if (strcmp(fl->arr[0]->path, repath) == 0)
			continue;
		
		/* 이름이 다른 경우 탐색 제외 */
		if (strcmp(fl->filename, name_list[i]->d_name))
			continue;

		/* 두 파일의 형식이 같지 않은 경우, 탐색에서 제외 */
		if (!((S_ISDIR(fstat.st_mode) && S_ISDIR(nstat.st_mode) || S_ISREG(fstat.st_mode) && S_ISREG(nstat.st_mode))))
			continue;

		/* directory의 경우 내부 파일들의 크기 합을 재귀적으로 계산 */
		if (S_ISDIR(nstat.st_mode)) {
			nstat.st_size = sizeOfDirectory(repath);
		}

		/* 크기가 같은 대상 파일만 ListArray에 추가 */
		if (fstat.st_size != nstat.st_size)
			continue;

		/* 최종적인 탐색 대상 파일 Array에 저장 */
		if(!AddList(fl, repath)) {
			fprintf(stderr, "AddList failure for %s\n", repath);
		}
	}

	for (int i = 0; i < name_count; i++) {
		free(name_list[i]);
		name_list[i] = NULL;
	}

	free(name_list);
	name_list = NULL;
	
	return TRUE; // 성공시 TRUE(=1) 리턴
}

/* 두 Regular Files의 내용을 비교하는 함수 */
int compareFiles(char* fpath, char* ipath, int printDiff)
{
	FILE* ff, * fi; // ff : 기준 파일(index 0), fi : 대상 파일(입력받는 index)

	char bufferf[CHAR_MAX + 2] = {'\0', }, bufferi[CHAR_MAX + 2] = {'\0', }; // 문자 개수 1024를 넘는지 여부 판별을 위해 + '\0' -> CHAR_MAX + 2 
	char* tempf[CHAR_MAX]= {NULL, }, * tempi[CHAR_MAX] = {NULL, }; // 파일을 줄단위로 tempf, tempi 배열에 포인터로 저장
	char* diff[CHAR_MAX] = {NULL, }; // 출력되는 차이점을 줄단위로 저장
	char ch; 

	int strNumF = 0, strNumI = 0; // 각 파일의 line 수

	/* 파일 오픈 */
	ff = fopen(fpath, "r");
	if (ff == NULL) {
		fprintf(stderr, "Can't open file: %s\n", fpath);
		return -1;
	}

	fi = fopen(ipath, "r");
	if (fi == NULL) {
		fprintf(stderr, "Can't open file: %s\n", ipath);
		return -1;
	}

	/* 파일 모든 내용 버퍼로 이동 */
	fread(bufferf, sizeof(bufferf), 1, ff);
	fread(bufferi, sizeof(bufferi), 1, fi);

	/* offset 파일의 처음으로 */
	fseek(ff, 0L, SEEK_SET);
	fseek(fi, 0L, SEEK_SET);
	
	/* 글자 수 1023 초과한 경우 프롬포트로 이동 */
	if (strlen(bufferf) > CHAR_MAX) {
		fprintf(stderr, "More than 1023 characters in the file: %s\n", fpath);
		fclose(ff);
		fclose(fi);
		return -1;
	}
	else if (strlen(bufferi) > CHAR_MAX) {
		fprintf(stderr, "More than 1023 characters in the file: %s\n", ipath);
		fclose(ff);
		fclose(fi);
		return -1;
	}

	/* 내용 같은 경우 출력 없이 프롬포트로 이동 */
	if (iOp || wOp) { // i Option TRUE -> 대소문자 구분 X, w Option TRUE -> 공백 무시 
		if (op_strcmp(bufferf, bufferi) == 0) {
			fclose(ff);
			fclose(fi);
			return 1;
		}
	}
	else { // i, w Option FALSE
		if (strcmp(bufferf, bufferi) == 0) {
			fclose(ff);
			fclose(fi);
			return 1;
		} 
	}

	/* printDiff == TRUE 인 경우만 차이점 출력 */
	if (!printDiff) { // FALSE인 경우 출력하지 않고 compareFiles 함수 종료
		fclose(ff);
		fclose(fi);
		return 0;
	}

	/* 파일 내용 비교, 출력 */
	for (int i = 1; i <= CHAR_MAX + 1; i++) { // ff 파일 스트림 line단위로 -> tempf
		tempf[i] = (char*)malloc(CHAR_MAX + 1);
		if (tempf[i] == NULL) {
			fprintf(stderr, "malloc() error\n");
			exit(1);
		}
		if (fgets(tempf[i], CHAR_MAX, ff) == NULL)
			break;

		strNumF++; // strNumF : 기준 파일 line 수
	}

	for (int i = 1; i <= CHAR_MAX + 1; i++) { // fi 파일 스트림 line단위로 -> tempi
		tempi[i] = (char*)malloc(CHAR_MAX + 1);
		if (tempi[i] == NULL) {
			fprintf(stderr, "malloc() error\n");
			exit(1);
		}
		if (fgets(tempi[i], CHAR_MAX, fi) == NULL)
			break;
		
		strNumI++; // strNumI : 대상 파일 line 수
	}

	/* 각 파일의 마지막 line 길이 */
	int strEndLenF = strlen(tempf[strNumF]);
	int strEndLenI = strlen(tempi[strNumI]);

	/* 각 파일의 마지막 line이 개행문자 '\n' 이라면 각각 isNewELF, isNewELI는 TRUE */
	bool isNewELF = FALSE;
	if (tempf[strNumF][strEndLenF - 1] == '\n')
		isNewELF = TRUE;
	
	bool isNewELI = FALSE;
	if (tempi[strNumI][strEndLenI - 1] == '\n')
		isNewELI = TRUE;

	/* LCS 알고리즘 TABLE */
	int tableLCS[strNumI + 1][strNumF + 1];

	/* TABLE 초기화 (첫 행과 첫 열은 모두 0) */
	for (int i = 0; i <= strNumF; i++)
		tableLCS[0][i] = 0;

	for (int i = 1; i <= strNumI; i++)
		tableLCS[i][0] = 0;

	/* LCS TABLE 세팅 */
	for (int i = 1; i <= strNumI; i++) {
		for (int j = 1; j <= strNumF; j++) {
			if (iOp || wOp) { // i Option TRUE -> 대소문자 구분 X, w Option TRUE -> 공백 무시
				if (op_strcmp(tempf[j], tempi[i]) == 0) 
					tableLCS[i][j] = tableLCS[i - 1][j - 1] + 1;
				else
					tableLCS[i][j] = max(tableLCS[i - 1][j], tableLCS[i][j - 1]);
			} // 대소문자 구분 O, 공백 고려
			else {
				if (strcmp(tempf[j], tempi[i]) == 0)
					tableLCS[i][j] = tableLCS[i - 1][j - 1] + 1;
				else
					tableLCS[i][j] = max(tableLCS[i - 1][j], tableLCS[i][j - 1]);
			}
		}
	}

	/* diff 문자열 찾기 */
	int diffIdx = 0; // diff 배열 index

	bool isMoveF = FALSE; // TABLE에서 가로방향으로 이동했는가 여부
	bool isMoveI = FALSE; // TABLE에서 세로방향으로 이동했는가 여부

	int i = strNumI; // 세로방향 시작 index : 대상 파일 line 수 (맨 아래)
	int j = strNumF; // 가로방향 시작 index : 기준 파일 line 수 (맨 오른쪽)

	int sameCountF = 0; // TABLE에서 바로 왼쪽 자리와 값이 같을때마다 증가
	int sameCountI = 0; // TABLE에서 바로 위쪽 자리와 값이 같을때마다 증가

	while (i >= 1 || j >= 1)
	{
		if (iOp || wOp) { // i or w Option TRUE
			if (i >= 1 && j >= 1 && (op_strcmp(tempf[j], tempi[i]) == 0)) { // 두 문자열이 같으면 왼쪽, 위쪽으로 1칸씩 이동
				i--, j--;
				continue;
			}
		} // i, w 모두 FALSE
		else {
			if (i >= 1 && j >= 1 && (strcmp(tempf[j], tempi[i]) == 0)) { // 두 문자열이 같으면 왼쪽, 위쪽으로 1칸씩 이동
				i--, j--;
				continue;
			}
		}

		// TABLE[i][j]와 위쪽 값인 TABLE[i - 1][j]의 값이 같은 경우
		while (i >= 1 && (tableLCS[i][j] == tableLCS[i - 1][j])) {
			isMoveI = TRUE; // 이동했으므로 TRUE

			if ((i == strNumI) && !isNewELI) { // i가 TABLE 맨 아래 자리이면서 대상 파일의 마지막 line에 개행문자가 없을 경우
				diff[diffIdx] = (char*)malloc(sizeof(char) * 30); // 30은 "\n\\ No newline at end of file\n" 의 길이
				if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

				strcpy(diff[diffIdx++], "\n\\ No newline at end of file\n"); // 문자열 대입 후 diffIdx++
			}
				
			diff[diffIdx] = (char*)malloc(sizeof(char) * (CHAR_MAX + 1));
			if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

			sprintf(diff[diffIdx++], "> %s", tempi[i]); // 위쪽값과 같다면 그 index의 tempi 문자열은 > 방향 diff

			sameCountI++;

			i--; // 위로 한 칸 이동
		}

		// isMoveI == TRUE 이면서 위 방향으로 마지막으로 움직인 그 자리에서 왼쪽값과 같다면 두 파일 내용이 "교체"된 문자열들임
		if (isMoveI && ( j >= 1 && (tableLCS[i][j] == tableLCS[i][j - 1]))) {
				
			diff[diffIdx] = (char*)malloc(sizeof(char) * 5); // 5는 "---\n"의 길이
			if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

			strcpy(diff[diffIdx++], "---\n"); // "---"
		}

		// 왼쪽값과 같은 경우
		while (j >= 1 && (tableLCS[i][j] == tableLCS[i][j - 1])) {
			isMoveF = TRUE; // 이동했으므로 TRUE

			if ((j == strNumF) && !isNewELF) { // j의 값이 가장 오른쪽 자리이면서 기준 파일의 마지막 line에 개행문자가 없는 경우
				diff[diffIdx] = (char*)malloc(sizeof(char) * 30);
				if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

				strcpy(diff[diffIdx++], "\n\\ No newline at end of file\n");
			}

			diff[diffIdx] = (char*)malloc(sizeof(char) * (CHAR_MAX + 1));
			if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

			sprintf(diff[diffIdx++], "< %s", tempf[j]); // < 방향 diff

			sameCountF++;

			j--; // 왼쪽 한칸 이동
		}

		diff[diffIdx] = (char*)malloc(sizeof(char) * (CHAR_MAX + 1));
		if (diff[diffIdx] == NULL) {
					fprintf(stderr, "malloc() error\n");
					exit(1);
				}

		// 왼쪽, 위 방향 모두 이동 -> 교체
		if (isMoveF && isMoveI) {
			if (sameCountF == 1 && sameCountI == 1) { // 두 이동 모두 한 번만 실행되었을 경우
				sprintf(diff[diffIdx++], "%dc%d\n", j + 1, i + 1); // 각 한개의 line만 교체
			}
			else if (sameCountF == 1) { // 왼쪽 방향만 한 번 이동
				sprintf(diff[diffIdx++], "%dc%d,%d\n", j + 1, i + 1, i + sameCountI); // 기준 파일 한개의 line이 대상 파일 여러 line과 교체
			}
			else if (sameCountI == 1) { // 위쪽 방향만 한 번 이동
				sprintf(diff[diffIdx++], "%d,%dc%d\n", j + 1, j + sameCountF, i + 1); // 기준 파일 여러개의 line이 대상 파일 한개의 line과 교체
			}
			else { // 두 방향 모두 두 번 이상 이동
				sprintf(diff[diffIdx++], "%d,%dc%d,%d\n", j + 1, j + sameCountF, i + 1, i + sameCountI); // 여러 line들 끼리 교체
			}
		}
		// 왼쪽 방향만 이동 -> 삭제
		else if (isMoveF) {
			if (sameCountF == 1) { // 한 번만 이동
				sprintf(diff[diffIdx++], "%dd%d\n", j + 1, i);
			}
			else { // 여러번 이동
				sprintf(diff[diffIdx++], "%d,%dd%d\n", j + 1, j + sameCountF, i);
			}
		}
		// 위 방향만 이동 -> 추가
		else { // isMoveI == TRUE
			if (sameCountI == 1) { // 한 번만 이동
				sprintf(diff[diffIdx++], "%da%d\n", j, i + 1);
			}
			else { // 여러번 이동
				sprintf(diff[diffIdx++], "%da%d,%d\n", j, i + 1, i + sameCountI);
			}
		}

		sameCountF = 0, sameCountI = 0; // sameCount (이동 횟수) 초기화
		isMoveF = FALSE, isMoveI = FALSE; // 이동 여부 초기화
	}

	int strlen = 0; // diff 출력될 line 개수

	/* diff 출력될 line 개수 계산 */
	for (int i = 0; i < CHAR_MAX; i++) {
		if (diff[i] == NULL)
			break;
		strlen++;
	}

	/* diff 배열에 저장된 문자열들을 역순으로 출력 */
	for (int i = strlen - 1; i >= 0; i--) {
		printf("%s", diff[i]);
	}

	/* malloc free */
	for (int i = 0; i < CHAR_MAX; i++) {
		if (diff[i] == NULL)
			break;
		free(diff[i]);
		diff[i] = NULL;
	}

	for (int i = 1; i <= CHAR_MAX + 1; i++) {
		if (tempf[i] == NULL)
			break;
		free(tempf[i]);
		tempf[i] = NULL;
	}
	for (int i = 1; i < CHAR_MAX + 1; i++) {
		if (tempi[i] == NULL)
			break;
		free(tempi[i]);
		tempi[i] = NULL;
	}

	// 차이점 출력과정 종료

	fclose(ff);
	fclose(fi);

	return 0;
}

/* fileList 초기화 */
void ListInit(fileList* fl)
{
	fl -> numOfData = 0;
}

/* directory의 size를 계산하는 함수 */
int sizeOfDirectory(char* path)
{
	int name_count;
	int size = 0; // size 초기화
	struct dirent** name_list = NULL;

	char repath[PATH_MAX];
	Stat nstat;

	name_count = scandir(path, &name_list, file_select, alphasort);
	
	if (name_count < 0) {
		fprintf(stderr, "scandir() fuction Error for %s\n", path);
		exit(1);
	}
	else if (name_count == 0) {
		return 0; // 파일이 없으면 size는 0
	}

	for (int i = 0; i < name_count; i++) {

		sprintf(repath, "%s/%s", path, name_list[i]->d_name);
		stat(repath, &nstat);
		
		if (S_ISDIR(nstat.st_mode)) { // Directory -> 재귀 호출
			size += sizeOfDirectory(repath); 
		}
		else // Regular
			size += nstat.st_size;
	}

	return size; // size 리턴
}

/* fileList Array에 파일을 추가하는 함수 */
int AddList(fileList* fl, char* path)
{
	if (fl->numOfData >= ARR_LEN) {
		fprintf(stderr, "AddList failure\n");
		return FALSE; // 최대 저장가능 개수(1024)를 넘어서면 실패 
	}

	fl->arr[fl->numOfData] = (FData*)malloc(sizeof(FData));
	if (fl->arr[fl->numOfData] == NULL) {
		fprintf(stderr, "malloc() error\n");
		exit(1);
	}

	if (stat(path, &(fl->arr[fl->numOfData]->stif)) != 0)  {
		fprintf(stderr, "Get file information Failed for %s\n", path);
		return FALSE; // 실패 : FALSE(0) 반환
	}

	if (S_ISDIR(fl->arr[fl->numOfData]->stif.st_mode)) { // Directory인 경우 sizeOfDirectory함수 호출하여 size를 계산
		int size = sizeOfDirectory(path);
		fl->arr[fl->numOfData]->stif.st_size = size;
	}

	strcpy(fl->arr[fl->numOfData]->path, path);

   	(fl -> numOfData)++; // 개수 + 1
	return TRUE; // 성공 : TRUE(1) 반환
}

/* Array를 PATH의 ASCII기준으로 정렬 */
void StatSorting(FData* arr[], int num)
{
	int i, j;
	int min;

	for (i = 1; i < num - 1; i++)
	{
		min = i;
		for (j = i + 1; j < num; j++)
		{
			if(strcmp(arr[min]->path, arr[j]->path) > 0) {
				min = j;
			}
		}
		FData* temp;
		temp = arr[i];
		arr[i] = arr[min];
		arr[min] = temp;
	}
}

/* 프로그램 종료 또는 프롬프트 재시작할 때 Array에 동적할당 된 메모리를 정리하고 fileList를 초기화하는 함수 */
void ListClean(fileList* fl)
{
	for (int i = 0; i < fl->numOfData; i++) {
		free((fl->arr)[i]);
		fl->arr[i] = NULL;
	}

	fl->numOfData = 0;
}

/* st_mode 포맷 변경 */
char* modeFormat(mode_t st_mode)
{
	static char fmode[15];

	sprintf(fmode, "%c%c%c%c%c%c%c%c%c%c",
			((S_ISDIR(st_mode)) ? 'd' : '-'),
			((st_mode & S_IRUSR) ? 'r' : '-'),
			((st_mode & S_IWUSR) ? 'w' : '-'),
			((st_mode & S_IXUSR) ? 'x' : '-'),
			((st_mode & S_IRGRP) ? 'r' : '-'),
			((st_mode & S_IWGRP) ? 'w' : '-'),
			((st_mode & S_IXGRP) ? 'x' : '-'),
			((st_mode & S_IROTH) ? 'r' : '-'),
			((st_mode & S_IWOTH) ? 'w' : '-'),
			((st_mode & S_IXOTH) ? 'x' : '-'));

	return fmode;
}

/* 탐색된 파일 목록을 출력 */
void printStat(fileList* fl)
{
	char atime[20] = {'\0', };
	char ctime[20] = {'\0', }; 
	char mtime[20] = {'\0', };
	struct tm timeinfo;

	puts("Index  Size	Mode         Blocks Links UID  GID  Access	    Change	    Modify          Path");

	for (int i = 0; i < fl->numOfData; i++) {

		Stat* sb = &((fl->arr)[i]->stif); // 출력될 파일 stat정보
		char* path = fl->arr[i]->path; // 출력될 파일 path
		
		// time 포맷 변형
		strftime(atime, 20, "%y-%m-%d %H:%M", localtime_r(&sb->st_atime, &timeinfo));
		strftime(ctime, 20, "%y-%m-%d %H:%M", localtime_r(&sb->st_ctime, &timeinfo));
		strftime(mtime, 20, "%y-%m-%d %H:%M", localtime_r(&sb->st_mtime, &timeinfo));
		
		printf("%-4d   %lld	%s   %lld      %ld     %-4ld %-4ld %s  %s  %s  %s\n",
				i, (long long)sb->st_size, modeFormat(sb->st_mode), (long long)sb->st_blocks, (long)sb->st_nlink, (long)sb->st_uid, (long)sb->st_gid, 
				atime, ctime, mtime, path);
	}
}

/* help 함수 */
void help (void)
{
    puts("Usage:");
	puts("  > find [FILENAME] [PATH]");
	puts("     >> [INDEX] [OPTION ... ]");
	puts("  > help");
	puts("  > exit");
	puts("");
	puts("[OPTION ... ]");
	puts(" q : report only when files differ");
	puts(" s : report when two files are the same");
	puts(" i : ignore case differences in file contents");
	puts(" r : recursively compare any subdirectories found");
	puts(" v : compare with index 0 based on the selected index");
	puts(" c : no returning to the prompt, re-print filelist, re-input index and option");
	puts(" e : end of file comparison and end of prompt");
	puts(" w : ignore whitespace differences in file contents");
}