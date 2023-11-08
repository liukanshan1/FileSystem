#include <sstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <bitset>
#include <sys/stat.h>
#include <vector>
#include <fstream>
#include <cstring>
#include <cstdlib>
#include <sys/ipc.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "utils.hpp"

#define BLOCK_SIZE 1024

int curr_user = 0;// 当前用户
Dir* last_dir;// 上一次所在目录
Dir* curr_dir;// 根目录
Dir* root_dir;// 根目录
int bit_map[100 * BLOCK_SIZE] = {0};// 位图
map<string, int> file_lock; // 文件锁
map<int, string> file_lock_name; // 文件锁 用户名-文件名
map<int, int> file_lock_seq; // 文件锁 用户名-文件块号
map<int, Dir*> user_dir; // 用户目录 用户名-目录指针

// 初始化
void init();
// 用户函数
void add_user_ptr(int userId);
void remove_user_ptr(int userId);
// 路径切分
string show_path();
vector<string> cut_path(string path);
Dir* get_ptr_by_path(string path);
// 对目录树进行操作
void load_dir(ifstream& file, Dir* dentry);
void del_dir(ofstream& file, Dir* dentry);
// 对位图进行操作inode读写
void set_busy(ofstream& file,unsigned int index);
void set_free(ofstream& file,unsigned int index);
void load_bit_map();
int find_block_for_inode();
int find_block_for_file();
// inode读写
Inode* init_inode(int type, string name, unsigned int parent, string creator);
Inode* read_inode(ifstream& file, int block_seq);
void write_inode(Inode* inode, unsigned int index);
Inode* del_child(Inode* parent, unsigned int child);
void add_child(Inode* parent, unsigned int child);
// block读写
void clean_block(unsigned int index);
string write_file_block(ofstream& disk,string content, unsigned int index);
string read_file_block(ifstream& disk, unsigned int index);
// 命令行操作
string info();
string cd(string path);
string dir(string arg);
string md(string name);
string rd(string name);
string newfile(string name);
string cat(string name);
string copy(string arg1, string arg2);
string del(string content);
string write(string name);
string write_check(string name); // 互斥处理
string help();
string check();

//初始化磁盘
void init() {
    ifstream disk("disk.bin", ios::binary | ios::in | ios::out);
    load_bit_map();
    last_dir = nullptr;
    Inode* root_inode = read_inode(disk, 101);
    if(root_inode->num_of_children == 0){
        // 如果根目录下没有子目录/文件，则初始化根目录
        curr_dir = new Dir();
        curr_dir->inode = new Inode();
        curr_dir->inode->type = 0;
        curr_dir->inode->size = 0;
        curr_dir->inode->block_seq = 101;
        curr_dir->inode->num_of_children = 0;
        for(int i = 0; i < 10; i++){
            curr_dir->inode->children[i] = 0;
        }
        for(int i = 0; i < 3; i++){
            curr_dir->inode->mode[i] = 0;
        }
        curr_dir->inode->parent = 0;
        curr_dir->inode->name = "~";
        curr_dir->inode->creator = "cyx";
        curr_dir->inode->last_modified = "2023-09-22 13:23:23";
        curr_dir->name = "~";
        write_inode(curr_dir->inode, curr_dir->inode->block_seq);
    }
    else{
        // 如果根目录下有子目录/文件，则从磁盘中读取根目录
        curr_dir = new Dir();
        curr_dir->inode = root_inode;
        curr_dir->name = root_inode->name;
        curr_dir->parent = nullptr;
        load_dir(disk, curr_dir);
    }
    disk.close();
    root_dir = curr_dir;
}

/*用户操作*/
void add_user_ptr(int userId){
    user_dir[userId] = root_dir;
}

void remove_user_ptr(int userId){
    user_dir.erase(userId);
}

/*inode读写*/
//初始化inode
Inode* init_inode(int type, string name, unsigned int parent, string creator) {
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    Inode* inode = new Inode();
    inode->type = type;
    inode->name = name;
    inode->size = 0;
    inode->block_seq = find_block_for_inode();
    inode->parent = parent;
    inode->num_of_children = 0;
    set_busy(file, inode->block_seq);
    file.close();
    for(int i = 0; i < 10; i++){
        inode->children[i] = 0;
    }
    for(int i = 0; i < 3; i++){
        inode->mode[i] = 0;
    }
    inode->creator = creator;
    inode->last_modified = get_current_time();
    return inode;
}

