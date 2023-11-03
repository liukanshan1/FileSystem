/**
 * @brief This is the main function of the file system program. It initializes shared memory and semaphore, and then enters a loop to wait for commands from the shell. It executes the commands and stores the results in shared memory for the shell to retrieve.
 * 
 * @return int Returns 0 if the program runs successfully.
 */
#include "simdisk.h"
#include <termios.h>
#include <unistd.h>
#include <sstream>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <fcntl.h>
#include <string.h>

using namespace std;

vector<int> user_record;// 用户记录

bool check_exist(int userId){
    for(int i = 0; i < user_record.size(); i++){
        if(user_record[i] == userId){
            return true;
        }
    }
    return false;
}

void remove_user(int userId){
    for(int i = 0; i < user_record.size(); i++){
        if(user_record[i] == userId){
            user_record.erase(user_record.begin() + i);
            break;
        }
    }
}

int main() {
    init();
    // 创建或连接到共享内存
    cout.setf(ios::unitbuf);
    cout << "-------Welcome to Perkz's file system!------" << endl;
    cout << "----Type 'info' to get more information.----" << endl;
    int shmid = shmget(SHARED_MEMORY_KEY, SHARED_MEMORY_SIZE, 0666 | IPC_CREAT);
    if (shmid == -1) {
        perror("shmget");
        exit(1);
    }

    // 连接到共享内存
    char* shared_memory = (char*)shmat(shmid, (void*)0, 0);
    if (shared_memory == (char*)(-1)) {
        perror("shmat");
        exit(1);
    }

    // 创建或连接到信号量
    int semid = semget(SEMAPHORE_KEY, 1, 0666 | IPC_CREAT);
    if (semid == -1) {
        perror("semget");
        exit(1);
    }

    while (true) {
        // 等待shell发送信号
        struct sembuf sops;
        sops.sem_num = 0;
        sops.sem_op = -1;
        sops.sem_flg = 0;
        semop(semid, &sops, 1);
        string response;
        // 从共享内存中读取命令
        struct Message* cmd = (struct Message*)shared_memory;
        int userId = cmd->userId;
        curr_user = userId;
        std::cout << "User " << userId << " : ";
        std::cout << "command: " << cmd->message << std::endl;
        std::string command = cmd->message;
        std::vector<std::string> args = cut_command(command);
        if (args.size() < 2) {
            args.push_back("");
        }
        if (args[0] == "info") {
            response = info();
        } 
        else if(args[0] == "login"){
            std::cout << "已登录用户: ";
            if(user_record.size() == 0){
                std::cout << userId << std::endl; 
                std::cout << userId << " login successfully!" << std::endl;               
                response = "Login successfully!\n";
                user_record.push_back(userId);
                add_user_ptr(userId);
            }else if(check_exist(userId)){
                std::cout << userId << " have already logged in!" << std::endl;
                response = "You have already logged in!\n";
            }
            else{
                std::cout << std::endl;
                std::cout << userId << " login successfully!" << std::endl;
                response = "Login successfully!\n";
                user_record.push_back(userId);
                add_user_ptr(userId);
            }
            for(int i = 0; i < user_record.size(); i++){
                std::cout << user_record[i] << "  ";
            }
            std::cout << std::endl;
        }
        else if (args[0] == "write"){
            std::cout << "write" << std::endl;
            response = write_check(args[1]);
        }
        else if(args[0] == "#writing"){
            std::cout << "#writing" << std::endl;
            std::string context = command.substr(9);
            response = write(context);
        }
        else if (args[0] == "cd") {
            if (args.size() == 2) {
                response = cd(args[1]);
            } 
            else {
                response = "cd: too many arguments\n";
                std::cout << "cd: too many arguments" << std::endl;
            }
        } 
        else if(args[0] == "help"){
            response = help();
        }
        else if (args[0] == "dir") {
            if (args.size() == 2 && args[1] != "/s") {
                response = dir(args[1]);
            }
            else {
                response = "dir: too many arguments\n";
                std::cout << "dir: too many arguments" << std::endl;
            }
        } 
        else if (args[0] == "md") {
            for (int i = 1; i < (int)args.size(); i++) {
                response += md(args[i]);
            }
        }
        else if (args[0] == "rd") {
            for (int i = 1; i < (int)args.size(); i++) {
                response += rd(args[i]);
            }
        } 
        else if (args[0] == "newfile") {
            for (int i = 1; i < (int)args.size(); i++) {
                response += newfile(args[i]);
            }
        } 
        else if (args[0] == "cat") {
            if (args.size() == 2) {
                response = cat(args[1]);
                response += "\n";
            } else {
                std::cout << "cat: too many arguments" << std::endl;
                response = "cat: too many arguments\n";
            }
        } 
        else if (args[0] == "copy") {
            response = copy(args[1], args[2]);
        }
        else if (args[0] == "del") {
            for (int i = 1; i < (int)args.size(); i++) {
                response += del(args[i]);
            }
        }
        else if (args[0] == "exit") {
            std::cout << userId << " exit successfully!" << std::endl;
            response = "exit successfully!\n";
            remove_user(userId);
            remove_user_ptr(userId);
        } 
        else if (args[0].empty()) continue;
        else {
            response = args[0] + ": command not found\n";
            cout << args[0] << ": command not found" << endl;
        }
        // 执行命令并将结果存入共享内存
        // std::string response = executeCommand(command);
        strncpy(shared_memory + sizeof(struct Message), response.c_str(), SHARED_MEMORY_SIZE - sizeof(struct Message));
        // 发送信号通知shell回信已准备好
        sops.sem_op = 1;
        semop(semid, &sops, 1);
    }

    // 分离共享内存
    shmdt(shared_memory);

    return 0;
}