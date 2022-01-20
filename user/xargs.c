#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/param.h"

char* gets_buf(char* buf){
    char c;
    while(read(0,&c,1) > 0){
        if(c == '\n' || c == '\r'){
            *buf++ = '\0';
            return buf;
        }else{
            *buf++ = c;
        }
    }
    return 0;
}

int get_argc(char** argv){
    int res = 0;
    for(int i=0;i<MAXARG;i++){
        if(argv[i]) res++;
    }
    return res;
}

void get_arg(char** arg, char** res, int num, char* buf){

    int idx = 0;

    for(int i=0;i<num;i++){
        if(!arg[i]) continue;

        if(strcmp(arg[i],"xargs") == 0) continue;

        if(strcmp(arg[i], "-n") == 0){
            i++;
            continue;
        }

        res[idx++] = arg[i]; 
    }
    int argc = get_argc(res);
    char* temp_buf = malloc(sizeof(char) * 512);
    memset(temp_buf,0,sizeof(char) * 512);
    idx = 0;

    for(int i=0;i<=strlen(buf);i++){
        if(buf[i] == ' ' || buf[i] == '\0'){
            temp_buf[idx++] = '\0';
            res[argc++] = temp_buf;
            temp_buf = malloc(sizeof(char) * 512);
            memset(temp_buf,0,sizeof(char) * 512);
            idx = 0;
        }else {
            temp_buf[idx++] = buf[i];
        }
    }

    // printf("debug: buf is %s\n",buf);
    
    // for(int i=0;i<MAXARG;i++){
    //     printf("debug: res[%d] is %s\n",i,res[i]);
    // }
}

int main(int argc, char* argv[]){
    char buf[512];
    int status;

    char* argv_child[MAXARG] = {0};

    // for(int i=0;i<argc;i++){
    //     printf("debug: argv[%d] is %s\n",i,argv[i]);
    // }

    while(gets_buf(buf)){
        memset(argv_child,0,MAXARG * sizeof(char*));
        get_arg(argv,argv_child,argc,buf);
        if(fork() == 0){
            // child
            exec(argv_child[0],argv_child);
        }else{
            // parent
            wait(&status);
        }
    }    
    exit(0);
}