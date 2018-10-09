//
//  multi-lookup.c
//  PA3__
//
//  Created by Joel Davidson on 3/5/18.
//  Copyright © 2018 Joel Davidson. All rights reserved.
//

#include "multi-lookup.h"
#include <pthread.h>        // pthreads!
#include <stdlib.h>
#include <stdio.h>        // for file I/O, stderr
#include <errno.h>        // for errno
#include <unistd.h>        // for
#include "util.h"        // for DNS lookup
#include <time.h>         // for the timer
#include <sys/time.h>


int main(int argc,char * argv[]) {
    //clock_t begin = clock();
    time_t time1;
    time_t time2;
    time(&time1);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int arg1 = atoi(argv[1]);
    int arg2 = atoi(argv[2]);
    struct argsForThreads threadInfo;
    threadInfo.requestors = arg1;

    
    //Initialize the mutexes
    //https://www.ibm.com/support/knowledgecenter/en/SSLTBW_2.1.0/com.ibm.zos.v2r1.bpxbd00/ptcwait.htm
    if (pthread_mutex_init(&threadInfo.sharedArrayLock, NULL) != 0) {
        perror("pthread_mutex_init() error");
        exit(1);
    }
    
    if (pthread_mutex_init(&threadInfo.requestorLock, NULL) != 0) {
        perror("pthread_mutex_init() error");
        exit(1);
    }
    
    if (pthread_mutex_init(&threadInfo.resolverLock, NULL) != 0) {
        perror("pthread_mutex_init() error");
        exit(1);
    }
    
    //to verify args
    printf("\nargc = %d\n", argc);
    
    //Initialize the elements of the struct to null characters or 0.
    for(int i=0; i<20; i++){
        threadInfo.hostNames[i][0] = '\0';
    }
    for(int i=0; i<argc-5; i++){
        threadInfo.infiles[i] = fopen(argv[i+5], "r");
    }
    threadInfo.results = fopen(argv[4], "w");
    threadInfo.args = (void*)argv;
    threadInfo.requestorsDone = false;
    threadInfo.whichFile = 0;
    threadInfo.filesLeft =argc-5;
    threadInfo.lookupCounter =0;
    threadInfo.argc = argc;
    threadInfo.serviced = fopen(argv[3], "w");
    
    //copy names of input files from argv to filesToDo whose elements will be set to \0 when requestors are done with files
    for(int i=0; i<argc-5; i++){
        strncpy(threadInfo.filesToDo[i], argv[i+5], sizeof(threadInfo.filesToDo[i]));
        printf("\n%s", threadInfo.filesToDo[i]);
    }
    
    
    pthread_t requestor;
    for(int i=0; i<arg1; i++){
        pthread_create(&requestor, NULL, (void*) readFile, &threadInfo);
        printf("\nID: %d", (int)requestor);
    }
    
    pthread_t resolver;
    for(int i=0; i<arg2; i++){
        pthread_create(&resolver, NULL, (void*) lookup, &threadInfo);
    }
    
    pthread_join(requestor, NULL);
    
    pthread_join(resolver, NULL);
    
    //print out shared buffer to verify it has been emptied
    for(int i=0; i<20; i++){
        printf("\nElement %d %s", i, threadInfo.hostNames[i]);
    }
    time(&time2);
    time1 = time2 - time1;
    printf("\nRuntime: %ld\n", time1);//runtime in seconds
    //clock_t end = clock();
    //double time_spent = (double)(end - begin) / (double)CLOCKS_PER_SEC;
    //printf("\nRuntime: %f\n", time_spent);
    fclose(threadInfo.serviced);
    fclose(threadInfo.results);
    gettimeofday(&end, NULL);
    //runtime in miliseconds
    printf("\n%ld\n", ((end.tv_sec * 1000000 + end.tv_usec) - (start.tv_sec * 1000000 + start.tv_usec)));
    pthread_mutex_destroy(&threadInfo.resolverLock);
    pthread_mutex_destroy(&threadInfo.sharedArrayLock);
    pthread_mutex_destroy(&threadInfo.requestorLock);
    return 0;
}