//将inode写入磁盘
void write_inode(Inode* inode,unsigned int block_seq){
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    file.seekp(BLOCK_SIZE*block_seq, ios::beg);
    int type = inode->type;
    file.write((char*)&type, sizeof(int));
    string name = inode->name;
    file.write(name.c_str(), 64);
    unsigned int size = inode->size;
    file.write((char*)&size, sizeof(unsigned int));
    file.write((char*)&block_seq, sizeof(unsigned int));
    unsigned int num_of_children = inode->num_of_children;
    file.write((char*)&num_of_children, sizeof(unsigned int));
    unsigned int parent = inode->parent;
    file.write((char*)&parent, sizeof(unsigned int));
    unsigned int children[10];
    for(int i = 0; i < 10; i++){
        children[i] = inode->children[i];
    }
    file.write((char*)&children, sizeof(unsigned int) * 10);
    unsigned int mode[3];
    for(int i = 0; i < 3; i++){
        mode[i] = inode->mode[i];
    }
    file.write((char*)&mode, sizeof(unsigned int) * 3);
    string creator = inode->creator;
    file.write(creator.c_str(), 64);
    string last_modified = inode->last_modified;
    file.write(last_modified.c_str(), 64);
    // 将位图对应位置置1
    // file.seekp(BLOCK_SIZE + block_seq * sizeof(int), ios::beg);
    // int busy = 1;
    // file.write((char*)&busy, sizeof(int));
    set_busy(file, block_seq);
    file.close();
}

//从磁盘读取inode
Inode* read_inode(ifstream& file, int block_seq){
    Inode* inode = new Inode;
    file.seekg(BLOCK_SIZE*block_seq, ios::beg);
    int type;
    file.read((char*)&type, sizeof(int));
    inode->type = type;
    char name[64];
    file.read(name, 64);
    inode->name = name;
    unsigned int size;
    file.read((char*)&size, sizeof(unsigned int));
    inode->size = size;
    file.read((char*)&block_seq, sizeof(unsigned int));
    inode->block_seq = block_seq;
    unsigned int num_of_children;
    file.read((char*)&num_of_children, sizeof(unsigned int));
    inode->num_of_children = num_of_children;
    unsigned int parent;
    file.read((char*)&parent, sizeof(unsigned int));
    inode->parent = parent;
    unsigned int children[10];
    file.read((char*)&children, sizeof(unsigned int) * 10);
    for(int i = 0; i < 10; i++){
        inode->children[i] = children[i];
    }
    unsigned int mode[3];
    file.read((char*)&mode, sizeof(unsigned int) * 3);
    for(int i = 0; i < 3; i++){
        inode->mode[i] = mode[i];
    }
    char creator[64];
    file.read(creator, 64);
    inode->creator = creator;
    char last_modified[64];
    file.read(last_modified, 64);
    inode->last_modified = last_modified;
    return inode;
}

//为inode添加子目录/文件
void add_child(Inode* parent, unsigned int child){
    for(int i = 0; i < 10; i++){
        if(parent->children[i] == 0){
            parent->children[i] = child;
            parent->num_of_children++;
            return;
        }
    }
    BOOST_LOG_TRIVIAL(debug)<<endl << "add_child error: children more than 10!" << endl;
}

//为inode删除子目录/文件
Inode* del_child(Inode* parent, unsigned int child){
    for(int i = 0; i < 10; i++){
        if(parent->children[i] == child){
            parent->children[i] = 0;
            parent->num_of_children--;
            Inode* inode = new Inode();
            *inode = *parent;
            return inode;
        }
    }
    return nullptr;
}

/*位图操作*/
//将位图对应位置置1，并将磁盘中的位图更新
void set_busy(ofstream& disk, unsigned int index) {
    bit_map[index] = 1;
    disk.seekp(BLOCK_SIZE + index * sizeof(int), ios::beg);
    int busy = 1;
    disk.write((char*)&busy, sizeof(int));
}

//将位图对应位置置0，并将磁盘中的位图更新
void set_free(ofstream& disk, unsigned int index) {
    bit_map[index] = 0;
    disk.seekp(BLOCK_SIZE + index * sizeof(int), ios::beg);
    int free = 0;
    disk.write((char*)&free, sizeof(int));
}
//从磁盘读取位图
void load_bit_map() {
    fstream disk = fstream("disk.bin", ios::binary | ios::in | ios::out);
    disk.seekg(BLOCK_SIZE, ios::beg);
    disk.read((char*)bit_map, sizeof(bit_map));
    disk.close();
}

//从位图中找到一个空闲块，用于存储inode
int find_block_for_inode() {
    for (int i = 102; i < 51251; ++i) {
        if (bit_map[i] == 0) {
            return i;
        }
    }
    return -1;
}

//从位图中找到一个空闲块，用于存储文件
int find_block_for_file(){
    for (int i = 51251; i < 102400; ++i) {
        if (bit_map[i] == 0) {
            return i;
        }
    }
    return -1;
}

