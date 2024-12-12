#include "ec440threads.h"
#include <pthread.h>
#include <setjmp.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>

void schedule();
void helper();
#define JB_R12  2
#define JB_R13  3
#define JB_RSP  6
#define JB_PC   7


typedef enum{
        NOT_INITIALIZED,
        READY,
        RUNNING,
        EXITED
} threadStatus;


typedef struct{
        pthread_t threadID;
        jmp_buf context;
        void* stackPointer;
        threadStatus status;
}TCB;


TCB TCBs[127];
int numThreads = 0;
int current_thread = 0;
int alarmON = 0;
int firstCall = 1;
int threadID_counter = 1;

int pthread_create (pthread_t *thread, const pthread_attr_t *attr, void *(start_routine) (void*), void *arg){

        if(firstCall){
                helper();
                firstCall = 0;
        }

        //STEP1: CREATE NEW TCB

        if(numThreads > 127){
                return -1;
        }

        TCB *tcb = &TCBs[numThreads];
        tcb->threadID = threadID_counter++;
        numThreads++;

        tcb->stackPointer = malloc(32767);
        void* stack_top = (void*)(tcb->stackPointer + 32767);

        stack_top = stack_top - sizeof(void*);
        *(unsigned long int *) stack_top = (unsigned long int) pthread_exit;

        tcb->context[0].__jmpbuf[JB_RSP] = ptr_mangle((unsigned long int)( stack_top ));
        tcb->context[0].__jmpbuf[JB_PC] = ptr_mangle((unsigned long int)(start_thunk));

        tcb->context[0].__jmpbuf[JB_R12] = (unsigned long int)(start_routine);
        tcb->context[0].__jmpbuf[JB_R13] = (unsigned long int)(arg);


        //STEP2: SET STATE TO READY
        tcb->status = READY;


        return 0;
}




void helper(){
        struct sigaction sa;
        sa.sa_handler = schedule;
        sa.sa_flags = SA_NODEFER;
        sigemptyset(&sa.sa_mask);  //ensure no signals are masked


        if(sigaction(SIGALRM, &sa, NULL) == -1){
                perror("sigaction");
                exit(EXIT_FAILURE);
        }


        TCB *tcb = &TCBs[numThreads++];
        tcb->threadID = threadID_counter++;
        tcb->status = RUNNING;

        ualarm(50000,50000);
}




void schedule(){
        //printf("SCHEDULING!!!!!!\n");
        if( setjmp(TCBs[current_thread].context) == 0 ){
                TCBs[current_thread].status = READY;

                do{
                        current_thread = (current_thread + 1) % threadID_counter;
                }
                while(TCBs[current_thread].status != READY);


                TCBs[current_thread].status = RUNNING;
                longjmp(TCBs[current_thread].context, 1);
        }

        //ualarm(50000,50000);
}



void pthread_exit(void *value_ptr){
        //printf("EXITING !!!!!!!\n");

        TCBs[current_thread].status = EXITED;
        free(TCBs[current_thread].stackPointer);
        memset(&TCBs[current_thread], 0 , sizeof(TCB));
        numThreads--;

        if(numThreads == 0){
                exit(0);
        }

        while(1){};
}




pthread_t pthread_self(void){
        return TCBs[current_thread].threadID;
}
