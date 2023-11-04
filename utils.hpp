#include "iostream"
#include <cstring>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <iomanip>
#include <chrono>
#include <map>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>

using namespace std;
using namespace boost;

#define SIZE 1024

// 命令结构
struct Message {
    int userId;
    char message[1024];
};

Message default_msg = Message{-1, ""};

struct Inode {
    int type;// 0: dir, 1: file
    std::string name;// 文件名
    unsigned int size;// 文件大小
    unsigned int block_seq;// 文件所在块号
    unsigned int num_of_children = 0;// 子目录/文件数量
    unsigned int parent;// 父目录块号
    unsigned int children[10];// 子目录/文件块号
    unsigned int mode[3];// 文件权限
    std::string creator;// 创建者
    std::string last_modified;// 最后修改时间
};

struct Dir {
    Inode* inode = nullptr;// 目录对应的inode
    std::string name;// 目录名
    Dir* parent = nullptr;// 父目录
    std::map<std::string, Dir*> files;// 子目录/文件
};

class User {
public:
    int userId;
    bool* vcmd;
    Message* cmd;
    Message* resp;
    //interprocess::named_semaphore* shell_sig;
    User(int userId, bool* vcmd, Message* cmd, Message* resp) {
        this->userId = userId;
        this->vcmd = vcmd;
        this->cmd = cmd;
        this->resp = resp;
        //this->shell_sig = shell_sig;
    }
};

std::string num_to_str(int num){
    return std::to_string(num);
}

vector<std::string> cut_command(string command) {
    vector<string> res;
    split(res, command, boost::is_any_of(" "));
    return res;
}

std::string int_to_hex(int num){
    // 将输入的int转化为8位16进制字符串
    std::stringstream stream;
    stream << std::hex << num;
    std::string result(stream.str());
    while(result.size() < 8){
        result = "0" + result;
    }
    return result;
}

std::string get_current_time() {
    // 获取当前时间点
    std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
    // 将时间点转换为时间结构
    std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm local_time = *std::localtime(&time);
    // 使用stringstream来格式化时间为字符串
    std::stringstream ss;
    ss << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void show_help(){
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
}