/*目录树操作*/
// 由存储的inode生成同时包含目录和文件的树形结构
void load_dir(ifstream& file, Dir* dentry) {
    for (int i = 0; i < 10; ++i) {
        if (dentry->inode->children[i] != 0) {
            Inode* inode = read_inode(file, dentry->inode->children[i]);
            Dir* child = new Dir();
            child->inode = inode;
            child->name = inode->name;
            child->parent = dentry;
            dentry->files[inode->name] = child;
            if (inode->type == 0) {
                load_dir(file, child);
            }
        }
    }
}

//递归删除目录树
void del_dir(ofstream& file, Dir* dentry) {
    // ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    for (auto it = dentry->files.begin(); it != dentry->files.end(); ++it) {
        if (it->second->inode->type == 1) {
            clean_block(it->second->inode->block_seq);
            for(int i = 0; i < 10; i++){
                if(it->second->inode->children[i] != 0){
                    clean_block(it->second->inode->children[i]);
                    set_free(file, it->second->inode->children[i]);
                }
            }
            set_free(file, it->second->inode->block_seq);
            // del_child(dentry->inode, it->second->inode->block_seq);
            write_inode(del_child(dentry->inode, it->second->inode->block_seq), dentry->inode->block_seq);
            it->second = nullptr;
        }    
    }
    for (auto it = dentry->files.begin(); it != dentry->files.end(); ++it) {
        if (it->second) {
            del_dir(file, it->second);
        }
    }
    clean_block(dentry->inode->block_seq);
    set_free(file, dentry->inode->block_seq);
    // del_child(dentry->parent->inode, dentry->inode->block_seq);
    write_inode(del_child(dentry->parent->inode, dentry->inode->block_seq), dentry->parent->inode->block_seq);
    // file.close();
    dentry->files.clear();
    delete dentry->inode;
    delete dentry;
}

/*路径操作*/
//输出当前路径
string show_path() {
    Dir* curr_dir = user_dir[curr_user];
    string path_return = "/";
    Dir* temp = curr_dir;
    vector<string> path;
    while (temp != nullptr) {
        path.push_back(temp->name);
        temp = temp->parent;
    }
    BOOST_LOG_TRIVIAL(debug)<<endl << "/";
    int i = (int)path.size() - 2;
    for (; i >= 1; --i) {
        BOOST_LOG_TRIVIAL(debug)<<endl << path[i] << "/";
        path_return += path[i];
        path_return += "/";
    }
    if (i >= 0) {
        BOOST_LOG_TRIVIAL(debug)<<endl << path[i];
        path_return += path[i];
    }
    return path_return;
}

//切分路径
vector<string> cut_path(string path) {
    vector<string> res;
    string curr_path;
    for (int i = 0; i < (int)path.size(); ++i) {
        if (path[i] != '/' && path[i] != '>') {
            curr_path.push_back(path[i]);
        }
        else if(path[i] == '>'){
            curr_path.push_back(path[i]);
            res.push_back(curr_path);
            curr_path.clear();
        } 
        else {
            res.push_back(curr_path);
            curr_path.clear();
        }
    }
    if (!curr_path.empty()) res.push_back(curr_path);
    return res;
}

//根据输入的path，返回指向对应的目录的指针
Dir* get_ptr_by_path(string path) {
    Dir* curr_dir = user_dir[curr_user];
    if(path.empty()) return nullptr;
    if(path == "~") {
        return root_dir;
    }
    //此函数的功能为：根据输入的path，返回指向对应的目录的指针
    if (path[0] == '/') {
        //绝对路径
        Dir* absolute_path = root_dir;
        vector<string> splitpath = cut_path(path);
        for (int i = 1; i < (int)splitpath.size(); i++) {
            if (absolute_path->files.find(splitpath[i]) == absolute_path->files.end()) {
                return nullptr;
            }
            absolute_path = absolute_path->files[splitpath[i]];
        }
        return absolute_path;
    } else {
        //相对路径
        Dir* relative_path = curr_dir;
        vector<string> splitpath = cut_path(path);
        for (int i = 0; i < (int)splitpath.size(); ++i) {
            if (splitpath[i] == ".") continue;
            else if (splitpath[i] == "..") {
                if (relative_path == root_dir) {
                    return nullptr;
                } else {
                    relative_path = relative_path->parent;
                }
            }
            else if (relative_path->files.find(splitpath[i]) == relative_path->files.end()) {
                return nullptr;
            }
            else if (relative_path->files.find(splitpath[i]) != relative_path->files.end()) {
                relative_path = relative_path->files[splitpath[i]];
            }
        }
        return relative_path;
    }
    return nullptr;
}

/*块操作*/
//根据块号清空块
void clean_block(unsigned int index) {
    fstream disk = fstream("disk.bin", ios::binary | ios::in | ios::out);
    disk.seekp(BLOCK_SIZE * index, ios::beg);
    char buf[BLOCK_SIZE] = {0};
    memset(buf, 0, BLOCK_SIZE);
    disk.write(buf, BLOCK_SIZE);
    disk.close();
}

