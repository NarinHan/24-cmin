#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

// fork, pipe, dup2, exec 연습
// parent -> child : pipe 로 받은 data 를 dup2 로 stdin 으로 전달 
// exec 로 target 돌리기 
// child -> parent : dup2 로 stderr 를 pipe 로 전달 
// 타이머 세팅 : target 이 3초 이상 실행되면 프로세스 죽이기 

int ptoc[2] ;
int ctop[2] ;

pid_t child ;

char *message = "AddressSanitizer: heap-buffer-overflow" ;
char *output_filename = "reduced" ;

// char *crash = "testsrc/jsmn/testcases/crash.json" ;
// char *target_filepath = "testsrc/jsmn/jsondump" ;

// char *crash = "testsrc/balance/testcases/fail" ;
// char *target_filepath = "testsrc/balance/balance" ;

char *crash = "../crash.png" ;
char *target_filepath = "./test_pngfix" ;

void 
sigalrm_handler(int signum) 
{
    // This function does nothing. It's only for catching the signal.
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

int
main(int argc, char *argv[])
{
    // pipe
    if (pipe(ptoc) == -1 || pipe(ctop) == -1) {
        perror("pipe error : ") ;
        exit(EXIT_FAILURE) ;
    }

    char buf[4096] ;
    int sz, read_sz = 0 ;
    
    // int fd = open(crash, O_RDONLY) ;
    FILE *fp = fopen(crash, "rb") ;
    if (fp == NULL) {
        perror("opening crash file error : ") ;
        exit(EXIT_FAILURE) ;
    }
    
    while ((sz = fread(buf + read_sz, 1, sizeof(buf), fp)) > 0) {
        read_sz += sz ;
    }
    buf[read_sz] = '\0' ;

    if ((child = fork()) == -1) {
        perror("fork error : ") ;
        exit(EXIT_FAILURE) ;
    }

    if (child == 0) { // child process
        child_proc() ;
    } else { // parent process
        // send data
        close(ptoc[0]) ; // close unused read end
        write(ptoc[1], buf, read_sz) ;
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

        // receive standard error
        close(ctop[1]) ; // close unused write end
        memset(buf, sizeof(buf), 0) ;
        read_sz = 0 ;
        while ((sz = read(ctop[0], buf, sizeof(buf))) > 0) {
            read_sz += sz ;
            printf("received stderr size %d....\n", read_sz) ;
            write(STDOUT_FILENO, buf, sz) ;
        }
        close(ctop[0]) ;

        char *comp = strstr(buf, message) ;

        if (comp) {
            printf("message matching...\n") ;
        }
    }

    fclose(fp) ;

    return 0 ;
}
