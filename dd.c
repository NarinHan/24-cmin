#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

#define MAX 4096

int ptoc[2] ;
int ctop[2] ;

pid_t child ;

char *message = "AddressSanitizer: heap-buffer-overflow" ;
char *output_filename = "reduced" ;

char *crash = "testsrc/jsmn/testcases/crash.json" ;
char *target_filepath = "testsrc/jsmn/jsondump" ;

// char *crash = "testsrc/balance/testcases/fail" ;
// char *target_filepath = "testsrc/balance/balance" ;

// char *crash = "testsrc/libpng/crash.png" ;
// char *target_filepath = "testsrc/libpng/libpng/test_pngfix" ;

void 
sigalrm_handler(int signum) 
{
    printf("SIGALRM received...\n") ;
    kill(child, SIGKILL) ;
}

void
child_proc()
{
    // receive data that is going to be given as a standard input
    close(ptoc[1]) ; // close unused write end
    dup2(ptoc[0], STDIN_FILENO) ; // redirect stdin to the read end
    
    // send standard error
    close(ctop[0]) ; // close unused read end
    dup2(ctop[1], STDERR_FILENO) ; // redirect stderr to the write end

    // execute the target program
    execl(target_filepath, target_filepath, NULL) ;
    perror("executing target program error : ") ; // execl only returns on error
    exit(EXIT_FAILURE) ;
}

void 
send_data(char *buf, size_t read_len)
{
    // send data
    close(ptoc[0]) ; // close unused read end
    write(ptoc[1], buf, read_len) ;
    close(ptoc[1]) ;

    // timer
    signal(SIGALRM, sigalrm_handler) ; // set up a signal handler for SIGALRM
    if (alarm(3) != 0) { // set 3 sec alarm
        perror("setting alarm error : ") ;
        exit(EXIT_FAILURE) ;
    }

    // wait for the child process to finish
    int status ;
    if (waitpid(child, &status, 0) == -1) {
        perror("waitpid error : ") ;
        exit(EXIT_FAILURE) ;
    } 

    // if the child process was terminated by SIGALRM, kill it
    if (WIFSIGNALED(status)) { 
        printf("Child process was killed because it executed for more than 3 seconds...\n") ;
    } else {
        alarm(0) ;
    }
}

void
compare_error()
{
    char buf[MAX] ;

    // receive standard error
    close(ctop[1]) ; // close unused write end
    memset(buf, sizeof(buf), 0) ;
    size_t len, read_len = 0 ;
    while ((len = read(ctop[0], buf, sizeof(buf))) > 0) {
        read_len += len ;
        write(STDOUT_FILENO, buf, len) ;
    }
    close(ctop[0]) ;


    printf("compare error function finished\n") ;
}

char *
delta_debugging(char *input, size_t input_len) 
{
    char tm[MAX] ;
    size_t tm_len ;

    memcpy(tm, input, input_len) ;
    tm_len = input_len ;
    tm[tm_len] = '\0' ;

    printf("%s", tm) ;

    size_t s = tm_len - 1 ;
    while (s > 0) {
        for (int i = 0; i <= tm_len - s; i++) {
            char head[MAX], tail[MAX] ;
            size_t head_len, tail_len ;
            
            head_len = i - 1 + 1 ; // + 1 for \0
            printf("head length : %ld\n", head_len) ;
            memcpy(head, tm, head_len) ;
            head[head_len] = '\0' ;
            
            tail_len = tm_len - 1 - i - s + 1 ;
            printf("tail length : %ld\n", tail_len) ; 
            memcpy(tail, tm + i + s - 1, tail_len) ;
            tail[tail_len] = '\0' ;
           
            printf("head : %s\n", head) ;
            printf("tail : %s\n", tail) ;
            
            char headtail[MAX] ;
            size_t headtail_len ;

            memcpy(headtail, head, head_len) ;
            memcpy(headtail + head_len, tail, tail_len) ;
            headtail_len = head_len + tail_len ;

            printf("head+tail : %s\n", headtail) ;

            // 만든 걸 stdin 으로 보내줘야 함
            // stderr 로 받은 것 & 에러 메시지 비교 -> crash 확인
            // 최종적으로 나온 결과물 저장 

            // pipe
            if (pipe(ptoc) == -1 || pipe(ctop) == -1) {
                perror("pipe error : ") ;
                exit(EXIT_FAILURE) ;
            }

            if ((child = fork()) == -1) {
                perror("fork error : ") ;
                exit(EXIT_FAILURE) ;
            }

            if (child == 0) { // child process
                child_proc() ;
            } else { // parent process
                send_data(headtail, headtail_len) ;
                compare_error() ;
            }

        }

        for (int i = 0; i <= tm_len - s; i++) {
            char mid[MAX] ;
            size_t mid_len ;

            mid_len = i + s - 1 - i + 1 ;
            printf("mid length : %ld\n", mid_len) ;
            memcpy(mid, tm + i, mid_len) ;
            mid[mid_len] = '\0' ;
            printf("mid : %s\n", mid) ;
        }
        
        break ;
        s-- ;
    }

    return tm ;
}

int
main(int argc, char *argv[])
{
    FILE *fp = fopen(crash, "rb") ;
    if (fp == NULL) {
        perror("opening crash file error : ") ;
        exit(EXIT_FAILURE) ;
    }

    char buf[MAX] ;
    size_t len, read_len = 0 ;

    while (!feof(fp)) {
        read_len += fread(buf + read_len, 1, sizeof(buf), fp) ;
    }
    buf[read_len] = '\0' ;

    delta_debugging(buf, read_len) ;

    fclose(fp) ;

    return 0 ;
}