//将content写入磁盘中的index块
string write_file_block(ofstream& disk,string content, unsigned int index) {
    if(content.size() > BLOCK_SIZE){
        BOOST_LOG_TRIVIAL(debug)<<endl << "write_file_block error: content size more than max size of file!" << endl;
        return "write_file_block error: content size more than max size of file!\n";
    }
    disk.seekp(BLOCK_SIZE * index, ios::beg);
    disk.write(content.c_str(), content.size());
    //删去file_lock中对应的锁
    file_lock.erase(file_lock_name[curr_user]);
    file_lock_name.erase(curr_user);
    file_lock_seq.erase(curr_user);
    return "write: successfully write into file\n";
}

//从磁盘中的index块读取文件存储内容
string read_file_block(ifstream& disk, unsigned int index){
    disk.seekg(BLOCK_SIZE * index, ios::beg);
    char buf[BLOCK_SIZE] = {0};
    disk.read(buf, BLOCK_SIZE);
    string content(buf, disk.gcount());
    return content;
}

/*功能函数*/
string info() {
    BOOST_LOG_TRIVIAL(debug)<<endl << "info" << endl;
    stringstream output;
    output << right << "Filesystem";
    output << setw(12) << "1k-blocks";
    output << setw(7) << "Used";
    output << setw(12) << "Available";
    output << setw(7) << "Use%";
    output << setw(12) << "Mounted on" << endl;
    output << setw(10) << "simdisk";
    output << setw(12) << "102400";
    output << setw(7) << "0";
    output << setw(12) << "102400";
    output << setw(7) << "0%";
    output << setw(12) << "/home" << endl;
    string response = output.str(); // 使用 str() 方法获取字符串
    BOOST_LOG_TRIVIAL(debug)<<endl << response << flush;
    return response;
}

string cd(string path = "") {
    stringstream output;
    string response;
    if (path.empty()) path = "/";
    if (path == "-") {
        if (last_dir == nullptr) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "cd: OLDPWD not set" << endl;
            response = "cd: OLDPWD not set\n";
            return response;
        }
        Dir* temp_dir = user_dir[curr_user];
        user_dir[curr_user] = last_dir;
        last_dir = temp_dir;
        response += show_path();
        BOOST_LOG_TRIVIAL(debug)<<endl << endl;
        return response;
    }
    if(path == "~") {
        user_dir[curr_user] = root_dir;
        return "";
    }
    Dir* cd_directory = get_ptr_by_path(path);
    if (cd_directory) {
        last_dir = user_dir[curr_user];
        if (cd_directory->inode->creator != num_to_str(curr_user)){
            response += "No Permission!!!\n";
            BOOST_LOG_TRIVIAL(debug)<< response << endl;
            return response;
        }
        user_dir[curr_user] = cd_directory;
        response += show_path();
        BOOST_LOG_TRIVIAL(debug)<< response << endl;
        return response;
    } else {
        output << "cd: " << "No such file or directory called '"<< path << "'" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<< response << endl;
        return response;
    }
}

