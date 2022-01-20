#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int is_prime(int num){
    // judge whether a number is prime

    for(int i = 2 ; i < num ; i++){
        if( (num % i) == 0 ){
            return 0;
        }
    }
    return 1;
}

void print_out(int num){
    fprintf(1,"prime %d\n",num);
}

int
main(int argc, char *argv[]){
    const int MAX_NUM = 35;
    const int MIN_NUM = 2;

    for(int i = MIN_NUM ; i <= MAX_NUM ; i++){
        int fd[2];
        int status;
        if(is_prime(i)){
            if(pipe(fd) < 0){
                fprintf(2,"pipe fail...\n");
                exit(1);
            }
            if(fork() == 0){
                // children
                int num[1];
                read(fd[0],num,sizeof(num));
                close(fd[0]);
                // judge prime and print
                print_out(num[0]);
                break;
            }else{
                // parent
                write(fd[1],&i,sizeof(i));
                // the concurrent is ugly
                // how to solve the data race problem ??
                wait(&status);
                close(fd[1]);
            }
        }
    }
    exit(0);
}