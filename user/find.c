#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"

char* get_name(char* path){
    // ./a/b -> b
    // a -> a
    char* p;

    // get the first char location after '/'
    for(p = path + strlen(path) ; p >= path && *p != '/' ; p--);
    p++;

    return p;
}

void 
find(char* path,char* name){
    char buf[512];
    int fd;
    struct stat st;
    struct dirent de;
    char* p;
    char* file_name;

    if((fd = open(path,O_RDONLY)) < 0){
        fprintf(2, "find: cannot open %s\n",path);
        return;
    }
    if(fstat(fd, &st) < 0){
        fprintf(2, "find: cannot stat %s\n",path);
        close(fd);
        return;
    }
    switch (st.type)
    {
    case T_FILE:
        file_name = get_name(path);
        if(strcmp(name,file_name) == 0){
            printf("%s\n",path);
        }
        break;
    case T_DIR:
        strcpy(buf,path);
        p = buf + strlen(buf);
        *p = '/';
        p++;
        while(read(fd, &de, sizeof(de)) == sizeof(de)){
            if(de.inum == 0 || strcmp(de.name,".") == 0 || strcmp(de.name,"..") == 0)
                continue;
            memmove(p, de.name, strlen(de.name));
            p[strlen(de.name)] = 0;
            find(buf,name);
        }
        break;
    
    default:
        break;
    }
    close(fd);
}

int 
main(int argc,char* argv[]){

    if(argc != 3){
        fprintf(2,"Usage: find [path] [file]...\n");
        exit(1);
    }

    char* path = argv[1];
    char* file = argv[2];

    find(path,file);
    exit(0);

}