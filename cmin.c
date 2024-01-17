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
char *target_filepath ;

char buf[MAX] ;
size_t read_len ;

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
        target_filepath = argv[optind] ;
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

    close(STDOUT_FILENO) ;

    // execute the target program
    execl(target_filepath, target_filepath, NULL) ;
    perror("executing target program error : ") ; // execl only returns on error
    exit(EXIT_FAILURE) ;
}

int
parent_proc(char *tosend, size_t tosend_len)
{
    // send data
    close(ptoc[0]) ; // close unused read end
    write(ptoc[1], tosend, tosend_len) ;
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
    
    // memset(buf, sizeof(buf), 0) ;
    size_t len ;
    read_len = 0 ;
    while ((len = read(ctop[0], buf+read_len, sizeof(buf)-read_len)) > 0) {
        read_len += len ;
        printf("received stderr size %ld...\n", read_len) ;
        // write(STDOUT_FILENO, buf, len) ;   
    }
    buf[read_len] = '\0';
    close(ctop[0]) ;

    // compare error messages
    if (strstr(buf, error_message) != NULL) {
        fprintf(stderr, "buf : %s\n", buf) ;
        fprintf(stderr, "error message argument : %s\n", error_message) ;
        return 1 ; // indicate that error message is found
    }

    return 0 ;
}

char *
delta_debugging(char *input, size_t input_len) 
{
    memcpy(tm, input, input_len) ;
    tm_len = input_len ;
    tm[tm_len] = '\0' ;

    fprintf(stderr, "tm : %s\n", tm) ;

    // printf("%s", tm) ;

    size_t s = tm_len - 1 ;
    while (s > 0) {
        for (int i = 0; i <= tm_len - s; i++) {
            char head[MAX], tail[MAX] ;
            size_t head_len, tail_len ;
            
            head_len = i - 1 + 1 ; 
            printf("head length : %ld\n", head_len) ;
            memcpy(head, tm, head_len) ;
            head[head_len] = '\0' ;
            
            tail_len = tm_len - 1 - i - s + 1 ;
            printf("tail length : %ld\n", tail_len) ; 
            memcpy(tail, tm + i + s, tail_len) ;
            tail[tail_len] = '\0' ;
           
            printf("head : %s\n", head) ;
            printf("tail : %s\n", tail) ;
            
            char headtail[MAX] ;
            size_t headtail_len ;

            memcpy(headtail, head, head_len) ;
            memcpy(headtail + head_len, tail, tail_len) ;
            headtail_len = head_len + tail_len ;
            headtail[headtail_len] = '\0' ;

            printf("head+tail : %s\n", headtail) ;

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
                    return delta_debugging(headtail, headtail_len) ;
                }
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
                    return delta_debugging(mid, mid_len) ;
                }
            }
        }
        s-- ;
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
    
    size_t len ;
    while ((len = fread(buf + read_len, 1, sizeof(buf), fp)) > 0) {
        read_len += len ;
    }
    buf[read_len] = '\0' ;

    printf("read length : %ld\n", read_len) ;

    fclose(fp) ;

    // 결과 받기 
    char *reduced = delta_debugging(buf, read_len) ;
    printf("reduced input : %s\n", reduced) ;

    return 0 ;
}