string dir(string arg) {
    Dir* curr_dir = user_dir[curr_user];
    stringstream output;
    output<<left;
    string response;
    if (arg.empty()) {
        if(curr_dir->files.begin() == curr_dir->files.end()) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "Nothing in this dir." << endl;
            response = "Nothing in this dir.\n";
            return response;
        }
        if(curr_dir->name == "~") {
            output << setw(8) << "." << setw(6)<< curr_dir->inode->creator;
            output << int_to_hex(curr_dir->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->inode->last_modified << endl;
        }
        else {
            output << setw(8) << "." << setw(6)<< curr_dir->inode->creator;
            output << int_to_hex(curr_dir->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->inode->last_modified << endl;
            output << setw(8) << ".." << setw(6)<< curr_dir->inode->creator; 
            output << int_to_hex(curr_dir->parent->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->parent->inode->last_modified << endl;
        }
        for (auto it = curr_dir->files.begin(); it != curr_dir->files.end(); ++it) {
            output << setw(8)<< it->first << setw(6)<< it->second->inode->creator; 
            output << int_to_hex(it->second->inode->block_seq * BLOCK_SIZE) << "  " << it->second->inode->last_modified << endl;
        }
        // if (!curr_dir->files.empty()) { 
        //     output << endl;
        // }
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    // 完成指定路径下的文件列表显示
    Dir* dir_directory = get_ptr_by_path(arg);
    if (dir_directory == nullptr) {
        output << "dir: cannot access '" << arg << "': No such file or directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    if (dir_directory->inode->type == 1) {
        output << "dir: cannot access '" << arg << "': Not a directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    if(dir_directory->files.begin() == dir_directory->files.end()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "Nothing in this dir." << endl;
        response = "Nothing in this dir.\n";
        return response;
    }
    if(dir_directory->name == "~") {
        output << setw(8) << "." << setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->inode->last_modified << endl;
    }
    else {
        output << setw(8) << "." << setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->inode->last_modified << endl;
        output << setw(8) << ".." << setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->parent->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->parent->inode->last_modified << endl;
    }
    for (auto it = dir_directory->files.begin(); it != dir_directory->files.end(); ++it) {
        output << setw(8)<< it->first << setw(6)<< it->second->inode->creator; 
        output << int_to_hex(it->second->inode->block_seq * BLOCK_SIZE) << "  " << it->second->inode->last_modified << endl;
    }
    // if (!dir_directory->files.empty()) {
    //     output << endl;
    // }
    response = output.str();
    BOOST_LOG_TRIVIAL(debug)<<endl <<response;
    return response;
}

string md(string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    stringstream output;
    string response;
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "md: missing operand" << endl;
        response = "md: missing operand\n";
        return response;
    }
    //切分name，得到父目录和新建目录名
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string new_dir_name;
    Dir* parent_directory = nullptr;
    //如果name是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            //name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            //name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    new_dir_name = splitpath[splitpath.size() - 1];
    //检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        output << "md: cannot create directory '" << parent_path << "': No such directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "md: cannot create directory '" << parent_path << "': Not a directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    //检查new_dir_name是否已存在
    if (parent_directory->files.find(new_dir_name) != parent_directory->files.end()) {
        output << "md: cannot create directory '" << name << "': Dir exists" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl <<response;
        return response;
    }
    Inode* inode = init_inode(0, new_dir_name, parent_directory->inode->block_seq, num_to_str(curr_user));
    write_inode(inode, inode->block_seq);
    // parent_directory->inode->children[parent_directory->inode->num_of_children] = inode->block_seq;
    // parent_directory->inode->num_of_children++;
    add_child(parent_directory->inode, inode->block_seq);
    write_inode(parent_directory->inode, parent_directory->inode->block_seq);
    Dir* dentry = new Dir();
    dentry->inode = inode;
    dentry->name = new_dir_name;
    dentry->parent = parent_directory;
    parent_directory->files[new_dir_name] = dentry;
    output << "md: create directory '" << name << "'" << endl;
    response = output.str();
    BOOST_LOG_TRIVIAL(debug)<<endl <<response;
    return response;
}

string rd(string name) {
    Dir* curr_dir = user_dir[curr_user];
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    stringstream output;
    string response;
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "rd: missing operand" << endl;
        response = "rd: missing operand\n";
        return response;
    }
    //切分name，得到父目录和要删除的目录名字
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string del_dir_name;
    Dir *parent_directory = nullptr;
    // 如果是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            //name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            //name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    del_dir_name = splitpath[splitpath.size() - 1];
    //检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        output << "rd: cannot remove directory '" << parent_path << "': No such file or directory '" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "rd: cannot remove directory '" << parent_path << "': Not a directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    //检查del_dir_name是否存在
    if(parent_directory->files.find(del_dir_name) == parent_directory->files.end()) {
        output << "rd: cannot remove directory '" << name << "': No such directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    //检查del_dir_name是否为目录
    if(parent_directory->files[del_dir_name]->inode->type != 0) {
        output << "rd: cannot remove directory '" << name << "': Not a directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    del_dir(file, parent_directory->files[del_dir_name]);
    parent_directory->files.erase(del_dir_name);
    file.close();
    output << "rd: remove directory '" << name << "'" << endl;
    response = output.str();
    BOOST_LOG_TRIVIAL(debug)<<endl << response;
    return response;
}

string newfile(string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    stringstream output;
    string response;
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "newfile: missing operand" << endl;
        response = "newfile: missing operand\n";
        return response;
    } 
    //切分name，得到父目录和新建目录名
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string new_dir_name;
    Dir *parent_directory = nullptr;
    //如果name是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            //name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            //name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    new_dir_name = splitpath[splitpath.size() - 1];
    //检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        output << "newfile: cannot create directory '" << parent_path << "': No such directory '" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "newfile: cannot create directory '" << parent_path << "': Not a directory" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    //检查new_dir_name是否已存在
    if (parent_directory->files.find(new_dir_name) != parent_directory->files.end()) {
        output << "newfile: cannot create directory '" << name << "': File exists" << endl;
        response = output.str();
        BOOST_LOG_TRIVIAL(debug)<<endl << response;
        return response;
    }
    Inode* inode = init_inode(1, new_dir_name, parent_directory->inode->block_seq, num_to_str(curr_user));
    write_inode(inode, inode->block_seq);
    parent_directory->inode->children[parent_directory->inode->num_of_children] = inode->block_seq;
    parent_directory->inode->num_of_children++;
    write_inode(parent_directory->inode, parent_directory->inode->block_seq);
    Dir* dentry = new Dir();
    dentry->inode = inode;
    dentry->name = new_dir_name;
    dentry->parent = parent_directory;
    parent_directory->files[new_dir_name] = dentry;
    output << "newfile: create file '" << name << "'" << endl;
    response = output.str();
    BOOST_LOG_TRIVIAL(debug)<<endl << response;
    return response;
}

string cat(string name) {
    Dir* curr_dir = user_dir[curr_user];
    string response;
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "cat: missing operand" << endl;
        return "cat: missing operand\n";
    }
    // //检查该文件是否正在被写入
    // if(file_lock.find(name) != file_lock.end()){
    //     BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot write file '" << name << "': File is being written" << endl;
    //     return "write: cannot write file '" + name + "': File is being written\n";
    // }
    //切分name，得到父目录和要删除的文件名字
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string cat_file_name;
    Dir *parent_directory = nullptr;
    //如果name是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            //name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            //name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    cat_file_name = splitpath[splitpath.size() - 1];
    //检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "cat: cannot write file '" << parent_path << "': No such directory" << endl;
        return "cat: cannot write file '" + parent_path + "': No such directory\n";
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "cat: cannot write file '" << parent_path << "': Not a directory" << endl;
        return "cat: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    //检查cat_file_name是否存在
    if(parent_directory->files.find(cat_file_name) == parent_directory->files.end()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "cat: cannot remove file '" << name << "': No such file" << endl;
        return "cat: cannot remove file '" + name + "': No such file\n";
    }
    //检查cat_file_name是否为文件
    if(parent_directory->files[cat_file_name]->inode->type != 1) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "cat: cannot write file '" << name << "': Not a file" << endl;
        return "cat: cannot write file '" + name + "': Not a file\n";
    }
    ifstream file("disk.bin", ios::binary | ios::in | ios::out);
    string content = read_file_block(file, parent_directory->files[cat_file_name]->inode->children[0]);
    content += '\n';
    BOOST_LOG_TRIVIAL(debug)<<endl<< content <<flush;
    return content;
}

