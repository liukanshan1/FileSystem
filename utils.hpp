#include "iostream"
#include <cstring>
#include <vector>
#include <boost/algorithm/string.hpp>
#include <iomanip>
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

void show_help(){
    // 输出绿色字体
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