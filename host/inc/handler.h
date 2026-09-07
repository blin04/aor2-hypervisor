#ifndef HANDLER_H
#define HANDLER_H

enum FILE_OP_STATE {
    NO_OP,     // no file op started
    STARTED    // file op in progress
};

void* handler(void* arg);

#endif