string copy(string arg1 = "", string arg2 = "") {
    Dir* curr_dir = user_dir[curr_user];
    if (arg1.empty() || arg2.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "copy: missing operand" << endl;
        return "copy: missing operand\n";
    }
    //拷贝文件，除支持模拟Linux文件系统内部的文件拷贝外，还支持host文件系统与模拟Linux文件系统间的文件拷贝，
    //host文件系统的文件命名为<host>…，如：将windows下D：盘的文件\data\sample\test.txt文件拷贝到模拟Linux文件系统中的/test/data目录，
    //windows下D：盘的当前目录为D：\data，则使用命令：
    //simdisk copy <host>D：\data\sample\test.txt /test/data
    //或者：simdisk copy <host>D：sample\test.txt /test/data 
    //实现host文件系统与模拟Linux文件系统间的文件拷贝
    if(arg1[0] = '<'){
        //被复制的文件来自host文件系统，路径以<host>开头
        //检查arg1在host文件系统中是否存在
        string host_path;
        vector<string> splitpath = cut_path(arg1);
        //如果输入的不是<host>
        if(splitpath[0] != "<host>"){
            BOOST_LOG_TRIVIAL(debug)<<endl << "Wrong input for '"<< splitpath[0] << "'. Do you mean '<host>' ?" <<endl;
            return "Wrong input for '" + splitpath[0] + "'. Do you mean '<host>' ?\n";
        }
        for(int i = 1; i < (int)splitpath.size(); i++){
            host_path += splitpath[i];
            if(i != (int)splitpath.size() - 1) host_path += "/";
        }
        FILE* fp = fopen(host_path.c_str(), "r");
        if(fp == NULL){
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg1 << "': No such file or directory in <host>" << endl;
            return "copy: cannot stat '" + arg1 + "': No such file or directory in <host>\n";
        }else{
            fclose(fp);
        }
        //分割arg1，得到文件名
        string filename;
        filename = splitpath[splitpath.size() - 1];
        //检查arg2是否存在
        Dir* paste_directory = get_ptr_by_path(arg2);
        if (paste_directory == nullptr) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg2 << "': No such directory" << endl;
            return "copy: cannot stat '" + arg2 + "': No such directory\n";
        }
        else{
            //分割arg2，得到文件被复制到的最后一级目录名path
            string path;
            vector<string> splitpath2 = cut_path(arg2);
            path = splitpath2[splitpath2.size() - 1];
            //检查path是否为目录
            if(paste_directory->inode->type != 0) {
                BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg2 << "': Not a directory" << endl;
                return "copy: cannot stat '" + arg2 + "': Not a directory\n";
            }
            //在path目录下创建一个新文件，文件名为filename
            Inode* inode = new Inode();
            inode->type = 1;
            inode->size = 0;
            Dir* dentry = new Dir();
            dentry->inode = inode;
            dentry->name = filename;
            dentry->parent = paste_directory;
            paste_directory->files[filename] = dentry;
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: successfully copy '" << arg1 << "' to '" << arg2 << "'" << endl;
            return "copy: successfully copy '" + arg1 + "' to '" + arg2 + "'\n";
        }
    }
    else{
        //实现simdisk内部文件的copy
        //分割arg1，得到文件名
        string filename;
        vector<string> splitpath = cut_path(arg1);
        filename = splitpath[splitpath.size() - 1];
        //逐级检查arg1是否存在
        Dir* copy_directory = get_ptr_by_path(arg1);
        if (copy_directory == nullptr) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg1 << "': No such file or directory" << endl;
            return "copy: cannot stat '" + arg1 + "': No such file or directory\n";
        }
        //检查arg2是否存在
        Dir* paste_directory = get_ptr_by_path(arg2);
        if (paste_directory == nullptr) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg2 << "': No such directory" << endl;
            return "copy: cannot stat '" + arg2 + "': No such directory\n";
        }
        //分割arg2，得到文件被复制到的最后一级目录名path
        string path;
        vector<string> splitpath2 = cut_path(arg2);
        path = splitpath2[splitpath2.size() - 1];
        //检查path是否为目录
        if(paste_directory->inode->type != 0) {
            BOOST_LOG_TRIVIAL(debug)<<endl << "copy: cannot stat '" << arg2 << "': Not a directory" << endl;
            return "copy: cannot stat '" + arg2 + "': Not a directory\n";
        }
        //在path目录下创建一个新文件，文件名为filename
        Inode* inode = new Inode();
        inode->type = 1;
        inode->size = 0;
        Dir* dentry = new Dir();
        dentry->inode = inode;
        dentry->name = filename;
        dentry->parent = paste_directory;
        paste_directory->files[filename] = dentry;
        BOOST_LOG_TRIVIAL(debug)<<endl << "copy: successfully copy '" << arg1 << "' to '" << arg2 << "'" << endl;
        return "copy: successfully copy '" + arg1 + "' to '" + arg2 + "'\n";
    }
}

