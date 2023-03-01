## trap 实验的 backtrace

这个实验给了一个思路：利用函数调用时的栈结构，从错误点开始倒推到最上面，找到函数调用的序列

实现的时候比较简化，xv6的内核栈也只有一个page，fp指向的是当前函数栈开始的地方，结构大致为：

| fp ----> |                 |
| -------- | --------------- |
|          | ra              |
|          | old fp          |
|          | arg             |
|          | arg             |
|          | saved registers |
| sp --->  |                 |

需要backtrace时读fp，然后记录每个 function frame 里面的ra，利用old fp一直往上推，一直推到超过一个page的边界就结束。利用这些记录的ra，可以通过addr2line工具，找到对应于哪个函数的哪一行，ra指向的是函数调用点返回的地方。

## trap 实验的 alarm

实现了一个用户态定时器，当进程在用户态下执行程序时，遇到时钟中断后会进入处理程序，查看是否注册了定时器，如果注册了且定时器到期，需要去执行用户态的一个函数，函数执行完之后通过一个特殊的系统调用返回到时钟中断发生时的用户态现场继续执行。

这个实验主要实现思路是：当进程注册定时器时，将interval和函数地址保存到进程控制块内，每次**这个进程**发生发生时钟中断时，如果注册了定时器，将定时器减一，一旦减到0，就需要将trapframe里面的epc设置为定时器处理函数，并且将时钟中断发生时用户态现场（包括epc和32个通用寄存器）保存到进程控制块中，时钟中断处理完成返回到定时器处理函数执行，执行完毕后使用一个系统调用，这个系统调用的处理是：重新开始计时，并将时钟中断时的现场恢复到trapframe中，系统调用一返回就可以回到当时的现场。

## COW

实现了一个 copy-on-write 的 fork

**需要额外注意多核下并发执行的情况**

给系统可访问的每一块物理页都设置了一个引用计数，分配时设置为1，fork时共享物理页加1，释放时减少1只有减少到0才会释放。

每次fork只复制父进程的页表，不复制数据，并且把父子进程的页表内的所有可写位清空，并在pte的保留位中写1，表明这是cow的页。后续父子进程任意一个要写都会产生store page fault。

产生page fault后，先检查能否处理这个异常，如果能处理，是一个cow的页，且物理页被多个进程共享，就分配一个新的物理页，**把内容拷贝过去**，设置pte指向新的物理页，并设置可写标识；如果是一个cow的页，物理页只有一个进程在用，就不分配新的物理页，只把pte中的可写标识设置上。

读，增减引用计数时都需要仔细的并发控制，防止两个核同时产生page fault，并同时读到引用计数为2，同时都去分配新的物理页，导致原来的物理页丢失。

## NET实验

这个实验实际要写的内容不多，但是老师额外为这个实验增加了大量的网络部分的代码，包括socket，网络栈支持，ARP，UDP，DNS协议，看这些代码对以前学的网络有很多帮助

首先是socket，用户程序通过connet系统调用来创建一个socket，指明目标ip地址和目标进程端口号和当前进程端口号，所有的socket都是用一个链表连起来的，通过ip地址和端口号来区分，connet返回一个文件描述符，用户程序通过read或者write系统调用来使用。

read write时判断到这是一个socket，会去执行socketread或者socketwrite。每一个socket会有一个链表，链起所有的网络包buffer，所有收到的网络包都会链上去，socketread时如果发现有就拿出来复制到用户空间去，没有就等待。socketwrite会分配一个网络包，把数据写进去，然后给到网络栈，网络栈一层一层给加上头部，先是udp头部，再是ip头部，再是以太网头部，最后通过驱动程序发出去。

每收到一个包，会有中断发生，处理程序会把这个包给到网络栈，网络栈一层一层解析头部，根据类型做不同的处理，首先是以太网头部，类型可能会有ip包或者arp包，如果是arp包就去给arp的处理函数，如果是ip包给ip的处理函数，ip头处理完了之后处理udp头部，udp头部处理完了之后就是完全的应用层数据，kernel不关心应用层是什么协议，给到socket，把这个包链到对应socket的网络包链表里面去，等待对应的进程去读

驱动部分收发倒是比较简单，根据hint去写就完了，收发都是一个ring，环形队列，发的时候去看tail指针，tail指针如果空闲，设置对应的descriptor，指明包的地址，长度，cmd，然后把tail指针加一。收的时候也是去看收的ring的tail指针，如果数据包到了，就把它给网络栈处理，给tail那重新分一个包，让后续来的包有地方可去

需要注意的是dns测试时，收到的包可能跟美国的不一样，包里面可能会有权威区域，需要像处理answer区域那样去处理权威区域，不然可能会收到 收到的数据与处理的数据长度不一致的错误

