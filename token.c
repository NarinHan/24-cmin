#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    char *target_cmd ;
    target_cmd = (char *)malloc(128) ;
    strcpy(target_cmd, "./testsrc/libxml2/xmllint --recover --postvalid -") ;
    char *token = strtok(target_cmd, " ") ;
    printf("first : %s\n", token) ;

    char *temp = (char *)malloc(strlen(target_cmd) - strlen(token) + 1) ;
    strcat(temp, target_cmd + strlen(token) + 1) ;
    printf("option? %s_\n", temp) ;

    free(target_cmd) ;
    free(temp) ;

    // while (token != NULL) {
    //     printf("%s\n", token);
    //     token = strtok(NULL, " ");
    // }

    return 0;
}
