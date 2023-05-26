#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define BUFSIZE 1024

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

void prompt(void);

int main(void)
{
	prompt(); // 프롬프트
	exit(0);
}

void prompt(void)
{
	char input[BUFSIZE];
	char real_target_directory[PATH_MAX]; // 절대경로
	char *ptr;
	char *homedir = getenv("HOME");
	char *inst, *file_extension, *minsize, *maxsize, *target_directory; // 각 입력받는 인자들
	char minstr[30], maxstr[30];
	double min, max;
	int i;
	pid_t pid;
	struct stat targetdir;

	while (1) {
		printf("20181321> ");
		fgets(input, BUFSIZE, stdin); // 명령어 입력
		
		if (input[0] == '\n') // 입력X
			continue;
		
		input[strlen(input) - 1] = '\0'; // '\n' -> '\0'
		inst = strtok(input, " "); // 첫번째 인자 -> inst

		if (strcmp(inst, "exit") == 0) { // inst가 "exit" -> 프롬프트 종료
			puts("Prompt End");
			return;
		}
		else if (strcmp(inst, "fmd5") && strcmp(inst, "fsha1")) { // "fmd5", "fsha1" 둘다X -> help
			if ((pid = fork()) < 0) { // 자식 프로세스 생성
				fprintf(stderr, "fork error\n");
				exit(1);
			}
			if (pid == 0) { // 자식 프로세스 help 실행파일 실행
				execl("./ssu_help", "./ssu_help", NULL);
				fprintf(stderr, "exec error\n");
				exit(1);
			}
			else {
				if (wait(NULL) < 0) { // 자식 프로세스 소멸까지 wait
					fprintf(stderr, "wait error\n");
					exit(1);
				}
				continue; // "학번> " 입력 대기
			}
		}
		/* check input */
		if ((file_extension = strtok(NULL, " ")) == NULL) { // file_extension 입력 X ?
			fprintf(stderr, "file_extension error\n");
			continue; // "학번> " 입력 대기
		}
		if ((minsize = strtok(NULL, " ")) == NULL) { // minsize 입력 X ?
			fprintf(stderr, "minsize error\n");
			continue; // "학번> " 입력 대기
		}
		if ((maxsize = strtok(NULL, " ")) == NULL) { // maxsize 입력 X ?
			fprintf(stderr, "maxsize error\n");
			continue; // "학번> " 입력 대기
		}
		if ((target_directory = strtok(NULL, " ")) == NULL) { // target_directory 입력 X ?
			fprintf(stderr, "target_directory error\n");
			continue; // "학번> " 입력 대기
		}
		if (strtok(NULL, " ") != NULL) { // 불필요한 인자가 입력되었는가?
			fprintf(stderr, "input error\n");
			continue; // "학번> " 입력 대기
		}
		/* check file_extension */
		if (file_extension[0] != '*') { // 파일 확장자 입력은 '*'로 시작해야함
			fprintf(stderr, "file_extension error\n");
			continue;
		}
		else if (file_extension[1] != '\0' && file_extension[1] != '.') { // '*' 이거나 '*.(확장자)'만 가능
			fprintf(stderr, "file_extension error\n");
			continue;
		}
		if (file_extension[1] == '.') { // '*.' 다음에는 확장자가 반드시 입력되어야함
			if (file_extension[2] == '\0') {
				fprintf(stderr, "file_extension error\n");
				continue;
			}
			else if (strchr(&file_extension[2], '.') != NULL) { // 확장자에 '.'가 있으면 안됨
				fprintf(stderr, "file_extension error\n");
				continue;
			}
		}
		/* check minsize */
		if ((strcmp(minsize, "~") != 0) && !isdigit(minsize[0])) { // '~' 이거나 숫자로 시작해야함
			fprintf(stderr, "minsize error\n");                    // '-' 음수도 허용X
			continue;
		}

		if (strcmp(minsize, "~") == 0) // minsize가 '~'-> 0
			strcpy(minstr, "0");
		else {
			for (i = 0; minsize[i] != '\0'; i++) // 소수점과 숫자를 제외한 문자(단위)의 포인터를 찾는다.
				if (!isdigit(minsize[i]) && (minsize[i] != '.')) {
					ptr = &minsize[i];
					break;
				}
			
			/* 단위에 따른 소수점 아래 처리 */
			if (minsize[i] == '\0') { // 단위가 없다면
				min = atof(minsize); // 문자 -> double형 숫자 변환
				sprintf(minstr, "%.0f", min); // 소수점 아래 모두 절삭 후 문자로 변환
			}
			else if (strcasecmp(ptr, "B") == 0) { // 단위가 B -> 소수점 아래 모두 절삭
				min = atof(minsize);
				sprintf(minstr, "%.0f", min);
			}
			else if (strcasecmp(ptr, "KB") == 0) { // 단위가 KB -> 소수점 아래 3자리까지 표현
				min = atof(minsize) * 1024.0;
				sprintf(minstr, "%.3f", min);
			}
			else if (strcasecmp(ptr, "MB") == 0) { // 단위가 MB -> 소수점 아래 6자리까지 표현
				min = atof(minsize) * 1024.0 * 1024.0;
				sprintf(minstr, "%.6f", min);
			}
			else if (strcasecmp(ptr, "GB") == 0) { // 단위가 GB -> 소수점 아래 9자리까지 표현
				min = atof(minsize) * 1024.0 * 1024.0 * 1024.0;
				sprintf(minstr, "%.9f", min);
			}
			else { // 그 외의 문자는 에러처리
				fprintf(stderr, "minsize error\n");
				continue;
			}
		}
		/* check maxsize -> min size와 동일 */
		if ((strcmp(maxsize, "~") != 0) && !isdigit(maxsize[0])) { // '~' 이거나 숫자로 시작해야함
			fprintf(stderr, "maxsize error\n");                    // '-' 음수도 허용X
			continue;
		}

		if (strcmp(maxsize, "~") == 0) // maxsize가 '~'
			strcpy(maxstr, "~");
		else {
			for (i = 0; maxsize[i] != '\0'; i++) // 소수점과 숫자를 제외한 문자(단위)의 포인터를 찾는다.
				if (!isdigit(maxsize[i]) && (maxsize[i] != '.')) {
					ptr = &maxsize[i];
					break;
				}

			/* 단위에 따른 소수점 아래 처리 */
			if (maxsize[i] == '\0') { // 단위가 없다면
				max = atof(maxsize);
				sprintf(maxstr, "%.0f", max); // 소수점 아래 모두 절삭
			}
			else if  (strcasecmp(ptr, "B") == 0) { // 단위가 B -> 소수점 아래 모두 절삭
				max = atof(maxsize);
				sprintf(maxstr, "%.0f", max);
			}
			else if  (strcasecmp(ptr, "KB") == 0) { // 단위가 KB -> 소수점 아래 3자리까지 표현
				max = atof(maxsize) * 1024.0;
				sprintf(maxstr, "%.3f", max);
			}
			else if  (strcasecmp(ptr, "MB") == 0) { // 단위가 MB -> 소수점 아래 6자리까지 표현
				max = atof(maxsize) * 1024.0 * 1024.0;
				sprintf(maxstr, "%.6f", max);
			}
			else if  (strcasecmp(ptr, "GB") == 0) { // 단위가 GB -> 소수점 아래 9자리까지 표현
				max = atof(maxsize) * 1024.0 * 1024.0 * 1024.0;
				sprintf(maxstr, "%.9f", max);
			}
			else { // 그 외의 문자는 에러처리
				fprintf(stderr, "maxsize error\n");
				continue;
			}
		}

		/* check target_directory */
		if (target_directory[0] == '~') // '~' 을 홈 디렉토리로 변환
			sprintf(real_target_directory, "%s%s", homedir, target_directory + 1);
		else if (realpath(target_directory, real_target_directory) == NULL) { // 절대경로로 변환
			fprintf(stderr, "realpath error\n");
			continue;
		}

		if (stat(real_target_directory, &targetdir) < 0) { // stat 함수
			fprintf(stderr, "stat error\n");
			continue;
		}
		if (!S_ISDIR(targetdir.st_mode)) { // 타겟은 디렉토리만 가능
			fprintf(stderr, "not directory\n");
			continue;
		}
		
		if ((pid = fork()) < 0) { // 자식 프로세스 생성
			fprintf(stderr, "fork error\n");
			exit(1);
		}
		if (pid == 0) { // 자식 프로세스 fmd5 실행
			if (strcmp(inst, "fmd5") == 0)
				execl("./ssu_find-md5", "./ssu_find-md5", file_extension, minstr, maxstr, real_target_directory, NULL);
			else // 자식 프로세스 fsha1 실행
				execl("./ssu_find-sha1", "./ssu_find-sha1", file_extension, minstr, maxstr, real_target_directory, NULL);
			fprintf(stderr, "excel error\n");
			exit(1);
		}
		else {
			if (wait(NULL) < 0) { // 부모 프로세스 대기
				fprintf(stderr, "wait error\n");
				exit(1);
			}
		}
	}
}