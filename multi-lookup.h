//
//  multi-lookup.h
//  PA3__
//
//  Created by Joel Davidson on 3/5/18.
//  Copyright © 2018 Joel Davidson. All rights reserved.
//

#ifndef multi_lookup_h
#define multi_lookup_h

#include <pthread.h>        // pthreads!
#include <stdio.h>
#include <stdbool.h>


struct argsForThreads{
    FILE* infiles[10];
    char hostNames[20][1025];
    char filesToDo[10][1025];
    void* args;
    bool requestorsDone;
    int whichFile;
    int filesLeft;
    int lookupCounter;
    int writeCounter;
    int argc;
    int requestors;
    FILE* serviced;
    FILE* results;
    pthread_mutex_t sharedArrayLock;
    pthread_mutex_t requestorLock;
    pthread_mutex_t resolverLock;
};
void requestorCreator(char* fileName[]);
void* readFile(struct argsForThreads *);
void* lookup(struct argsForThreads *);


#endif /* multi_lookup_h */
