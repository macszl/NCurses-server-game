//
// Created by maciek on 1/15/22.
//

#ifndef NCURSES_SERVER_GAME_FIFOHELPER_H
#define NCURSES_SERVER_GAME_FIFOHELPER_H

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

int make_fifos();
int unlink_fifos();

#endif //NCURSES_SERVER_GAME_FIFOHELPER_H