void* readFile(struct argsForThreads *threadInfo){
    bool done = false;
    bool dontCopy;
    int count = 0;
    //int line = 0;
    pthread_mutex_lock(&threadInfo->requestorLock);
    
    int inputFileNo = threadInfo->whichFile++;
    if(threadInfo->whichFile == threadInfo->argc-5){
        threadInfo->whichFile = 0;
        inputFileNo =threadInfo->whichFile++;
    }
    pthread_mutex_unlock(&threadInfo->requestorLock);
    //char **arguments = (char**)threadInfo->args;
    char hostNameHolder[1025];
    printf("\nI'm inside the readFile().\n");
    if(!threadInfo->infiles[inputFileNo]){
        printf("\nSomething went wrong with opening the file.\n");
        exit(0);
    }
    

    while(!done){
        dontCopy = false;
        //FilesToDo is an array that is has the file names copied to it from argv
        pthread_mutex_lock(&threadInfo->requestorLock);
        
        //checkk to make sure the file the thread is in was not finished by another thread
        if(threadInfo->filesToDo[inputFileNo][0] == '\0'){
            //if it is finished, look for another file to work on
            for(int i=0; i<threadInfo->argc-5; i++){
                if(threadInfo->filesToDo[i][0] != '\0'){
                    inputFileNo = i;
                    break;
                }else if(i==threadInfo->argc-1){
                    done = true;
                    threadInfo->requestors--;
                    printf("\nI just quit because I went through the files top.\n");
                    pthread_mutex_unlock(&threadInfo->requestorLock);
                    break;
                }
            }
        }
        //read in a namae from the file until EOF.  if statement is for when EOF
        if(!(fscanf(threadInfo->infiles[inputFileNo], "%s", hostNameHolder) >0)){
            if(threadInfo->filesToDo[inputFileNo][0] != '\0'){
                threadInfo->filesLeft--;
                threadInfo->filesToDo[inputFileNo][0] = '\0';
            }
            
            printf("\nDone reading file. %d names read.\n", count);
            
            dontCopy = true;
            if(threadInfo->filesLeft==0){
                printf("\nI just quit because filesLeft == 0.\n");
                threadInfo->requestors--;
                done = true;
                if(!threadInfo->requestors){
                    pthread_mutex_lock(&threadInfo->sharedArrayLock);
                    threadInfo->requestorsDone = true;
                    printf("\nAll requestors done.\n");
                    pthread_mutex_unlock(&threadInfo->sharedArrayLock);

                }
                
            }else{//not EOF
                //look for another file to work on
                for(int i=0; i<threadInfo->argc-5; i++){
                    if(threadInfo->filesToDo[i][0] != '\0'){
                        inputFileNo = i;
                        break;
                    }else if(i==threadInfo->argc-1){
                        done = true;
                        threadInfo->requestors--;
                        printf("\nI just quit because I went through the files bottom.\n");
                        pthread_mutex_unlock(&threadInfo->requestorLock);
                        break;
                    }
                }
                
            }
            
            pthread_mutex_unlock(&threadInfo->requestorLock);
            
        }else{
            /*
            for(int i=0; i<20; i++){
                printf("\nElement %d %s", i, threadInfo->hostNames[i]);
            }
            printf("\n");
            */
            count++;
            pthread_mutex_unlock(&threadInfo->requestorLock);
        }
        
        //dontCopy is for when the requestor gets to the end of a file, don't copy the EOF
        if(!dontCopy){
            for(int i=0; i<20; i++){
                pthread_mutex_lock(&threadInfo->sharedArrayLock);
                
                //find an empty element with first char set to '\0' and write to it
                if(threadInfo->hostNames[i][0] == '\0'){
                    strncpy(threadInfo->hostNames[i], hostNameHolder, sizeof(hostNameHolder));
                    pthread_mutex_unlock(&threadInfo->sharedArrayLock);
                    break;
                }else{
                    //if there are no empty elements
                    pthread_mutex_unlock(&threadInfo->sharedArrayLock);
                }
                if(i==19)
                    i=0;
            }
        }
        
    }
    fprintf(threadInfo->serviced, "Thread<%d> serviced %d files\n", (int)pthread_self(), count);
    printf("\nID: %d\n", (int)pthread_self());
    return NULL;
}

void* lookup(struct argsForThreads *threadInfo){
    bool done = false;
    char IPaddress[1025];
    while(!done){
        usleep(50);
        pthread_mutex_lock(&threadInfo->sharedArrayLock);
        
        for(int i=0; i<20; i++){
            if(threadInfo->hostNames[i][0] != '\0'){
                
                if(dnslookup(threadInfo->hostNames[i], IPaddress, sizeof(IPaddress))==UTIL_FAILURE){
                    fprintf(stderr, "Error: failed to lookup: %s\n", threadInfo->hostNames[i]);
                    strncpy(IPaddress, "", sizeof(IPaddress));
                }
                
                //Put blank line if it is unhandled
                if(strcmp(IPaddress,"UNHANDELED") == 0)
                    strncpy(IPaddress, "", sizeof(IPaddress));
                    
                threadInfo->lookupCounter ++;
                
                pthread_mutex_lock(&threadInfo->resolverLock);
                
                fprintf(threadInfo->results, "%s, %s\n", threadInfo->hostNames[i], IPaddress);
                
                printf("%s, %s,   %d\n", threadInfo->hostNames[i], IPaddress, threadInfo->lookupCounter);
                
                pthread_mutex_unlock(&threadInfo->resolverLock);
                
                threadInfo->hostNames[i][0] = '\0';
                
                
            }
        }
        
        //Make one pass through the entire shared array when the requestors are done
        if(threadInfo->requestorsDone == true){
            for(int i=0; i<20; i++){
                if(threadInfo->hostNames[i][0] != '\0'){
                    if(dnslookup(threadInfo->hostNames[i], IPaddress, sizeof(IPaddress))==UTIL_FAILURE){
                        fprintf(stderr, "Error: failed to lookup: %s\n", threadInfo->hostNames[i]);
                        strncpy(IPaddress, "", sizeof(IPaddress));
                    }
                    
                    if(strcmp(IPaddress,"UNHANDELED") == 0)
                        strncpy(IPaddress, "", sizeof(IPaddress));
                    threadInfo->lookupCounter ++;
                    
                    pthread_mutex_lock(&threadInfo->resolverLock);
                    
                    fprintf(threadInfo->results, "%s, %s\n", threadInfo->hostNames[i], IPaddress);
                    
                    printf("%s, %s,   %d\n", threadInfo->hostNames[i], IPaddress, threadInfo->lookupCounter);
                    pthread_mutex_unlock(&threadInfo->resolverLock);
                    threadInfo->hostNames[i][0] = '\0';
                    
                    
                }
            }
            done = true;
            pthread_mutex_unlock(&threadInfo->sharedArrayLock);
        }else{
            pthread_mutex_unlock(&threadInfo->sharedArrayLock);
        }
    }
            
    
    return NULL;
}




