
/**
 * @brief This header file contains the declarations of shared memory and semaphore related functions.
 * 
 */
#pragma once
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <map>

const int SHARED_MEMORY_KEY = 12345; // 共享内存的键
const int SEMAPHORE_KEY = 54321;    // 信号量的键
const int SHARED_MEMORY_SIZE = 2048; // 共享内存的大小

// 命令结构
struct Message {
    int userId;
    char message[1024];
};

// 信号量结构
union Semun {
    int val; // 信号量的值
    struct semid_ds* buf; // 用于IPC_STAT和IPC_SET的缓存区
    unsigned short* array; // 用于GETALL和SETALL的数组
};

