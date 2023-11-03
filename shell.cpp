#include <stdio.h>
#include <string.h>
#include <iostream>
#include <sys/wait.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <termios.h>
#include <sstream>
#include "share_memory.h"
#include <vector>
#include <iomanip>
#include <boost/algorithm/string.hpp>

#define SIZE 1024
using namespace std;
using namespace boost::algorithm;

std::vector<std::string> cut_command(std::string command) {
    vector<string> res;
    split(res, command, boost::is_any_of(" "));
    return res;
}

void show_help(){
    // 输出绿色字体
    std::cout << "\033[1;32m-------------------------Help-------------------------\033[0m" << std::endl;
    std::cout << std::left << std::setw(15) << "Command";
    std::cout << std::setw(15) << "Description" << std::endl;
    std::cout << std::setw(15) << "info";
    std::cout << std::setw(15) << "Show the information of the filesystem" << std::endl;
    std::cout << std::setw(15) << "cd";
    std::cout << std::setw(15) << "Change the current directory" << std::endl;
    std::cout << std::setw(15) << "dir";
    std::cout << std::setw(15) << "List the files in the current directory" << std::endl;
    std::cout << std::setw(15) << "md";
    std::cout << std::setw(15) << "Make a new directory" << std::endl;
    std::cout << std::setw(15) << "rd";
    std::cout << std::setw(15) << "Remove a directory" << std::endl;
    std::cout << std::setw(15) << "newfile";
    std::cout << std::setw(15) << "Make a new file" << std::endl;
    std::cout << std::setw(15) << "del";
    std::cout << std::setw(15) << "Delete a file" << std::endl;
    std::cout << std::setw(15) << "cat";
    std::cout << std::setw(15) << "Show the content of a file" << std::endl;
    std::cout << std::setw(15) << "copy";
    std::cout << std::setw(15) << "Copy a file from host or local" << std::endl;
    std::cout << std::setw(15) << "write";
    std::cout << std::setw(15) << "Write a file" << std::endl;
    std::cout << std::setw(15) << "check";
    std::cout << std::setw(15) << "Check if the filesystem is OK" << std::endl;
    std::cout << std::setw(15) << "exit";
    std::cout << std::setw(15) << "Exit the system" << std::endl;
    std::cout << "\033[1;32m------------------------------------------------------\033[0m" << std::endl;
}

// 数字转字符串
std::string num_to_str(int num){
    std::stringstream ss;
    ss << num;
    std::string str = ss.str();
    return str;
}

int main() {
    // 创建或连接到共享内存
    int shmid = shmget(SHARED_MEMORY_KEY, SHARED_MEMORY_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1) {
        exit(1);
    }
    // 连接到共享内存
    char* shared_memory = (char*)shmat(shmid, (void*)0, 0);
    if (shared_memory == (char*)(-1)) {
        exit(1);
    }
    // 创建或连接到信号量
    int semid = semget(SEMAPHORE_KEY, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        exit(1);
    }

    // 显示欢迎界面
    cout << "-------------file system-------------" << endl;
    cout << endl;
    int userId;
    std::string password;
    std::string curr_path;
    bool is_login = false; // 是否已经登录
    bool is_writing = false; // 是否正在写文件
    while (true) {
        // cmd：共享内存
        struct Message* cmd = (struct Message*)shared_memory;
        char command[SIZE];
        memset(command, '\0', SIZE);
        if(!is_login){
            std::cout << "Please enter your user ID: ";
            std::cin >> userId;
            std::cin.ignore();
            cmd->userId = userId;
            std::cout << "Please enter your password: ";
            // 设置密码不可见
            struct termios original;
            tcgetattr(STDIN_FILENO, &original);
            struct termios no_echo = original;
            no_echo.c_lflag &= ~ECHO;
            tcsetattr(STDIN_FILENO, TCSANOW, &no_echo);
            // std::cin >> password;
            // std::cin.ignore();
            char ch; 
            while((ch = getchar()) != '\n'){
                password.push_back(ch);
                std::cout << '*' << std::flush;
            }
            tcsetattr(STDIN_FILENO, TCSANOW, &original);
            std::cout << std::endl;
            strcpy(command, "login");
            strncpy(cmd->message, command, sizeof(cmd->message));
        }
        else if(is_writing){
            // 输入文件内容，按ESC+Enter结束
            // 将“#writing ”写入command，用于判断是否正在写文件
            strcpy(command, "#writing ");
            int offset = 9;
            while(offset < SIZE){
                char input_line[SIZE];
                // memset(input_line, '\0', 512);
                std::cin.getline(input_line, sizeof(input_line));
                if(input_line[0] == 27){
                    break;
                }
                else{
                    // 将输入的内容写入到#writing后面
                    strncpy(command + offset, input_line, sizeof(input_line) - offset);
                    offset += strlen(input_line);
                    command[offset++] = '\n';
                }
            }
            // 向共享内存写入命令
            strncpy(cmd->message, command, sizeof(cmd->message));
            cmd->userId = userId;
        }else{
            cmd->userId = userId;
            std::cin.getline(command, sizeof(command));
            // 向共享内存写入命令
            // cmd->userId = userId;
            strncpy(cmd->message, command, sizeof(cmd->message));
            cmd->userId = userId;
        }

        // 发送信号通知simdisk处理命令
        struct sembuf sops;
        sops.sem_num = 0;
        sops.sem_op = 1;
        sops.sem_flg = 0;
        semop(semid, &sops, 1);
        // 等待simdisk的回信
        sops.sem_op = -1;
        semop(semid, &sops, 1);
        string response = shared_memory + sizeof(struct Message);
        string prefix = "user" + num_to_str(userId) + "@PERKZ:~";
        // string response = cmd->message;
        // 从共享内存中读取simdisk的回信
        if(is_writing){
            // 刚写完文件，标记写入过程结束
            std::cout << response;
            std::cout << prefix << curr_path << "$ " << std::flush;
            is_writing = false;
        }
        else if(strcmp(command, "exit") == 0){
            // 退出文件系统
            std::cout << response;
            break;
        }
        else if(strcmp(command, "login") == 0){
            // 登录
            if(response == "You have already logged in!\n"){
                std::cout << response<< std::endl;
            }else{
                is_login = true;
                std::cout << response << std::endl;
                show_help();
                std::cout<<std::endl;
                std::cout << prefix << "$ ";
            }
        }
        else if(cut_command(command)[0] == "write"){
            // 写文件
            std::cout << response << std::flush;
            if(response == "Please input the content you want to write into the file. Press 'ESC + Enter' to finish.\n"){
                is_writing = true;
            }
            else{
                is_writing = false;
                std::cout << prefix << curr_path << "$ " << std::flush;
            }
        }
        else if(cut_command(command)[0] == "cd"){
            // 切换目录，用于改变curr_path
            string temp_path = response;
            if(temp_path[0] == '/'){
                std::cout << prefix << response << "$ "<< std::flush;
                curr_path = temp_path;
            }
            else if (temp_path == ""){
                std::cout << prefix << "$ "<< std::flush;
                curr_path.clear();
            }
            else{
                std::cout  << response;
                std::cout << prefix << curr_path << "$ " << std::flush;
            }
        }
        else{
            std::cout << response;
            std::cout << prefix << curr_path << "$ " << std::flush;
        }
    }
    // 分离共享内存
    shmdt(shared_memory);
    return 0;
}