string del(string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "del: missing operand" << endl;
        return "del: missing operand\n";
    }
    //切分name，得到父目录和要删除的文件名字
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string del_file_name;
    Dir *parent_directory = nullptr;
    //如果name是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            //name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            //name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    del_file_name = splitpath[splitpath.size() - 1];
    //检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "del: cannot remove file '" << parent_path << "': No such directory" << endl;
        return "del: cannot remove file '" + parent_path + "': No such directory\n";
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "del: cannot remove file '" << parent_path << "': Not a directory" << endl;
        return "del: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    //检查del_file_name是否存在
    if(parent_directory->files.find(del_file_name) == parent_directory->files.end()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "del: cannot remove file '" << name << "': No such file" << endl;
        return "del: cannot remove file '" + name + "': No such file\n";
    }
    //检查del_file_name是否为文件
    if(parent_directory->files[del_file_name]->inode->type != 1) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "del: cannot remove file '" << name << "': Not a file" << endl;
        return "del: cannot remove file '" + name + "': Not a file\n";
    }
    //删除文件
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    clean_block(parent_directory->files[del_file_name]->inode->block_seq);
    write_inode(del_child(parent_directory->inode, parent_directory->files[del_file_name]->inode->block_seq), parent_directory->inode->block_seq);
    set_free(file, parent_directory->files[del_file_name]->inode->block_seq);
    file.close();
    delete parent_directory->files[del_file_name];
    parent_directory->files.erase(del_file_name);
    BOOST_LOG_TRIVIAL(debug)<<endl << "del: remove file '" << name << "'" << endl;
    return "del: remove file '" + name + "'\n";
}

