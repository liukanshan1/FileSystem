#include "utils.hpp"

using namespace std;
using namespace boost::algorithm;
using namespace boost::interprocess;

int main() {
    // 创建或连接到共享内存
    managed_shared_memory shm(open_or_create, "FileSystemMessage", 32000);
    // 创建或连接到信号量
    named_semaphore sig(open_or_create, "msg_signal", 0);

    // 显示欢迎界面
    cout << "-------------file system-------------" << endl;
    int userId;
    string curr_path;
    bool is_writing = false; // 是否正在写文件
    Message message = Message();
    char command[SIZE];
    memset(command, '\0', SIZE);
    string user_id;
    string prefix;

    // login
    cout << "Login as (Positive User ID): ";
    cin >> userId;
    user_id = num_to_str(userId);
    // 向共享内存写入命令
    strncpy(command, "login", sizeof(command));
    strncpy(message.message, command, sizeof(message.message));
    message.userId = userId;
    // send message
    auto msg = shm.find_or_construct<Message>("msg0")(message);
    auto vmsg = shm.find_or_construct<bool>("vmsg0")(true);
    *msg = message;
    *vmsg = true;
    // 发送信号通知simdisk处理命令
    sig.post();

    // 等待simdisk的回信
    prefix = "shell";
    ipcdetail::char_ptr_holder<char> shellid = (prefix + user_id).c_str();
    named_semaphore::remove(shellid);
    named_semaphore shell_sig(open_or_create, shellid, 0);
    shell_sig.wait();

    // 从共享内存中读取simdisk的回信
    prefix = "resp";
    Message resp_msg = Message();
    ipcdetail::char_ptr_holder<char> respid = (prefix + user_id).c_str();
    auto* resp = shm.find_or_construct<Message>(respid)(resp_msg);
    string response = resp->message;
    prefix = "user" + num_to_str(userId) + "@cyx:~";
    // 登录
    cout << response << endl;
    show_help();
    cout << endl;
    cout << prefix << "$ ";

    prefix = "vmsg";
    ipcdetail::char_ptr_holder<char> vmsgid = (prefix + user_id).c_str();
    vmsg = shm.find_or_construct<bool>(vmsgid)(true);
    prefix = "msg";
    ipcdetail::char_ptr_holder<char> msgid = (prefix + user_id).c_str();
    msg = shm.find_or_construct<Message>(msgid)(message);

    while (true) {
        memset(command, '\0', SIZE);
        if(is_writing){
            // 输入文件内容，按ESC+Enter结束
            // 将“#writing ”写入command，用于判断是否正在写文件
            strcpy(command, "#writing ");
            int offset = 9;
            while(offset < SIZE){
                char input_line[SIZE];
                // memset(input_line, '\0', 512);
                cin.getline(input_line, sizeof(input_line));
                if(input_line[0] == '$'){
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
            *vmsg = true;
            *msg = message;
        }
        else{
            cin.getline(command, sizeof(command));
            while(command[0] == '\0'){
                cin.getline(command, sizeof(command));
            }
            // 向共享内存写入命令
            strncpy(message.message, command, sizeof(message.message));
            *vmsg = true;
            *msg = message;
        }
        // 发送信号通知simdisk处理命令
        sig.post();

        // 等待simdisk的回信
        shell_sig.wait();
        // 从共享内存中读取simdisk的回信
        response = resp->message;
        prefix = "user" + num_to_str(userId) + "@cyx:~";
        if(is_writing){
            // 刚写完文件，标记写入过程结束
            cout << response;
            cout << prefix << curr_path << "$ " << flush;
            is_writing = false;
        }
        else if(strcmp(command, "exit") == 0){
            // 退出文件系统
            cout << response;
            named_semaphore::remove(shellid);
            break;
        }
        else if(cut_command(command)[0] == "write"){
            // 写文件
            cout << response << flush;
            if(response == "Input the content end with $\n"){
                is_writing = true;
            }
            else{
                is_writing = false;
                cout << prefix << curr_path << "$ " << flush;
            }
        }
        else if(cut_command(command)[0] == "cd"){
            // 切换目录，用于改变curr_path
            string temp_path = response;
            if(temp_path[0] == '/'){
                cout << prefix << response << "$ "<< flush;
                curr_path = temp_path;
            }
            else if (temp_path == ""){
                cout << prefix << "$ "<< flush;
                curr_path.clear();
            }
            else{
                cout  << response;
                cout << prefix << curr_path << "$ " << flush;
            }
        }
        else{
            cout << response;
            cout << prefix << curr_path << "$ " << flush;
        }
    }
    return 0;
}