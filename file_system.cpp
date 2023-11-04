#include "simdisk.hpp"
#include <sstream>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/sync/named_semaphore.hpp>
#include <boost/interprocess/containers/string.hpp>

using namespace std;
using namespace boost;

vector<User> user_record;// 用户记录

void remove_user(int userId) {
    for (auto it = user_record.begin(); it != user_record.end(); it++) {
        if (it->userId == userId) {
            user_record.erase(it);
            break;
        }
    }
}

bool is_login(int userId) {
    for (auto user:user_record) {
        if (user.userId == userId) {
            return true;
        }
    }
    return false;
}

string apply_cmd(const string& command, int userId) {
    string response;
    std::vector<std::string> args = cut_command(command);
    if (args.size() < 2) {
        args.emplace_back("");
    }
    if (args[0] == "info") {
        response = info();
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
//    else if (args[0].empty()) continue;
    else {
        response = args[0] + ": command not found\n";
        cout << args[0] << ": command not found" << endl;
    }
    return response;
}

int main() {
    init();
    // 创建或连接到共享内存
    interprocess::shared_memory_object::remove("FileSystemMessage");
    interprocess::managed_shared_memory shm(interprocess::open_or_create, "FileSystemMessage", 32000);
    // 创建或连接到信号量
    interprocess::named_semaphore::remove("msg_signal");
    interprocess::named_semaphore sig(interprocess::open_or_create, "msg_signal", 0);

    while (true) {
       // 等待shell发送信号
        sig.wait();
        string response;

        // 从共享内存中读取命令
        // login or exit
        bool* vcmd = shm.find<bool>("vmsg0").first;
        if (*vcmd) {
            *vcmd = false;
            Message* cmd = shm.find<Message>("msg0").first;
            int userId = cmd->userId;
            std::string command = cmd->message;
            std::vector<std::string> args = cut_command(command);

            std::cout << "已登录用户: ";
            string s;
            string id = num_to_str(userId);
            if (!is_login(userId)) {
                std::cout << userId << " login successfully!" << std::endl;
                response = "Login successfully!\n";
                // 创建用户
                s = "vmsg";
                interprocess::ipcdetail::char_ptr_holder<char> vmsgid = (s + id).c_str();
                auto vcmd = shm.find_or_construct<bool>(vmsgid)(false);
                s = "msg";
                interprocess::ipcdetail::char_ptr_holder<char> msgid = (s + id).c_str();
                auto cmd = shm.find_or_construct<Message>(msgid)(default_msg);
                s = "resp";
                interprocess::ipcdetail::char_ptr_holder<char> respid = (s + id).c_str();
                auto* resp = shm.find_or_construct<Message>(respid)(default_msg);
                User user = User(userId, vcmd, cmd, resp);
                user_record.push_back(user);
                add_user_ptr(userId);
            } else {
                std::cout << userId << " have already logged in!" << std::endl;
                response = "You have already logged in!\n";
            }

            Message resp_msg = Message();
            strncpy(resp_msg.message, response.c_str(), sizeof(resp_msg.message));
            // 执行命令并将结果存入共享内存
            s = "resp";
            interprocess::ipcdetail::char_ptr_holder<char> respid = (s + id).c_str();
            auto* resp = shm.find_or_construct<Message>(respid)(resp_msg);
            *resp = resp_msg;
            // 发送信号通知shell回信已准备好
            s = "shell";
            interprocess::ipcdetail::char_ptr_holder<char> shellid = (s + id).c_str();
            interprocess::named_semaphore shell_sig(interprocess::open_or_create, shellid, 0);
            shell_sig.post();
            continue;
        }

        // 其他命令
        for(auto user:user_record){
            if(!*user.vcmd){
                continue;
            }
            *user.vcmd = false;
            Message* cmd = user.cmd;
            curr_user = cmd->userId;
            cout << "User " << curr_user << " : ";
            cout << "command: " << cmd->message << endl;

            response = apply_cmd(cmd->message, curr_user);

            // 执行命令并将结果存入共享内存
            Message resp_msg = Message();
            strncpy(resp_msg.message, response.c_str(), sizeof(resp_msg.message));
            // 执行命令并将结果存入共享内存
            *user.resp = resp_msg;

            // 发送信号通知shell回信已准备好
            string s = "shell";
            interprocess::ipcdetail::char_ptr_holder<char> shellid = (s + num_to_str(cmd->userId)).c_str();
            interprocess::named_semaphore shell_sig(interprocess::open_or_create, shellid, 0);
            shell_sig.post();
            break;
        }
    }
}