//
// Created by Kevin Trogant on 13.05.17.
//

#ifndef MULTITHREADED_COMPILER_LOCATION_H
#define MULTITHREADED_COMPILER_LOCATION_H

typedef struct {
    char* file;
    int start_line;
    int end_line;
    int start_column;
    int end_column;
} location_t;

#endif //MULTITHREADED_COMPILER_LOCATION_H
