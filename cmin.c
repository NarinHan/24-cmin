#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>

char *crash ;
char *message ;
char *output_filename ;
char *target_filepath ;

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
                message = optarg ;
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

    if (crash == NULL || message == NULL || output_filename == NULL || target_filepath == NULL) {
        fprintf(stderr, "Missing required options...\n") ;
        exit(EXIT_FAILURE) ;
    }

}

int
main(int argc, char *argv[]) 
{
    if (argc != 8) {
        fprintf(stderr, "%s -i [file path of the crashing input] -m [standard error message string] -o [file path to store the result] [target file path]\n", argv[0]) ;
    }

    parse_option(argc, argv) ;

    return 0 ;
}