## Thread实验

这个实验整体比较简单，没学到什么新东西，在用户态下做了一个极其简单的线程，模仿kernel的调度线程实现就可以了，切换的时候把context保存，加载新线程的context，每个线程都有自己的栈，状态。都实现在用户态下，A指令集应该在用户态下也能用，这样应该可以在用户态下实现mutex，condition，semi这些。kernel没法感知这种线程，所以无法真正的并行，pthread应该也是用户态下的，但是Linux可以调度这种线程

pthread的使用没什么好说的，barrier可以拿mutex和condition实现，拿个计数器，线程来了先上锁，计数器加1，还有没到的就把当前线程阻塞了，释放锁，都来了就唤醒被阻塞的线程

## Lock实验

这个实验花了一整周多的时间，主要是bcache一直在调bug，尽管以前学过并发还是感慨并发的东西真难，不仅不好想，也不好调试，基本上很难复现出来。

kalloc是比较简单的，有几个核就用几个链表，每个核有自己的free链表，每次自己的没有了就去找别人要，每一个时刻都只拿一个锁，保证可以不死锁。先拿自己的链表的锁，发现没有了就释放，然后去拿别人的链表的锁，拿到就用，用完了拿自己的链表的锁，放在自己这。

主要难的就是bcache，我看了网上很多其他的做法感觉都不太行，要么有死锁的风险，要么会存在有buffer但是报无buffer的情况，最后实现的方案应该是正确性和性能都是最完美的解决方案了。

首先有13个hash bucket，每一个bucket会有自己的锁，会有一个bcache的全局锁。

+ 初始化
  
  + 把buffer均匀地分到每一bucket里面

+ bget(dev, blockno)
  
  + 1.首先算出hash，得到要访问的bucket，先拿这个bucket的锁
  
  + 2.访问这个bucket，如果找到了buffer就释放锁，去拿buffer的sleeplock然后返回
  
  + 3.如果没有找到buffer，**也释放bucket的锁**，去拿bcache的全局锁
  
  + 4.拿到全局锁之后，去访问系统总的那个buffer数组，找一个ref为0的最旧的buffer
    
    + 4.1 如果在这个过程中发现了一个buffer，其dev和blockno跟请求的一致，说明出现了 `false miss`，释放bcache的全局锁，重新回到1执行。（false miss指两个核想同时要一个buffer，都miss了，第一个先拿到全局锁，evict 了一个buffer，然后结束，此时其实系统有这个buffer了；但是第二个没有拿到全局锁的一直在锁那里自旋等待，等第一个miss释放全局锁后进来，其实它已经不用再evict一个buffer了，它一定能发现要找的buffer已经在系统中了，就释放锁重新回到1执行（goto语句）
    
    + 4.2 找到一个要替换的buffer后，查看这个buffer在哪个bucket里面，去拿这个bucket的锁，拿到之后去看ref是不是还等于0，如果不等于0说明：在遍历buffer数组找到这个buffer和拿到它的bucket的锁之间有其他进程又使用了这个buffer，使得它的ref不为0，此时不能替换这个buffer，而是释放掉它的bucket的锁，重新回到4.1重新找一个buffer
    
    + 4.3 如果上面都没有问题，说明找到了一个buffer去evict，此时我们有全局锁，以及这个buffer对应的bucket的锁，之后可以直接把这个buffer从bucket里面拿出来，释放这个bucket的锁，然后修改buffer里面的dev和blockno，然后去拿它应该要被放到的那个bucket的锁，放进去，释放bucket的锁，释放全局锁，拿buffer的sleeplock然后返回。

+ brelease和bpin bunpin
  
  + 都是先计算buffer在哪个bucket里面，拿到bucket的锁
  
  + 对ref操作
  
  + 释放锁

以上实现理论分析一定是无死锁的，一个进程最多会拿两个锁，一旦拥有两个锁，第一个一定是bcache的全局锁，顺序不会乱，破坏了死锁的一个必要条件。

还有一个很坑的地方是：**bcachetest test0测完之后没有把文件删掉**，导致在文件系统里面占用了30多个块，导致后续执行usertests的写大文件的时候会出现balloc报错(**但是单独测bcache test和usertests都能过**)(**在单核的原来的xv6里面按照bcachetest->usertests流程执行也同样会报文件系统没块的错**)，我后来还算了一下大概刚刚好会超过1000个块。修改完这个再执行./grade脚本就没问题了。

最后就是**goto非常容易导致错误**，要非常小心去用

## FS实验

两个实验都比较简单，第一个是关于文件系统的inode对数据块的索引组织方式，第二个是做一个软链接，以前只听说过软连接和硬链接的区别，这次实际感受了一下。

原本的 inode 结构为

