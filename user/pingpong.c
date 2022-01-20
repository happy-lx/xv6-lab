#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int 
main(int argc, char* argv[]){
    int fd[2];

    if(pipe(fd) < 0){
        fprintf(2,"pipe error...\n");
        exit(1);
    }

    int pid;

    if((pid = fork()) < 0){
        fprintf(2,"fork error...\n");
        exit(1);
    }
    // data race happen in fprintf ?
    // how to solve this ?
    // two process race for fprintf

    // my thought:
    // one use sys_write and go into kernel 
    // one is printing
    // time for one is over and it's [premitive]
    // turn to two
    // two print

    if(pid == 0){
        // child
        pid = getpid();
        char temp[1];
        read(fd[1],temp,1);
        // TODO: refactor
        sleep(10);
        fprintf(2, "%d: received pong\n",pid);
    }else{
        // parent 
        pid = getpid();
        write(fd[1],"f",1);
        fprintf(2, "%d: received ping\n",pid);
        // this is ugly
        sleep(11);
    }

    exit(0);


}