/**
 * simdisk.h
 * 
 * This header file defines the basic structures and functions for a simple file system simulation.
 * 
 * The file system is based on a tree structure, with each directory represented by a Dir struct and each file represented by an Inode struct.
 * 
 * The file system is stored in a binary file named "disk.bin".
 * 
 * The functions in this file include initialization, user management, path manipulation, directory tree operations, bit map operations, inode read/write operations, block read/write operations, and command line operations.
 * 
 * Author: Perkz
 * Date: 2023-10-07
 */

#ifndef SIMDISK_H_INCLUDED
#define SIMDISK_H_INCLUDED
#include <sstream>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <bitset>
#include <sys/stat.h>
#include <vector>
#include "share.h"
#include <fstream>

#define BLOCK_SIZE 1024

/*------------------------------------------------基本结构-------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

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

struct Superblock {
    int block_size = BLOCK_SIZE;
    int block_num = BLOCK_SIZE * 100;
    int inode_num = BLOCK_SIZE * 100; 
};

/*----------------------------------------------基本结构定义-----------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

int curr_user = 0;// 当前用户
Dir* last_dir;// 上一次所在目录
Dir* curr_dir;// 根目录
Dir* root_dir;// 根目录
int bit_map[100 * BLOCK_SIZE] = {0};// 位图
std::map<std::string, int> file_lock; // 文件锁
std::map<int, std::string> file_lock_name; // 文件锁 用户名-文件名
std::map<int, int> file_lock_seq; // 文件锁 用户名-文件块号
std::map<int, Dir*> user_dir; // 用户目录 用户名-目录指针
// std::fstream disk = std::fstream("disk.bin", std::ios::binary | std::ios::in | std::ios::out);

/*----------------------------------------------函数表------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

// 初始化
void init();

// 用户函数
void add_user_ptr(int userId);
void remove_user_ptr(int userId);

// 输出转化
std::string int_to_hex(int num);
std::string get_current_time();
std::string num_to_str(int num);

// 路径切分
std::string show_path();
std::vector<std::string> cut_path(std::string path);
Dir* get_ptr_by_path(std::string path);

// 对目录树进行操作
void load_dir(std::ifstream& file, Dir* dentry);
void del_dir(std::ofstream& file, Dir* dentry);

// 对位图进行操作inode读写
void set_busy(std::ofstream& file,unsigned int index);
void set_free(std::ofstream& file,unsigned int index);
void load_bit_map();
int find_block_for_inode();
int find_block_for_file();
void init_bit_map();

// inode读写
Inode* init_inode(int type, std::string name, unsigned int parent, std::string creator);
Inode* read_inode(std::ifstream& file, int block_seq);
void write_inode(Inode* inode, unsigned int index);
Inode* del_child(Inode* parent, unsigned int child);
void add_child(Inode* parent, unsigned int child);

// block读写
void clean_block(unsigned int index);
std::string write_file_block(std::ofstream& disk,std::string content, unsigned int index);
std::string read_file_block(std::ifstream& disk, unsigned int index);

// 命令行操作
std::string info();
std::string cd(std::string path);
std::string dir(std::string arg);
std::string md(std::string name);
std::string rd(std::string name);
std::string newfile(std::string name);
std::string cat(std::string name);
std::string copy(std::string arg1, std::string arg2);
std::string del(std::string content);
std::string write(std::string name);
std::string write_check(std::string name); // 互斥处理
std::string help();
std::string check();
void exit();

/*----------------------------------------------初始化-----------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 初始化磁盘
*/
void init() {
    std::ifstream disk("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
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
        curr_dir->inode->creator = "perkz";
        curr_dir->inode->last_modified = "2003-09-22 23:23:23";
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

/*-----------------------------------------------用户登记--------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

void add_user_ptr(int userId){
    user_dir[userId] = root_dir;
}

void remove_user_ptr(int userId){
    user_dir.erase(userId);
}

/*-----------------------------------------------输出转化--------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 将输入的int转化为8位16进制字符串
*/
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

/***
 * 获取当前时间
*/
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

std::string num_to_str(int num){
    std::stringstream ss;
    ss << num;
    std::string str = ss.str();
    return str;
}

/*----------------------------------------------inode读写--------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 初始化inode
*/
Inode* init_inode(int type, std::string name, unsigned int parent, std::string creator) {
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
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

/***
 * 将inode写入磁盘
*/
void write_inode(Inode* inode,unsigned int block_seq){
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    file.seekp(BLOCK_SIZE*block_seq, std::ios::beg);
    int type = inode->type;
    file.write((char*)&type, sizeof(int));
    std::string name = inode->name;
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
    std::string creator = inode->creator;
    file.write(creator.c_str(), 64);
    std::string last_modified = inode->last_modified;
    file.write(last_modified.c_str(), 64);
    // 将位图对应位置置1
    // file.seekp(BLOCK_SIZE + block_seq * sizeof(int), std::ios::beg);
    // int busy = 1;
    // file.write((char*)&busy, sizeof(int));
    set_busy(file, block_seq);
    file.close();
}

/***
 * 从磁盘读取inode
*/
Inode* read_inode(std::ifstream& file, int block_seq){
    Inode* inode = new Inode;
    file.seekg(BLOCK_SIZE*block_seq, std::ios::beg);
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

/***
 * 为inode添加子目录/文件
*/
void add_child(Inode* parent, unsigned int child){
    for(int i = 0; i < 10; i++){
        if(parent->children[i] == 0){
            parent->children[i] = child;
            parent->num_of_children++;
            return;
        }
    }
    std::cout << "add_child error: children more than 10!" << std::endl;
}

/***
 * 为inode删除子目录/文件
*/
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

/*---------------------------------------------位图操作----------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 将位图对应位置置1，并将磁盘中的位图更新
*/
void set_busy(std::ofstream& disk, unsigned int index) {
    bit_map[index] = 1;
    disk.seekp(BLOCK_SIZE + index * sizeof(int), std::ios::beg);
    int busy = 1;
    disk.write((char*)&busy, sizeof(int));
}

/***
 * 将位图对应位置置0，并将磁盘中的位图更新
*/
void set_free(std::ofstream& disk, unsigned int index) {
    bit_map[index] = 0;
    disk.seekp(BLOCK_SIZE + index * sizeof(int), std::ios::beg);
    int free = 0;
    disk.write((char*)&free, sizeof(int));
}

/***
 * 初始化位图
*/
void init_bit_map() {
    for (int i = 0; i < 102; ++i) {
        bit_map[i] = 1;
    }
}

/***
 * 从磁盘读取位图
*/
void load_bit_map() {
    std::fstream disk = std::fstream("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    disk.seekg(BLOCK_SIZE, std::ios::beg);
    disk.read((char*)bit_map, sizeof(bit_map));
    disk.close();
}

/***
 * 从位图中找到一个空闲块，用于存储inode
*/
int find_block_for_inode() {
    for (int i = 102; i < 51251; ++i) {
        if (bit_map[i] == 0) {
            return i;
        }
    }
    return -1;
}

/***
 * 从位图中找到一个空闲块，用于存储文件
*/
int find_block_for_file(){
    for (int i = 51251; i < 102400; ++i) {
        if (bit_map[i] == 0) {
            return i;
        }
    }
    return -1;
}

/*---------------------------------------------目录树操作---------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 由存储的inode生成同时包含目录和文件的树形结构
*/ 
void load_dir(std::ifstream& file, Dir* dentry) {
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

/***
 * 递归删除目录树
*/
void del_dir(std::ofstream& file, Dir* dentry) {
    // std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
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

/*---------------------------------------------路径相关----------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 输出当前路径
*/
std::string show_path() {
    Dir* curr_dir = user_dir[curr_user];
    std::string path_return = "/";
    Dir* temp = curr_dir;
    std::vector<std::string> path;
    while (temp != nullptr) {
        path.push_back(temp->name);
        temp = temp->parent;
    }
    std::cout << "/";
    int i = (int)path.size() - 2;
    for (; i >= 1; --i) {
        std::cout << path[i] << "/";
        path_return += path[i];
        path_return += "/";
    }
    if (i >= 0) {
        std::cout << path[i];
        path_return += path[i];
    }
    return path_return;
}

/***
 * 切分命令
*/
std::vector<std::string> cut_command(std::string command) {
    std::vector<std::string> res;
    std::string token;
    for (int i = 0; i < (int)command.size(); ++i) {
        if (command[i] != ' ') {
            token.push_back(command[i]);
        } else {
            if (!token.empty()) {
                res.push_back(token);
                token.clear();
            }
        }
    }
    if (!token.empty()) res.push_back(token);
    return res;
}

/***
 * 切分路径
*/
std::vector<std::string> cut_path(std::string path) {
    std::vector<std::string> res;
    std::string curr_path;
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

/***
 * 根据输入的path，返回指向对应的目录的指针
*/
Dir* get_ptr_by_path(std::string path) {
    Dir* curr_dir = user_dir[curr_user];
    if(path.empty()) return nullptr;
    if(path == "~") {
        return root_dir;
    }
    //此函数的功能为：根据输入的path，返回指向对应的目录的指针
    if (path[0] == '/') {
        //绝对路径
        Dir* absolute_path = root_dir;
        std::vector<std::string> splitpath = cut_path(path);
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
        std::vector<std::string> splitpath = cut_path(path);
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

/*----------------------------------------------块操作-----------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

/***
 * 根据块号清空块
*/
void clean_block(unsigned int index) {
    std::fstream disk = std::fstream("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    disk.seekp(BLOCK_SIZE * index, std::ios::beg);
    char buf[BLOCK_SIZE] = {0};
    memset(buf, 0, BLOCK_SIZE);
    disk.write(buf, BLOCK_SIZE);
    disk.close();
}

/***
 * 将content写入磁盘中的index块
*/
std::string write_file_block(std::ofstream& disk,std::string content, unsigned int index) {
    if(content.size() > BLOCK_SIZE){
        std::cout << "write_file_block error: content size more than max size of file!" << std::endl;
        return "write_file_block error: content size more than max size of file!\n";
    }
    disk.seekp(BLOCK_SIZE * index, std::ios::beg);
    disk.write(content.c_str(), content.size());
    //删去file_lock中对应的锁
    file_lock.erase(file_lock_name[curr_user]);
    file_lock_name.erase(curr_user);
    file_lock_seq.erase(curr_user);
    return "write: successfully write into file\n";
}

/***
 * 从磁盘中的index块读取文件存储内容
*/
std::string read_file_block(std::ifstream& disk, unsigned int index){
    disk.seekg(BLOCK_SIZE * index, std::ios::beg);
    char buf[BLOCK_SIZE] = {0};
    disk.read(buf, BLOCK_SIZE);
    std::string content(buf, disk.gcount());
    return content;
}

/*----------------------------------------------- 功能函数 --------------------------------------------------*/
/*----------------------------------------------------------------------------------------------------------*/

/***
 * info命令
*/
std::string info() {
    std::cout << "info" << std::endl;
    std::stringstream output;
    output << std::right << "Filesystem";
    output << std::setw(12) << "1k-blocks";
    output << std::setw(7) << "Used";
    output << std::setw(12) << "Available";
    output << std::setw(7) << "Use%";
    output << std::setw(12) << "Mounted on" << std::endl;
    output << std::setw(10) << "simdisk";
    output << std::setw(12) << "102400";
    output << std::setw(7) << "0";
    output << std::setw(12) << "102400";
    output << std::setw(7) << "0%";
    output << std::setw(12) << "/home" << std::endl;
    std::string response = output.str(); // 使用 str() 方法获取字符串
    std::cout << response << std::flush;
    return response;
}

/***
 * cd命令
*/
std::string cd(std::string path = "") {
    std::stringstream output;
    std::string response;
    if (path.empty()) path = "/";
    if (path == "-") {
        if (last_dir == nullptr) {
            std::cout << "cd: OLDPWD not set" << std::endl;
            response = "cd: OLDPWD not set\n";
            return response;
        }
        Dir* temp_dir = user_dir[curr_user];
        user_dir[curr_user] = last_dir;
        last_dir = temp_dir;
        response += show_path();
        std::cout << std::endl;
        return response;
    }
    if(path == "~") {
        user_dir[curr_user] = root_dir;
        return "";
    }
    Dir* cd_directory = get_ptr_by_path(path);
    if (cd_directory) {
        last_dir = user_dir[curr_user];
        user_dir[curr_user] = cd_directory;
        response += show_path();
        std::cout << std::endl;
        return response;
    } else {
        output << "cd: " << "No such file or directory called '"<< path << "'" << std::endl;
        response = output.str();
        std::cout <<response;
        return response;
    }
}

/***
 * dir命令
*/
std::string dir(std::string arg) {
    Dir* curr_dir = user_dir[curr_user];
    std::stringstream output;
    output<<std::left;
    std::string response;
    if (arg.empty()) {
        if(curr_dir->files.begin() == curr_dir->files.end()) {
            std::cout << "Nothing in this dir." << std::endl;
            response = "Nothing in this dir.\n";
            return response;
        }
        if(curr_dir->name == "~") {
            output << std::setw(8) << "." << std::setw(6)<< curr_dir->inode->creator;
            output << int_to_hex(curr_dir->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->inode->last_modified << std::endl;
        }
        else {
            output << std::setw(8) << "." << std::setw(6)<< curr_dir->inode->creator;
            output << int_to_hex(curr_dir->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->inode->last_modified << std::endl;
            output << std::setw(8) << ".." << std::setw(6)<< curr_dir->inode->creator; 
            output << int_to_hex(curr_dir->parent->inode->block_seq * BLOCK_SIZE) << "  " << curr_dir->parent->inode->last_modified << std::endl;
        }
        for (auto it = curr_dir->files.begin(); it != curr_dir->files.end(); ++it) {
            output << std::setw(8)<< it->first << std::setw(6)<< it->second->inode->creator; 
            output << int_to_hex(it->second->inode->block_seq * BLOCK_SIZE) << "  " << it->second->inode->last_modified << std::endl;
        }
        // if (!curr_dir->files.empty()) { 
        //     output << std::endl;
        // }
        response = output.str();
        std::cout <<response;
        return response;
    }
    // 完成指定路径下的文件列表显示
    Dir* dir_directory = get_ptr_by_path(arg);
    if (dir_directory == nullptr) {
        output << "dir: cannot access '" << arg << "': No such file or directory" << std::endl;
        response = output.str();
        std::cout <<response;
        return response;
    }
    if (dir_directory->inode->type == 1) {
        output << "dir: cannot access '" << arg << "': Not a directory" << std::endl;
        response = output.str();
        std::cout <<response;
        return response;
    }
    if(dir_directory->files.begin() == dir_directory->files.end()) {
        std::cout << "Nothing in this dir." << std::endl;
        response = "Nothing in this dir.\n";
        return response;
    }
    if(dir_directory->name == "~") {
        output << std::setw(8) << "." << std::setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->inode->last_modified << std::endl;
    }
    else {
        output << std::setw(8) << "." << std::setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->inode->last_modified << std::endl;
        output << std::setw(8) << ".." << std::setw(6)<< dir_directory->inode->creator; 
        output << int_to_hex(dir_directory->parent->inode->block_seq * BLOCK_SIZE) << "  " << dir_directory->parent->inode->last_modified << std::endl;
    }
    for (auto it = dir_directory->files.begin(); it != dir_directory->files.end(); ++it) {
        output << std::setw(8)<< it->first << std::setw(6)<< it->second->inode->creator; 
        output << int_to_hex(it->second->inode->block_seq * BLOCK_SIZE) << "  " << it->second->inode->last_modified << std::endl;
    }
    // if (!dir_directory->files.empty()) {
    //     output << std::endl;
    // }
    response = output.str();
    std::cout <<response;
    return response;
}

/***
 * md命令
*/
std::string md(std::string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    std::stringstream output;
    std::string response;
    if (name.empty()) {
        std::cout << "md: missing operand" << std::endl;
        response = "md: missing operand\n";
        return response;
    }
    //切分name，得到父目录和新建目录名
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string new_dir_name;
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
        output << "md: cannot create directory '" << parent_path << "': No such directory" << std::endl;
        response = output.str();
        std::cout <<response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "md: cannot create directory '" << parent_path << "': Not a directory" << std::endl;
        response = output.str();
        std::cout <<response;
        return response;
    }
    //检查new_dir_name是否已存在
    if (parent_directory->files.find(new_dir_name) != parent_directory->files.end()) {
        output << "md: cannot create directory '" << name << "': Dir exists" << std::endl;
        response = output.str();
        std::cout <<response;
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
    output << "md: create directory '" << name << "'" << std::endl;
    response = output.str();
    std::cout <<response;
    return response;
}

/***
 * rd命令
*/
std::string rd(std::string name) {
    Dir* curr_dir = user_dir[curr_user];
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    std::stringstream output;
    std::string response;
    if (name.empty()) {
        std::cout << "rd: missing operand" << std::endl;
        response = "rd: missing operand\n";
        return response;
    }
    //切分name，得到父目录和要删除的目录名字
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string del_dir_name;
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
        output << "rd: cannot remove directory '" << parent_path << "': No such file or directory '" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "rd: cannot remove directory '" << parent_path << "': Not a directory" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    //检查del_dir_name是否存在
    if(parent_directory->files.find(del_dir_name) == parent_directory->files.end()) {
        output << "rd: cannot remove directory '" << name << "': No such directory" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    //检查del_dir_name是否为目录
    if(parent_directory->files[del_dir_name]->inode->type != 0) {
        output << "rd: cannot remove directory '" << name << "': Not a directory" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    del_dir(file, parent_directory->files[del_dir_name]);
    parent_directory->files.erase(del_dir_name);
    file.close();
    output << "rd: remove directory '" << name << "'" << std::endl;
    response = output.str();
    std::cout << response;
    return response;
}

/***
 * del命令
*/
std::string newfile(std::string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    std::stringstream output;
    std::string response;
    if (name.empty()) {
        std::cout << "newfile: missing operand" << std::endl;
        response = "newfile: missing operand\n";
        return response;
    } 
    //切分name，得到父目录和新建目录名
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string new_dir_name;
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
        output << "newfile: cannot create directory '" << parent_path << "': No such directory '" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        output << "newfile: cannot create directory '" << parent_path << "': Not a directory" << std::endl;
        response = output.str();
        std::cout << response;
        return response;
    }
    //检查new_dir_name是否已存在
    if (parent_directory->files.find(new_dir_name) != parent_directory->files.end()) {
        output << "newfile: cannot create directory '" << name << "': File exists" << std::endl;
        response = output.str();
        std::cout << response;
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
    output << "newfile: create file '" << name << "'" << std::endl;
    response = output.str();
    std::cout << response;
    return response;
}

/***
 * del命令
*/
std::string cat(std::string name) {
    Dir* curr_dir = user_dir[curr_user];
    std::string response;
    if (name.empty()) {
        std::cout << "cat: missing operand" << std::endl;
        return "cat: missing operand\n";
    }
    // //检查该文件是否正在被写入
    // if(file_lock.find(name) != file_lock.end()){
    //     std::cout << "write: cannot write file '" << name << "': File is being written" << std::endl;
    //     return "write: cannot write file '" + name + "': File is being written\n";
    // }
    //切分name，得到父目录和要删除的文件名字
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string cat_file_name;
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
        std::cout << "cat: cannot write file '" << parent_path << "': No such directory" << std::endl;
        return "cat: cannot write file '" + parent_path + "': No such directory\n";
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        std::cout << "cat: cannot write file '" << parent_path << "': Not a directory" << std::endl;
        return "cat: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    //检查cat_file_name是否存在
    if(parent_directory->files.find(cat_file_name) == parent_directory->files.end()) {
        std::cout << "cat: cannot remove file '" << name << "': No such file" << std::endl;
        return "cat: cannot remove file '" + name + "': No such file\n";
    }
    //检查cat_file_name是否为文件
    if(parent_directory->files[cat_file_name]->inode->type != 1) {
        std::cout << "cat: cannot write file '" << name << "': Not a file" << std::endl;
        return "cat: cannot write file '" + name + "': Not a file\n";
    }
    std::ifstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    std::string content = read_file_block(file, parent_directory->files[cat_file_name]->inode->children[0]);
    content += '\n';
    std::cout<< content <<std::flush;
    return content;
}

/***
 * write命令
*/
std::string copy(std::string arg1 = "", std::string arg2 = "") {
    Dir* curr_dir = user_dir[curr_user];
    if (arg1.empty() || arg2.empty()) {
        std::cout << "copy: missing operand" << std::endl;
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
        std::string host_path;
        std::vector<std::string> splitpath = cut_path(arg1);
        //如果输入的不是<host>
        if(splitpath[0] != "<host>"){
            std::cout << "Wrong input for '"<< splitpath[0] << "'. Do you mean '<host>' ?" <<std::endl;
            return "Wrong input for '" + splitpath[0] + "'. Do you mean '<host>' ?\n";
        }
        for(int i = 1; i < (int)splitpath.size(); i++){
            host_path += splitpath[i];
            if(i != (int)splitpath.size() - 1) host_path += "/";
        }
        FILE* fp = fopen(host_path.c_str(), "r");
        if(fp == NULL){
            std::cout << "copy: cannot stat '" << arg1 << "': No such file or directory in <host>" << std::endl;
            return "copy: cannot stat '" + arg1 + "': No such file or directory in <host>\n";
        }else{
            fclose(fp);
        }
        //分割arg1，得到文件名
        std::string filename;
        filename = splitpath[splitpath.size() - 1];
        //检查arg2是否存在
        Dir* paste_directory = get_ptr_by_path(arg2);
        if (paste_directory == nullptr) {
            std::cout << "copy: cannot stat '" << arg2 << "': No such directory" << std::endl;
            return "copy: cannot stat '" + arg2 + "': No such directory\n";
        }
        else{
            //分割arg2，得到文件被复制到的最后一级目录名path
            std::string path;
            std::vector<std::string> splitpath2 = cut_path(arg2);
            path = splitpath2[splitpath2.size() - 1];
            //检查path是否为目录
            if(paste_directory->inode->type != 0) {
                std::cout << "copy: cannot stat '" << arg2 << "': Not a directory" << std::endl;
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
            std::cout << "copy: successfully copy '" << arg1 << "' to '" << arg2 << "'" << std::endl;
            return "copy: successfully copy '" + arg1 + "' to '" + arg2 + "'\n";
        }
    }
    else{
        //实现simdisk内部文件的copy
        //分割arg1，得到文件名
        std::string filename;
        std::vector<std::string> splitpath = cut_path(arg1);
        filename = splitpath[splitpath.size() - 1];
        //逐级检查arg1是否存在
        Dir* copy_directory = get_ptr_by_path(arg1);
        if (copy_directory == nullptr) {
            std::cout << "copy: cannot stat '" << arg1 << "': No such file or directory" << std::endl;
            return "copy: cannot stat '" + arg1 + "': No such file or directory\n";
        }
        //检查arg2是否存在
        Dir* paste_directory = get_ptr_by_path(arg2);
        if (paste_directory == nullptr) {
            std::cout << "copy: cannot stat '" << arg2 << "': No such directory" << std::endl;
            return "copy: cannot stat '" + arg2 + "': No such directory\n";
        }
        //分割arg2，得到文件被复制到的最后一级目录名path
        std::string path;
        std::vector<std::string> splitpath2 = cut_path(arg2);
        path = splitpath2[splitpath2.size() - 1];
        //检查path是否为目录
        if(paste_directory->inode->type != 0) {
            std::cout << "copy: cannot stat '" << arg2 << "': Not a directory" << std::endl;
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
        std::cout << "copy: successfully copy '" << arg1 << "' to '" << arg2 << "'" << std::endl;
        return "copy: successfully copy '" + arg1 + "' to '" + arg2 + "'\n";
    }
}

/***
 * write命令
*/
std::string del(std::string name = "") {
    Dir* curr_dir = user_dir[curr_user];
    if (name.empty()) {
        std::cout << "del: missing operand" << std::endl;
        return "del: missing operand\n";
    }
    //切分name，得到父目录和要删除的文件名字
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string del_file_name;
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
        std::cout << "del: cannot remove file '" << parent_path << "': No such directory" << std::endl;
        return "del: cannot remove file '" + parent_path + "': No such directory\n";
    }
    //检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        std::cout << "del: cannot remove file '" << parent_path << "': Not a directory" << std::endl;
        return "del: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    //检查del_file_name是否存在
    if(parent_directory->files.find(del_file_name) == parent_directory->files.end()) {
        std::cout << "del: cannot remove file '" << name << "': No such file" << std::endl;
        return "del: cannot remove file '" + name + "': No such file\n";
    }
    //检查del_file_name是否为文件
    if(parent_directory->files[del_file_name]->inode->type != 1) {
        std::cout << "del: cannot remove file '" << name << "': Not a file" << std::endl;
        return "del: cannot remove file '" + name + "': Not a file\n";
    }
    //删除文件
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    clean_block(parent_directory->files[del_file_name]->inode->block_seq);
    write_inode(del_child(parent_directory->inode, parent_directory->files[del_file_name]->inode->block_seq), parent_directory->inode->block_seq);
    set_free(file, parent_directory->files[del_file_name]->inode->block_seq);
    file.close();
    delete parent_directory->files[del_file_name];
    parent_directory->files.erase(del_file_name);
    std::cout << "del: remove file '" << name << "'" << std::endl;
    return "del: remove file '" + name + "'\n";
}

/***
 * write命令的检查函数，检查文件是否存在、是否为文件、是否正在被写入
*/
std::string write_check(std::string name){
    Dir* curr_dir = user_dir[curr_user];
    std::string response;
    if (name.empty()) {
        std::cout << "write: missing operand" << std::endl;
        return "write: missing operand\n";
    }
    //检查该文件是否正在被写入
    if(file_lock.find(name) != file_lock.end()){
        std::cout << "write: cannot write file '" << name << "': File is being written" << std::endl;
        return "write: cannot write file '" + name + "': File is being written\n";
    }
    //切分name，得到父目录和要删除的文件名字
    std::vector<std::string> splitpath = cut_path(name);
    std::string parent_path;
    std::string del_file_name;
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
        std::cout << "write: cannot write file '" << parent_path << "': No such directory" << std::endl;
        return "write: cannot write file '" + parent_path + "': No such directory\n";
    }
    // 检查parent_path是否为目录
    if(parent_directory->inode->type != 0) {
        std::cout << "write: cannot write file '" << parent_path << "': Not a directory" << std::endl;
        return "write: cannot remove file '" + parent_path + "': Not a directory\n";
    }
    // 检查del_file_name是否存在
    if(parent_directory->files.find(del_file_name) == parent_directory->files.end()) {
        std::cout << "write: cannot remove file '" << name << "': No such file" << std::endl;
        return "write: cannot remove file '" + name + "': No such file\n";
    }
    // 检查del_file_name是否为文件
    if(parent_directory->files[del_file_name]->inode->type != 1) {
        std::cout << "write: cannot write file '" << name << "': Not a file" << std::endl;
        return "write: cannot write file '" + name + "': Not a file\n";
    }
    // 将文件名和块号写入文件锁file_lock中
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    file_lock[name] = parent_directory->files[del_file_name]->inode->block_seq;
    file_lock_name[curr_user] = name;
    parent_directory->files[del_file_name]->inode->num_of_children++;
    parent_directory->files[del_file_name]->inode->children[0] = find_block_for_file();
    write_inode(parent_directory->files[del_file_name]->inode, parent_directory->files[del_file_name]->inode->block_seq);
    set_busy(file, parent_directory->files[del_file_name]->inode->children[0]);
    file_lock_seq[curr_user] = parent_directory->files[del_file_name]->inode->children[0];
    std::cout << "Please input the content you want to write into the file. Press 'ESC + Enter' to finish." << std::endl;
    response = "Please input the content you want to write into the file. Press 'ESC + Enter' to finish.\n";
    return response;
}

/***
 * write命令
*/
std::string write(std::string content) {
    std::ofstream file("disk.bin", std::ios::binary | std::ios::in | std::ios::out);
    // 写入传回来的text，text中可能带有空格、换行符等
    std::string response;
    // 不需要检查text是否为空，因为write_check函数已经检查过了
    // 也不需要检查当前文件是否正在被写入，因为在shell中检查过了
    // 开始向文件中写入text
    response = write_file_block(file, content,file_lock_seq[curr_user]);
    file.close();
    std::cout << response << std::flush;
    return response;
}

/***
 * check命令
*/
std::string help(){
    std::stringstream output;
    output << "-------------------Help----------------------" << std::endl;
    output << std::left << std::setw(10) << "Command";
    output << std::setw(10) << "Description" << std::endl;
    output << std::setw(10) << "info";
    output << std::setw(10) << "Show information of the filesystem" << std::endl;
    output << std::setw(10) << "cd";
    output << std::setw(10) << "Change the current directory" << std::endl;
    output << std::setw(10) << "dir";
    output << std::setw(10) << "List the files in the directory" << std::endl;
    output << std::setw(10) << "md";
    output << std::setw(10) << "Make a new directory" << std::endl;
    output << std::setw(10) << "rd";
    output << std::setw(10) << "Remove a directory" << std::endl;
    output << std::setw(10) << "newfile";
    output << std::setw(10) << "Make a new file" << std::endl;
    output << std::setw(10) << "del";
    output << std::setw(10) << "Delete a file" << std::endl;
    output << std::setw(10) << "cat";
    output << std::setw(10) << "Show the content of a file" << std::endl;
    output << std::setw(10) << "copy";
    output << std::setw(10) << "Copy a file" << std::endl;
    output << std::setw(10) << "write";
    output << std::setw(10) << "Write a file" << std::endl;
    output << std::setw(10) << "check";
    output << std::setw(10) << "Check the filesystem" << std::endl;
    output << std::setw(10) << "exit";
    output << std::setw(10) << "Exit the filesystem" << std::endl;
    output << "---------------------------------------------" << std::endl;
    std::string response = output.str();
    std::cout << response;
    return response;
}

/***
 * check命令
*/
std::string check(){
    // 检查文件系统是否正常
    std::cout << "Everything is OK" << std::endl;
    return "Everything is OK\n";
}

/***
 * exit命令
*/
void exit() {
    std::cout<<"exit"<<std::endl;
}

/*--------------------------------------------------------------------------------------------------------*/
/*--------------------------------------------------------------------------------------------------------*/

#endif 