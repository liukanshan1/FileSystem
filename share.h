#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <map>

const int SHARED_MEMORY_KEY = 14325; // 共享内存的键
const int SEMAPHORE_KEY = 54891;    // 信号量的键
const int SHARED_MEMORY_SIZE = 2048; // 共享内存的大小

// 命令结构
struct Message {
    int userId;
    char message[1024];
};


