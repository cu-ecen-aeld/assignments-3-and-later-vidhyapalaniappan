#include<stdio.h>
#include <stdlib.h>
#include <syslog.h>

int main(int argc, char *argv[])
{
    openlog(NULL, 0, LOG_USER);
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <file_path> <write_string>\n", argv[0]);
        syslog(LOG_ERR, "Error : Exactly two arguments are not passed \n");
        exit (1);
    }

    char *file_name = argv[1];
    char *write_str = argv[2];

    FILE *fp;

    fp = fopen(file_name, "w+");
    if (fp == NULL) {
        perror("Error opening file");
        exit (1);
    }

    fputs(write_str, fp);
    syslog(LOG_DEBUG, "Successfully written to file");
    fclose(fp);

    return 0;
}