```c
0      -> datablock
1      -> datablock
2      -> datablock
3      -> datablock
4      -> datablock
5      -> datablock
6      -> datablock
7      -> datablock
8      -> datablock
9      -> datablock
10     -> datablock
11     -> datablock
12     -> -> -> -> -> -> | block contains 256 inode number |
```

现在改为了

```c
0      -> datablock
1      -> datablock
2      -> datablock
3      -> datablock
4      -> datablock
5      -> datablock
6      -> datablock
7      -> datablock
8      -> datablock
9      -> datablock
10     -> datablock
11     -> -> -> -> -> -> | block contains 256 inode number |
12     -> -> -> -> -> -> | block contains 256 inode number |    -> -> -> ->     | block contains 256 inode number |
```

多增加了一级索引，主要就是对 bmap 的映射进行了修改，和 itruct 删除时删除所有的块的方式进行了修改

软链接是通过一个系统调用 symlink 来创建，创建时文件类型为软链接类型，文件内容为实际的文件路径，这个路径可以不存在。

实际使用软链接的时候，使用 open，在 sys_open 中会发现这个文件为软链接，通过 namei 去找指向的文件，可能会继续找到软链接，如果深度大于10就认为出现了环，报错。找到实际指向的文件的inode后，把文件描述符里面的inode设置为这个inode，后续的读写操作就都是在实际指向的文件中进行的。如果open的时候指定 O_NOFOLLOW ，就不会找指向的文件，而是直接打开软链接文件

硬链接是在某一个目录下创建一个目录项，这个目录项的inode号指向对应的文件。软链接是一个新的文件，inode号和对应的文件inode号不一样

打开软链接的时候还会根据文件中存的地址一层一层找inode找到指向的文件，效率会差一点。软链接可以跨文件系统，硬链接不可以

## MMap

做了一个极其简化的 mmap，只对文件进行 mmap，不能 mmap 任意的虚拟地址，就很难搞出一堆骚操作，比如这个老师讲用户程序利用的 page fault，可以把一个巨大的数组只实际使用一个页面，还有奇怪的垃圾回收算法。

首先需要给每一个进程都增加一个 vma，这个也是个极其简化的 vma，是一个 16 项的数组，每一项的虚拟地址空间大小为 4G，一共 64 G，分布在 MAXVA 的低 68 G，MAXVA 往下 4G 是一个 gap，由于 sv39 xv6 用到了 38 位所以实际上可用的虚地址有 256G，只要 heap 上界不要超过 188 G 就可以

```c
#define SINGLEVMASIZE (1 << 32)
#define VMASIZE (16)

// virtual memory area
struct vma {
  uint8  valid;
  uint8  shared;
  uint64 address;
  uint64 length;
  uint64 permission;
  uint64 offset; // offset of file
  struct file* f;
};

// User memory layout.
// Address zero first:
//   text
//   original data and bss
//   fixed-size stack
//   expandable heap
//   ... (todo: make sure vma will not cross over this)
//   vma[VMASIZE - 1]
//   vma[VMASIZE - 2]
//   ...
//   ---------------------------------------------------
//   vma[0]                                            + -> SINGLEVMASIZE
//   ---------------------------------------------------
//   gap                                               +
//   TRAPFRAME (p->trapframe, used by the trampoline)  + -> SINGLEVMASIZE
//   TRAMPOLINE (the same page as in the kernel)       +
//   ---------------------------------------------------
```

增加一个 mmap 的系统调用，给一个文件描述符和想要 map 的地址空间大小（注意：map 的地址空间可以比文件实际空间大，多出来的这部分为全0，但是 unmap 的时候不能改变原本的文件大小），处理这个系统调用时，只需要在这个进程的 vma 数组里面找一个空的就行，分配时默认的虚拟地址空间的起始地址为 MAXVA - 4G - (4G) \* i，i 为 vma 数组下标，长度为 mmap 参数。

等进程实际访问这个地址空间时，产生 page fault，在处理函数里面，检查这个地址是不是在有效的 vma 中，如果是才给它分配页表，同时分配物理页，把对应的文件内容读入物理页，进行地址映射。

增加一个 munmap 的系统调用，munmap 的时候，检查这个地址是不是在有效的 vma 中，如果是，取消对应的映射，如果是以 share 进行的 mmap，需要把对应的物理页写到文件中，释放对应的物理页，修改页表。

进程 exit 的时候，也需要把它所有的 vma 设置为 false，并且取消对应的映射，因为有的进程在 exit 前并不会自己调用 munmap

进程 fork 的时候，拷贝所有的 vma，为了简化，不用 lazy 的方式，如果父进程有效的 vma 中地址空间页表有内容，就分一个物理页，把对应的内容拷过来，对子进程建立新的映射
