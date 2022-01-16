//
// Created by maciek on 1/15/22.
//

#include "fifohelper.h"

int make_fifos()
{

    char fifo_fname[20] = "fifo_s_to_p";
    char fifo_fname1[20] = "fifo_p_to_s";
    const char * init_postfix = "_init";

    char fifo_fname_init[20];
    memset(fifo_fname_init, 0, 20 * sizeof(char ));
    strcpy(fifo_fname_init, fifo_fname);
    strcat(fifo_fname_init, init_postfix);
    if( mkfifo( fifo_fname_init , 0666) == -1) {
        if( errno != EEXIST)
        {
            printf("Couldn't create FIFO: %s\n", fifo_fname_init);
            return -1;
        }
    }
    memset(fifo_fname_init, 0, 20 * sizeof(char ));
    strcpy(fifo_fname_init, fifo_fname1);
    strcat(fifo_fname_init, init_postfix);
    if( mkfifo( fifo_fname_init, 0666) == -1) {
        if( errno != EEXIST)
        {
            printf("Couldn't create FIFO: %s\n", fifo_fname_init);
            return -1;
        }
    }


    char num[2] = "0";
    strcat(fifo_fname, num);
    strcat(fifo_fname1, num);
    for(int i = 1; i < 5; i++)
    {
        sprintf(num, "%d", i);
        fifo_fname[strlen(fifo_fname) - 1] = num[0];
        fifo_fname1[strlen(fifo_fname1) - 1] = num[0];
        if( mkfifo( fifo_fname, 0666) == -1) {
            if( errno != EEXIST)
            {
                printf("Couldn't create FIFO: %s\n", fifo_fname);
                return -1;
            }
        }
        if( mkfifo( fifo_fname1, 0666) == -1) {
            if( errno != EEXIST)
            {
                printf("Couldn't create FIFO: %s\n", fifo_fname1);
                return -1;
            }
        }
    }
    return 0;
}
int unlink_fifos()
{
    unlink("fifo_s_to_p_init");
    unlink("fifo_p_to_s_init");

    unlink("fifo_p_to_s1");
    unlink("fifo_p_to_s2");
    unlink("fifo_p_to_s3");
    unlink("fifo_p_to_s4");

    unlink("fifo_s_to_p1");
    unlink("fifo_s_to_p2");
    unlink("fifo_s_to_p3");
    unlink("fifo_s_to_p4");

    return 0;
}
