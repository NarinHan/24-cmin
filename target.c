#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int main(int argc, char * argv[]){
    char buff[256];
    // printf("here!\n");
    scanf("%s", buff);
    fprintf(stderr, "%s", buff);
    // printf("printf target : %s\n", buff);
    // fprintf(stderr, "fprintf target : %s\n", buff);
    return 0;
}