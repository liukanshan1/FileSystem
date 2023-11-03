#include <string.h>
#include <iostream>
#include <sstream>
#include "share_memory.h"
#include <vector>
#include <iomanip>
#include <boost/algorithm/string.hpp>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>

#define SIZE 1024
using namespace std;
using namespace boost::algorithm;
using namespace boost::interprocess;

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
    managed_shared_memory shm(open_or_create, "FileSystemMessage", 32000);
    // 创建或连接到信号量
    named_semaphore sig(open_or_create, "msg_signal", 0);

    // 显示欢迎界面
    cout << "-------------file system-------------" << endl;
    cout << endl;
    int userId;
    std::string curr_path;
    bool is_login = false; // 是否已经登录
    bool is_writing = false; // 是否正在写文件
    while (true) {
        Message message = Message();
        char command[SIZE];
        memset(command, '\0', SIZE);
        if(!is_login){
            std::cout << "Please enter your user ID: ";
            std::cin >> userId;
            // 向共享内存写入命令
            strncpy(message.message, command, sizeof(message.message));
            message.userId = userId;
            strncpy(command, "login", sizeof(command));
            // 创建或连接到信号量
            ipcdetail::char_ptr_holder<char> shellid = ("shell" + num_to_str(userId)).c_str();
            named_semaphore::remove(shellid);
            named_semaphore sig(open_or_create, shellid, 0);

            auto msg = shm.find_or_construct<Message>("msg0")(message);
            auto vmsg = shm.find_or_construct<bool>("vmsg0")(false);
            *msg = message;
            *vmsg = true;
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
            strncpy(message.message, command, sizeof(message.message));
            message.userId = userId;
            ipcdetail::char_ptr_holder<char> vmsgid = ("vmsg" + num_to_str(userId)).c_str();
            auto vmsg = shm.find_or_construct<bool>(vmsgid)(true);
            *vmsg = true;
            ipcdetail::char_ptr_holder<char> msgid = ("msg" + num_to_str(userId)).c_str();
            auto msg = shm.find_or_construct<Message>(msgid)(message);
            *msg = message;
        }
        else{
            //std::cin.getline(command, sizeof(command));
            cin>>command;
            // 向共享内存写入命令
            strncpy(message.message, command, sizeof(message.message));
            message.userId = userId;
            ipcdetail::char_ptr_holder<char> vmsgid = ("vmsg" + num_to_str(userId)).c_str();
            auto vmsg = shm.find_or_construct<bool>(vmsgid)(true);
            *vmsg = true;
            ipcdetail::char_ptr_holder<char> msgid = ("msg" + num_to_str(userId)).c_str();
            auto msg = shm.find_or_construct<Message>(msgid)(message);
            *msg = message;
        }
        // 发送信号通知simdisk处理命令
        sig.post();

        // 等待simdisk的回信
        ipcdetail::char_ptr_holder<char> shellid = ("shell" + num_to_str(userId)).c_str();
        named_semaphore shell_sig(open_or_create, shellid, 0);
        shell_sig.wait();

        // 从共享内存中读取simdisk的回信
        Message resp_msg = Message();
        ipcdetail::char_ptr_holder<char> respid = ("resp" + num_to_str(userId)).c_str();
        auto* resp = shm.find_or_construct<Message>(respid)(resp_msg);
        string response = resp->message;

        string prefix = "user" + num_to_str(userId) + "@cyx:~";
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
            ipcdetail::char_ptr_holder<char> shellid = ("shell" + num_to_str(userId)).c_str();
            named_semaphore::remove(shellid);
            break;
        }
        else if(strcmp(command, "login") == 0){
            is_login = true;
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
    return 0;
}