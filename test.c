#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

// fork, pipe, dup2, exec 연습
// parent -> child : pipe 로 받은 data 를 dup2 로 stdin 으로 전달 
// exec 로 target 돌리기 
// child -> parent : dup2 로 stderr 를 pipe 로 전달 
// 타이머 세팅 : target 이 3초 이상 실행되면 프로세스 죽이기 

int ptoc[2] ;
int ctop[2] ;
// char *crash = "testsrc/jsmn/testcases/crash.json" ;
// char *message = "AddressSanitizer: heap-buffer-overflow" ;
// char *output_filename = "reduced" ;
// char *target_filepath = "testsrc/jsmn/jsondump" ;

char *crash = "testsrc/balance/testcases/fail" ;
char *target_filepath = "testsrc/balance/balance" ;

void 
sigalrm_handler(int signum) 
{
    // This function does nothing. It's only for catching the signal.
}

void
child_proc()
{
    // receive data that is going to be given as a standard input
    close(ptoc[1]) ; // close unused write end
    dup2(ptoc[0], STDIN_FILENO) ; // redirect stdin to the read end
    
    // send standard error
    close(ctop[0]) ; // close unused read end
    dup2(STDERR_FILENO, ctop[1]) ; // redirect stdout to the write end

    // char recv[4096] ;
    // int sz ;
    // while ((sz = read(ptoc[0], &recv, sizeof(recv))) > 0) {
    //     write(STDOUT_FILENO, &recv, sz) ;
    // }
    // close(ptoc[0]) ;

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
    
    int fd = open(crash, O_RDONLY) ;
    if (fd == -1) {
        perror("opening crash file error : ") ;
        exit(EXIT_FAILURE) ;
    }
    
    while ((sz = read(fd, buf + read_sz, sizeof(buf))) > 0) {
        read_sz += sz ;
    }
    buf[read_sz] = '\0' ;

    pid_t child = fork() ;
    if (child == -1) {
        perror("fork error : ") ;
        exit(EXIT_FAILURE) ;
    }

    if (child == 0) { // child process
        child_proc() ;
    } else { // parent process
        // send data
        close(ptoc[0]) ; // close unused read end
        write(ptoc[1], buf, strlen(buf)) ;
        close(ptoc[1]) ;

        signal(SIGALRM, sigalrm_handler) ; // set up a signal handler for SIGALRM
        alarm(3) ; // set 3 sec alarm

        // wait for the child process to finish
        int status ;
        if (waitpid(child, &status, 0) == -1) {
            perror("waitpid error : ") ;
            exit(EXIT_FAILURE) ;
        }

        // if the child process was terminated by SIGALRM, kill it
        if (WIFSIGNALED(status) && WTERMSIG(status) == SIGALRM) {
            kill(child, SIGKILL) ;
            printf("Child process was killed because it executed for more than 3 seconds.\n") ;
        }

        // receive standard error
        close(ctop[1]) ; // close unused write end
        memset(buf, sizeof(buf), 0) ;
        while ((sz = read(ctop[0], buf, sizeof(buf)) > 0)) {
            write(STDOUT_FILENO, buf, sz) ;
        }
        close(ctop[0]) ;
    }

    close(fd) ;

    return 0 ;
}