#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int
main(int argc, char *argv[])
{
    struct sysinfo res;

    if((sysinfo(&res)) < 0) {
        exit(1);
    }
    // printf("kernel free memory : %d(bytes)\nkernel process num : %d\n", res.freemem, res.nproc);

    exit(0);
}