string write_check(string name){
    Dir* curr_dir = user_dir[curr_user];
    string response;
    if (name.empty()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: missing operand" << endl;
        return "write: missing operand\n";
    }
    //检查该文件是否正在被写入
    if(file_lock.find(name) != file_lock.end()){
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot write file '" << name << "': File is being written" << endl;
        return "write: cannot write file '" + name + "': File is being written\n";
    }
    //切分name，得到父目录和要删除的文件名字
    vector<string> splitpath = cut_path(name);
    string parent_path;
    string del_file_name;
    Dir *parent_directory = nullptr;
    // 如果name是绝对路径
    if(name[0] == '/'){
        parent_path = "/";
        for(int i = 1; i < (int)splitpath.size() - 1; i++){
            parent_path += splitpath[i];
            if(i != (int)splitpath.size() - 2) parent_path += "/";
        }
        parent_directory = get_ptr_by_path(parent_path);
    }else{
        if((int)cut_path(name).size() == 1){
            // name是相对路径，且只有一级目录
            parent_path = curr_dir->name;
            parent_directory = curr_dir;
        }
        else{
            // name是相对路径，且有多级目录
            if(curr_dir->name == "~") parent_path = "/";
            else parent_path = curr_dir->name + "/";
            for(int i = 0; i < (int)splitpath.size() - 1; i++){
                parent_path += splitpath[i];
                if(i != (int)splitpath.size() - 2) parent_path += "/";
            }
            parent_directory = get_ptr_by_path(parent_path);
        }
    }
    del_file_name = splitpath[splitpath.size() - 1];
    // 检查parent_path是否存在
    // Dir* parent_directory = get_ptr_by_path(parent_path);
    if (parent_directory == nullptr) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot write file '" << parent_path << "': No such directory" << endl;
        return "write: cannot write file '" + parent_path + "': No such directory\n";
    }
    // 检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot write file '" << parent_path << "': Not a directory" << endl;
        return "write: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    // 检查del_file_name是否存在
    if(parent_directory->files.find(del_file_name) == parent_directory->files.end()) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot remove file '" << name << "': No such file" << endl;
        return "write: cannot remove file '" + name + "': No such file\n";
    }
    // 检查del_file_name是否为文件
    if(parent_directory->files[del_file_name]->inode->type != 1) {
        BOOST_LOG_TRIVIAL(debug)<<endl << "write: cannot write file '" << name << "': Not a file" << endl;
        return "write: cannot write file '" + name + "': Not a file\n";
    }
    // 将文件名和块号写入文件锁file_lock中
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    file_lock[name] = parent_directory->files[del_file_name]->inode->block_seq;
    file_lock_name[curr_user] = name;
    parent_directory->files[del_file_name]->inode->num_of_children++;
    parent_directory->files[del_file_name]->inode->children[0] = find_block_for_file();
    write_inode(parent_directory->files[del_file_name]->inode, parent_directory->files[del_file_name]->inode->block_seq);
    set_busy(file, parent_directory->files[del_file_name]->inode->children[0]);
    file_lock_seq[curr_user] = parent_directory->files[del_file_name]->inode->children[0];
    BOOST_LOG_TRIVIAL(debug)<<endl << "Input the content end with $" << endl;
    response = "Input the content end with $\n";
    return response;
}

string write(string content) {
    ofstream file("disk.bin", ios::binary | ios::in | ios::out);
    // 写入传回来的text，text中可能带有空格、换行符等
    string response;
    // 不需要检查text是否为空，因为write_check函数已经检查过了
    // 也不需要检查当前文件是否正在被写入，因为在shell中检查过了
    // 开始向文件中写入text
    response = write_file_block(file, content,file_lock_seq[curr_user]);
    file.close();
    BOOST_LOG_TRIVIAL(debug)<<endl << response << flush;
    return response;
}

string help(){
    stringstream output;
    output << "-------------------Help----------------------" << endl;
    output << left << setw(10) << "Command";
    output << setw(10) << "Description" << endl;
    output << setw(10) << "info";
    output << setw(10) << "Show information of the filesystem" << endl;
    output << setw(10) << "cd";
    output << setw(10) << "Change the current directory" << endl;
    output << setw(10) << "dir";
    output << setw(10) << "List the files in the directory" << endl;
    output << setw(10) << "md";
    output << setw(10) << "Make a new directory" << endl;
    output << setw(10) << "rd";
    output << setw(10) << "Remove a directory" << endl;
    output << setw(10) << "newfile";
    output << setw(10) << "Make a new file" << endl;
    output << setw(10) << "del";
    output << setw(10) << "Delete a file" << endl;
    output << setw(10) << "cat";
    output << setw(10) << "Show the content of a file" << endl;
    output << setw(10) << "copy";
    output << setw(10) << "Copy a file" << endl;
    output << setw(10) << "write";
    output << setw(10) << "Write a file" << endl;
    output << setw(10) << "check";
    output << setw(10) << "Check the filesystem" << endl;
    output << setw(10) << "exit";
    output << setw(10) << "Exit the filesystem" << endl;
    output << "---------------------------------------------" << endl;
    string response = output.str();
    BOOST_LOG_TRIVIAL(debug)<<endl<<endl << response;
    return response;
}

string check(){
    // 检查文件系统是否正常
    BOOST_LOG_TRIVIAL(debug)<<endl << "Everything is OK" << endl;
    return "Everything is OK\n";
}