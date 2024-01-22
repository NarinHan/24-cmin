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

char *crash ;
char *error_message ;
char *output_filename ;
char *target_filepath[10] ;

char tm[MAX] ;
size_t tm_len ;

void 
sigalrm_handler(int signum) 
{
    printf("SIGALRM received...\n") ;
    kill(child, SIGKILL) ;
}

void
parse_option(int argc, char *argv[])
{
    int opt ;
    while ((opt = getopt(argc, argv, "i:m:o:")) != -1) {
        switch (opt) {
            case 'i':
                crash = optarg ;
                break ;
            case 'm':
                error_message = optarg ;
                break ;
            case 'o':
                output_filename = optarg ;
                break ;
            case '?':
                if (optopt == 'i' || optopt == 'm' || optopt == 'o') {
                    fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                } else {
                    fprintf(stderr, "Unknown option: -%c\n", optopt);
                }
                exit(EXIT_FAILURE) ;
        }
    }
    
    // Check if there is an additional argument (target file path)
    if (optind < argc) {
        char *temp = argv[optind] ;
        char *token = strtok(temp, " ") ;
        int i = 0 ;
        while (token != NULL && i < 9) {
            target_filepath[i++] = token ;
            token = strtok(NULL, " ") ;
        }
        target_filepath[i] = NULL ;
    } else {
        fprintf(stderr, "Missing target file path...\n") ; 
        exit(EXIT_FAILURE) ;
    }

    if (crash == NULL || error_message == NULL || output_filename == NULL || target_filepath == NULL) {
        fprintf(stderr, "Missing required options...\n") ;
        exit(EXIT_FAILURE) ;
    }

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
    execv(target_filepath[0], target_filepath) ;
    perror("executing target program error : ") ; // execl only returns on error
    exit(EXIT_FAILURE) ;
}

int
parent_proc(char *tosend, size_t tosend_len)
{
    // send data
    close(ptoc[0]) ; // close unused read end
    size_t len, write_len = 0 ;
    while ((len = write(ptoc[1], tosend + write_len, tosend_len - write_len)) > 0) {
        write_len += len ;
    }
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
        alarm(0) ; // turn off the alarm
    }

    // receive standard error
    close(ctop[1]) ; // close unused write end
    
    char buf[MAX] ;
    size_t read_len = 0 ;
    while ((len = read(ctop[0], buf + read_len, sizeof(buf) - read_len)) > 0) {
        read_len += len ; 
        printf("received stderr size %ld...\n", read_len) ;
        // write(STDOUT_FILENO, buf, len) ;   
    }
    buf[read_len] = '\0';
    close(ctop[0]) ;

    // compare error messages
    if (strstr(buf, error_message) != NULL) {
        return 1 ; // indicate that error message is found
    }

    return 0 ;
}

char *
reduce(char *input, size_t input_len) 
{
    memcpy(tm, input, input_len) ;
    tm_len = input_len ;
    tm[tm_len] = '\0' ;

    fprintf(stderr, "tm : %s\n", tm) ;

    size_t s = tm_len - 1 ;
    while (s > 0) {
        for (int i = 0; i <= tm_len - s; i++) {
            size_t head_len = i - 1 + 1 ; 
            char *head = (char *)malloc(head_len + 1) ;
            printf("head length : %ld\n", head_len) ;
            memcpy(head, tm, head_len) ;
            head[head_len] = '\0' ;
            
            size_t tail_len = tm_len - 1 - i - s + 1 ;
            char *tail = (char *)malloc(tail_len + 1) ;
            printf("tail length : %ld\n", tail_len) ; 
            memcpy(tail, tm + i + s, tail_len) ;
            tail[tail_len] = '\0' ;
           
            printf("head : %s\n", head) ;
            printf("tail : %s\n", tail) ;
            
            size_t headtail_len = head_len + tail_len ;
            char *headtail = (char *)malloc(headtail_len + 1) ;
            memcpy(headtail, head, head_len) ;
            memcpy(headtail + head_len, tail, tail_len) ;
            headtail[headtail_len] = '\0' ;

            printf("head + tail : %s\n", headtail) ;
            
            free(head) ;
            free(tail) ;

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
                int ret = parent_proc(headtail, headtail_len) ;
                if (ret == 1) {
                    return reduce(headtail, headtail_len) ;
                }
            }

            free(headtail) ;
        }

        for (int i = 0; i <= tm_len - s; i++) {
            size_t mid_len = i + s - 1 - i + 1 ;
            char *mid = (char *)malloc(mid_len + 1) ;
            printf("mid length : %ld\n", mid_len) ;
            memcpy(mid, tm + i, mid_len) ;
            mid[mid_len] = '\0' ;
            printf("mid : %s\n", mid) ;

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
                int ret = parent_proc(mid, mid_len) ;
                if (ret == 1) {
                    return reduce(mid, mid_len) ;
                }
            }

            free(mid) ;
        }
        s-- ;
        printf("s: %ld\n", s) ; 
    }

    return tm ;
}

int
main(int argc, char *argv[]) 
{
    if (argc != 8) {
        fprintf(stderr, "%s -i [file path of the crashing input] -m [standard error message string] -o [file path to store the result] [target file path]\n", argv[0]) ;
    }

    parse_option(argc, argv) ;

    // read crash file
    FILE *fp = fopen(crash, "rb") ;
    if (fp == NULL) {
        perror("opening crash file error : ") ;
        exit(EXIT_FAILURE) ;
    }

    char buf[MAX] ; 
    size_t len, read_len = 0 ;
    while ((len = fread(buf + read_len, 1, sizeof(buf) - read_len, fp)) > 0) {
        read_len += len ;
    }
    buf[read_len] = '\0' ;

    fclose(fp) ;

    // 결과 받기 
    char *reduced = reduce(buf, read_len) ;
    reduced[strlen(reduced)] = '\0' ;
    printf("reduced input : %s\n", reduced) ;

    fp = fopen(output_filename, "wb") ;
    size_t write_len = 0 ;
    while ((len = fwrite(reduced + write_len, 1, strlen(reduced) - write_len, fp)) > 0) {
        write_len += len ;
    }

    fclose(fp) ;

    return 0 ;
}