#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>


//定义的常量
#define SIZE 1024000   //虚拟磁盘空间大小
#define END 65535      //FAT中文件结束标志
#define FREE 0         //FAT中盘块空闲标志
#define ROOTBLOCKNUM 2 //根目录初始所占的盘块总数
#define MAXOPENFILE 10 //最多同时打开文件个数
#define DIRLEN 80      //文件路径长度

#define max(X, Y) (((X) > (Y)) ? (X) : (Y))
#define min(X, Y) (((X) < (Y)) ? (X) : (Y))


//定义文件控制块FCB
typedef struct FCB {
    char filename[DIRLEN];       //文件名
    char exname[3];              //文件扩展名
    unsigned char attribute;     //文件属性字段：0：目录文件，1：数据文件。
    unsigned short time;         //文件创建时间
    unsigned short data;         //文件创建日期
    unsigned short first;        //文件起始盘块号
    unsigned long length;        //文件长度
    char free;                   //目录项是否为空，0：空，1：已分配
}fcb;

//定义文件分配表FAT
typedef struct FAT {
  unsigned short id;             //下一块的ID
}fat;

//定义用户打开文件表USEROPEN
typedef struct USEROPEN {
    fcb open_fcb;                //文件的FCB中的内容
    char dir[DIRLEN];            //打开文件所在的路径，方便快速检查出指定文件是否已经打开
    int count;                   //读写指针在文件中的位置
    char fcbstate;               //是否修改了文件的FCB的内容，如果修改了置为 1，否则为0
    char topenfile;              //表示该用户打开表项是否为空，若值为0，表示为空，否则表示已被某打开文件占据
    int dirno;                   //相应打开文件的目录项在父目录文件中的盘块号
    int diroff;                  //相应打开文件的目录项在父目录文件的dirno盘块中的起始位置
}useropen;

//定义引导块BLOCK0
typedef struct BLOCK0 {
    int blocksize;
    int blocknum;
    unsigned short root;         //根目录文件的起始盘块号
    unsigned char *startblock;   //虚拟磁盘上数据区开始位置
} block0;

//-------------------------------------函数声明--------------------------------------------------------

//初始化文件控制块FCB
void fcb_init(fcb *new_fcb, const char* filename, unsigned short first, unsigned char attribute);

//初始化用户打开文件表USEROPEN
void useropen_init(useropen *openfile, int dirno, int diroff, const char* dir);

//释放FAT表中记录的磁盘块号
void fatFree(int id);

//得到一个空闲的FAT块
int getFreeFatid();

//得到一个空闲的打开文件表项
int getFreeOpenlist();

//得到一个FAT表项的下一个FAT表项，如果没有则创建
int getNextFat(int id);

//检查一个打开文件表下表是否合法
int check_fd(int fd);

//把一个路径按'/'分割
int spiltDir(char dirs[DIRLEN][DIRLEN], char *filename);

//把一个路径的最后一个目录从字符串中删去
void popLastDir(char *dir);

//把一个路径的最后一个目录从字符串中分割出
void splitLastDir(char *dir, char new_dir[2][DIRLEN]);

// 得到某个长度在某个FAT中的对应的盘块号和偏移量，用来记录一个打开文件项在其父目录对应FAT的位置
void getPos(int *id, int *offset, unsigned short first, int length);

//把路径规范化并检查
int rewrite_dir(char *dir);

//根据盘块号和偏移量，直接从FAT上读取指定长度的信息
int fat_read(unsigned short id, unsigned char *text, int offset, int len);

//读取某个已打开文件的指定长度信息
int do_read(int fd, unsigned char *text, int len);

//根据盘块号和偏移量，直接从FAT上写入指定长度的信息
int fat_write(unsigned short id, unsigned char *text, int blockoffset, int len);

//某个已打开文件写入指定长度信息
int do_write(int fd, unsigned char *text, int len, char op);

//从一个已打开目录文件找到对应名称的文件夹FCB，用于一些不断递归打开文件夹的函数中
int getFcb(fcb* fcbp, int *dirno, int *diroff, int fd, const char *dir);

//在一个已打开目录文件下打开某个文件
int getOpenlist(int fd, const char *org_dir);

//打开文件
int my_open(char *filename);

//读取一个文件夹下的FCB信息
int read_ls(int fd, unsigned char *text, int len);

//把一个文件夹下的FCB信息打印出来
void my_ls();

//把一个打开文件的内容根据文件指针打印出来
int my_read(int fd, int pos);

//重新从磁盘中读取一个打开文件的FCB内容
void my_reload(int fd);

//把键盘输入的信息写入一个打开文件
int my_write(int fd, int pos);

//把一个指定目录的FCB的free置为1
void my_rmdir(char *dirname);

//把一个指定文件的fcb的free置为1
void my_rm(char *filename);

//初始化
void my_format();

//在指定目录下创建一个文件或文件夹
int my_touch(char *filename, int attribute, int *rpafd);

//调用touch创建出一个文件
int my_create(char *filename);

//调用touch创建出一个文件夹
void my_mkdir(char *dirname);

//启动系统，做好相关的初始化
void startsys();

//退出系统，做好相应备份工作
void my_exitsys();

//将一个打开文件的FAT信息储存下来
void my_save(int fd);

//关闭一个打开文件
void my_close(int fd);

//利用my_open把当前目录切换到指定目录下
void my_cd(char *dirname);

//按16进制显示FAT数据
void show_fat(void);

//改变磁盘大小
void change_block_size(void);
//-------------------------------------函数声明---------------------------------------

//定义全局变量
unsigned char *myvhard;                  //指向虚拟磁盘的起始地址
useropen openfilelist[MAXOPENFILE];      //用户打开文件表数组
int curdirid;                            //当前目录的文件描述符fd
unsigned char *blockaddr[100000];      //磁盘块地址
block0 initblock;                        //声明引导块
fat fat1[100000], fat2[100000];      //声明FAT表
const char USERNAME[] = "bowenddd";      //用户名
int  BLOCKSIZE; //磁盘块大小
int BLOCKNUM;  //磁盘块总数

//-------------------------------------函数实现---------------------------------------

int main(){
    int fd;
    int pos;
    char command[DIRLEN];
    startsys();
    printf("%s %s: ", USERNAME, openfilelist[curdirid].dir);
    while(~scanf("%s", command)){
        if(strcmp(command, "exit") == 0){
            break;
        }
        else if(strcmp(command, "ls") == 0){
            my_ls();
        }
        else if(strcmp(command, "mkdir") == 0){
            scanf("%s", command);
            if (rewrite_dir(command)){
                my_mkdir(command);
            }
        }
        else if(strcmp(command, "close") == 0){
            scanf("%d", &fd);
            my_close(fd);
        }
        else if(strcmp(command, "open") == 0){
            scanf("%s", command);
            if(rewrite_dir(command) == 0){
                continue;
            }
            fd = my_open(command);
            if(0 <= fd && fd < MAXOPENFILE){
                if(openfilelist[fd].open_fcb.attribute == 0){
                    my_close(fd);
                    printf("%s is dirictory, please use cd command\n", command);
                }
                else{
                    printf("%s is open, it\'s id is %d\n", openfilelist[fd].dir, fd);
                }
            }
        }
        else if(strcmp(command, "cd") == 0){
            scanf("%s", command);
            if(rewrite_dir(command)){
                my_cd(command);
            }
        }
        else if(strcmp(command, "create") == 0){
            scanf("%s", command);
            if(rewrite_dir(command) == 0){
                continue;
            }
            fd = my_create(command);
            if(0 <= fd && fd < MAXOPENFILE) {
                printf("%s is created, it\'s id is %d\n", openfilelist[fd].dir, fd);
            }
        }
        else if(strcmp(command, "rm") == 0){
            scanf("%s", command);
            if(rewrite_dir(command)){
                my_rm(command);
            }
        }
        else if(strcmp(command, "rmdir") == 0){
            scanf("%s", command);
            if(rewrite_dir(command)){
                my_rmdir(command);
            }
        }
        else if(strcmp(command, "read") == 0){
            scanf("%d %d", &fd, &pos);
            if(pos >= 0){
                my_read(fd, pos);
            }
            else{
                my_read(fd,-1);
            }
        }
        else if(strcmp(command, "write") == 0){
            scanf("%d %d", &fd, &pos);
            if(pos >= 0){
                my_write(fd,pos);
            }
            else{
                my_write(fd,-1);
            }
        }
        else if(strcmp(command, "sf") == 0){
            for(int i = 0; i < MAXOPENFILE; i++) {
                if (openfilelist[i].topenfile){
                    printf("  %d : %s\n", i, openfilelist[i].dir);
                }
            }
        }
        else if(strcmp(command, "format") == 0){
            scanf("%s", command);
            my_format();
        }
        else if(strcmp(command, "showfat") == 0){
            show_fat();
        }
        else if(strcmp(command,"changesize") == 0){
            change_block_size();
        }
        else{
            printf("command %s : no such command\n", command);
        }
        my_reload(curdirid);
        printf("%s %s: ", USERNAME, openfilelist[curdirid].dir);
    }
    my_exitsys();
    return 0;
}

void fcb_init(fcb *new_fcb, const char* filename, unsigned short first, unsigned char attribute){
    strcpy(new_fcb->filename, filename);
    new_fcb->first = first;
    new_fcb->attribute = attribute;
    new_fcb->free = 0;
    if(attribute == 1){
        //如果类型是数据文件，则它的初始化长度为0
        new_fcb->length = 0;
    }
    else{
        //如果是目录文件，则初始长度为2个FCB大小（初始化要建立'.'和'..'两个特殊的目录项）
        new_fcb->length = 2 * sizeof(fcb);
    }
}

void useropen_init(useropen *openfile, int dirno, int diroff, const char* dir){
    openfile->dirno = dirno;
    openfile->diroff = diroff;
    strcpy(openfile->dir, dir);
    openfile->fcbstate = 0;
    openfile->topenfile = 1;
    openfile->count = openfile->open_fcb.length;
}

void fatFree(int id){
    if(id == END){
        return;
    }
    if(fat1[id].id != END){
        fatFree(fat1[id].id);
    }
    fat1[id].id = FREE;
}

int getFreeFatid(){
    for(int i = 5; i < BLOCKNUM; i++){
        if(fat1[i].id == FREE){
            fat1[i].id = END;
            return i;
        }
    }
    return END;
}

int getFreeOpenlist(){
    for(int i = 0; i < MAXOPENFILE; i++){
        if(openfilelist[i].topenfile == 0){
            return i;
        }
    }
  return -1;
}

int getNextFat(int id){
    if(fat1[id].id == END){
        fat1[id].id = getFreeFatid();
    }
    return fat1[id].id;
}

int check_fd(int fd){
    if((fd < 0 || fd >= MAXOPENFILE)){
        printf("ERROR:\n");
        printf("check_fd: %d is invaild index\n", fd);
        return 0;
    }
    return 1;
}

int spiltDir(char dirs[DIRLEN][DIRLEN], char *filename){
    int bg = 0;
    int ed = strlen(filename);
    if(filename[0] == '/'){
        bg++;
    }
    if(filename[ed - 1] == '/'){
        ed--;
    }
    int ret = 0;
    int tlen = 0;
    for(int i = bg; i < ed; i++){
        if(filename[i] == '/'){
            dirs[ret][tlen] = '\0';
            tlen = 0;
            ret++;
        }
        else{
            dirs[ret][tlen] = filename[i];
            tlen++;
        }
    }
    dirs[ret][tlen] = '\0';
    return ret+1;
}

void popLastDir(char *dir){
    int len = strlen(dir) - 1;
    while(dir[len - 1] != '/'){
        len--;
    }
    dir[len] = '\0';
}

void splitLastDir(char *dir, char new_dir[2][DIRLEN]){
    int len = strlen(dir) - 1;
    int flag = -1;
    for(int i = len; i >= 0; i--){
        if(dir[i] == '/'){
            flag = i;
            break;
        }
    }
    if(flag == -1){
        printf("ERROR:\n");
        printf("splitLastDir: can\'t split %s\n", dir);
        return;
    }
    int tlen = 0;
    for(int i = 0; i < flag; i++){
        new_dir[0][tlen] = dir[i];
        tlen++;
    }
    new_dir[0][tlen] = '\0';
    tlen = 0;
    for(int i = flag + 1; i <= len; i++){
        new_dir[1][tlen] = dir[i];
        tlen++;
    }
    new_dir[1][tlen] = '\0';
}

void getPos(int *id, int *offset, unsigned short first, int length){
    int blockorder = length / BLOCKSIZE;
    *offset = length % BLOCKSIZE;
    *id = first;
    while(blockorder){
        blockorder--;
        *id = fat1[*id].id;
    }
}

int rewrite_dir(char *dir){
    int len = strlen(dir);
    if(dir[len-1] == '/'){
        len--;
    }
    /*
    不明觉历
    int pre = -1;
    for (int i = 0; i < len; i++){
        if (dir[len] == '/'){
            if (pre != -1){
                if (pre + 1 == i){
                    printf("rewrite_dir: %s is invaild, please check!\n", dir);
                    return 0;
                }
            }
            pre = i;
        }
    }
    */
    char newdir[len];
    if(dir[0] == '/'){
        strcpy(newdir, "~");
    }
    else{
        strcpy(newdir, openfilelist[curdirid].dir);
    }
    strcat(newdir, dir);
    strcpy(dir, newdir);
    return 1;
}

int fat_read(unsigned short id, unsigned char *text, int offset, int len){
    int ret = 0;
    unsigned char *buf = (unsigned char*)malloc(BLOCKSIZE);

    int count = 0;
    while(len){
        memcpy(buf, blockaddr[id], BLOCKSIZE);
        count = min(len, BLOCKSIZE - offset);
        memcpy(text + ret, buf + offset, count);
        len -= count;
        ret += count;
        offset = 0;
        id = fat1[id].id;
    }
    free(buf);
    return ret;
}

int do_read(int fd, unsigned char *text, int len){
    int blockorder = openfilelist[fd].count / BLOCKSIZE;
    int blockoffset = openfilelist[fd].count % BLOCKSIZE;
    unsigned short id = openfilelist[fd].open_fcb.first;
    while(blockorder){
        blockorder--;
        id = fat1[id].id;
    }
    int ret = fat_read(id, text, blockoffset, len);
    return ret;
}

int fat_write(unsigned short id, unsigned char *text, int blockoffset, int len){
    int ret = 0;
    char *buf = (char*)malloc(BLOCKSIZE);
    if(buf == NULL){
        printf("ERROR:\n");
        printf("fat_write: malloc error\n");
        return -1;
    }
    // 写之前先把磁盘长度扩充到所需大小
    int tlen = len;
    int toffset = blockoffset;
    unsigned short tid = id;
    while(tlen){
        if(tlen <= BLOCKSIZE - toffset){
            break;
        }
        tlen -= (BLOCKSIZE - toffset);
        toffset = 0;
        id = getNextFat(id);
        if(id == END){
            printf("ERROR:\n");
            printf("fat_write: no next fat\n");
            return -1;
        }
    }
    id = tid;
    int count = 0;
    while(len){
        memcpy(buf, blockaddr[id], BLOCKSIZE);
        count = min(len, BLOCKSIZE - blockoffset);
        memcpy(buf + blockoffset, text + ret, count);
        memcpy(blockaddr[id], buf, BLOCKSIZE);
        len -= count;
        ret += count;
        blockoffset = 0;
        id = fat1[id].id;
    }
    free(buf);
    return ret;
}

int do_write(int fd, unsigned char *text, int len, char op){
    fcb *fcbp = &openfilelist[fd].open_fcb;
    int blockorder = openfilelist[fd].count / BLOCKSIZE;
    int blockoffset = openfilelist[fd].count % BLOCKSIZE;
    unsigned short id = openfilelist[fd].open_fcb.first;
    while(blockorder){
        blockorder--;
        id = fat1[id].id;
    }
    int ret;
    if(op == 'a'){
        char *buf = (char *)malloc(BLOCKSIZE);
        memcpy(buf, blockaddr[id]+len, BLOCKSIZE - len);
        ret = fat_write(id, text, blockoffset, len);
        while(fat1[id].id != END){
            id = fat1[id].id;
        }
        blockoffset = (blockoffset + ret) % BLOCKSIZE;
        fat_write(id, buf, blockoffset, strlen(buf));
    }
    else{
        ret = fat_write(id, text, blockoffset, len);
    }
    fcbp->length += ret;
    openfilelist[fd].fcbstate = 1;
    // 如果文件夹被写了，那么其'.'也要被写进去
    // 其子文件夹的'..'也要被更新
    if(fcbp->attribute == 0){
        fcb tmp;
        memcpy(&tmp, fcbp, sizeof(fcb));
        strcpy(tmp.filename, ".");
        memcpy(blockaddr[fcbp->first], &tmp, sizeof(fcb));
        // 如果是根目录的话，".."也要被修改
        strcpy(tmp.filename, "..");
        if(fcbp->first == 5){
            memcpy(blockaddr[fcbp->first] + sizeof(fcb), &tmp, sizeof(fcb));
        }
        // 从磁盘中读出当前目录的信息
        unsigned char buf[SIZE];
        int read_size = read_ls(fd, buf, fcbp->length);
        if(read_size == -1){
            printf("ERROR:\n");
            printf("do_write: read_ls error\n");
            return 0;
        }
        fcb dirfcb;
        for(int i = 2 * sizeof(fcb); i < read_size; i += sizeof(fcb)){
            memcpy(&dirfcb, buf + i, sizeof(fcb));
            if(dirfcb.free || dirfcb.attribute){
                continue;
            }
            memcpy(blockaddr[dirfcb.first] + sizeof(fcb), &tmp, sizeof(fcb));
        }
    }
    return ret;
}

int getFcb(fcb* fcbp, int *dirno, int *diroff, int fd, const char *dir){
    if(fd == -1){
        memcpy(fcbp, blockaddr[5], sizeof(fcb));
        *dirno = 5;
        *diroff = 0;
        return 1;
    }
    useropen *file = &openfilelist[fd];
    //从磁盘中读出当前目录的信息
    unsigned char *buf = (unsigned char *)malloc(SIZE);
    int read_size = read_ls(fd, buf, file->open_fcb.length);
    if(read_size == -1){
        printf("ERROR:\n");
        printf("getFcb: read_ls error\n");
        return -1;
    }
    fcb dirfcb;
    int flag = -1;
    for(int i = 0; i < read_size; i += sizeof(fcb)){
        memcpy(&dirfcb, buf + i, sizeof(fcb));
        if(dirfcb.free){
            continue;
        }
        if(strcmp(dirfcb.filename, dir)==0){
            flag = i;
            break;
        }
    }
    free(buf);
    //没有找到需要的文件
    if(flag == -1){
        return -1;
    }
    //找到的话就开始计算相关信息，改变对应打开文件项的值
    getPos(dirno, diroff, file->open_fcb.first, flag);
    memcpy(fcbp, &dirfcb, sizeof(fcb));
    return 1;
}

int getOpenlist(int fd, const char *org_dir){
    // 把路径名处理成绝对路径
    char dir[DIRLEN];
    if(fd == -1){
        strcpy(dir, "~/");
    }
    else{
        strcpy(dir, openfilelist[fd].dir);
        strcat(dir, org_dir);
    }
    // 如果有打开的目录和想打开的目录重名，必须把原目录的内容写回磁盘
    for(int i = 0; i < MAXOPENFILE; i++){
        if(i != fd){
            if(openfilelist[i].topenfile==1 && strcmp(openfilelist[i].dir, dir)==0){
                my_save(i);
            }
        }
    }
    int fileid = getFreeOpenlist();
    if(fileid == -1){
        printf("ERROR:\n");
        printf("getOpenlist: openlist is full\n");
        return -1;
    }
    fcb dirfcb;
    useropen *file = &openfilelist[fileid];
    int ret;
    if(fd == -1){
        ret = getFcb(&file->open_fcb, &file->dirno, &file->diroff, -1, ".");
    }
    else{
        ret = getFcb(&file->open_fcb, &file->dirno, &file->diroff, fd, org_dir);
    }
    strcpy(file->dir, dir);
    file->fcbstate = 0;
    file->topenfile = 1;
    //如果打开的是一个文件夹，就在路径后面加上'/'
    if(file->open_fcb.attribute==0){
        int len = strlen(file->dir);
        if(file->dir[len-1] != '/'){
            strcat(file->dir, "/");
        }
    }
    if(ret == -1){
        file->topenfile = 0;
        return -1;
    }
    return fileid;
}

int my_open(char *filename){
    char dirs[DIRLEN][DIRLEN];
    int count = spiltDir(dirs, filename);
    char realdirs[DIRLEN][DIRLEN];
    int tot = 0;
    for(int i = 1; i < count; i++){
        if(strcmp(dirs[i], ".") == 0){
            continue;
        }
        if(strcmp(dirs[i], "..") == 0){
          if (tot){
            tot--;
          }
          continue;
        }
        strcpy(realdirs[tot], dirs[i]);
        tot++;
    }
    // 生成根目录的副本
    int fd = getOpenlist(-1, "");
    // 利用当前目录的副本不断找到下一个目录
    int flag = 0;
    for(int i = 0; i < tot; ++i){
        int newfd = getOpenlist(fd, realdirs[i]);
        if(newfd == -1){
            flag = 1;
            break;
        }
        my_close(fd);
        fd = newfd;
    }
    if(flag == 1){
        printf("ERROR:\n");
        printf("my_open: %s no such file or directory\n", filename);
        openfilelist[fd].topenfile = 0;
        return -1;
    }
    //如果当前打开的文件打开两次，则关闭后打开的一次，返回之前打开的fd
    for(int i = 0 ; i < MAXOPENFILE ; i++){
        if(openfilelist[i].topenfile == 1 && strcmp(openfilelist[i].dir, openfilelist[fd].dir) == 0 && fd != i ){
            my_close(fd);
            return i;
        }
    }
    if(openfilelist[fd].open_fcb.attribute == 1){
        openfilelist[fd].count = 0;
    }
    else{
        openfilelist[fd].count = openfilelist[fd].open_fcb.length;
    }
    return fd;
}

int read_ls(int fd, unsigned char *text, int len){
    int tcount = openfilelist[fd].count;
    //将当前的读写指针修改为最开始，读取当前目录的所有内容
    openfilelist[fd].count = 0;
    int ret = do_read(fd, text, len);
    openfilelist[fd].count = tcount;
    return ret;
}

void my_ls(){
    // 从磁盘中读出当前目录的信息
    unsigned char *buf = (unsigned char*)malloc(SIZE);
    int read_size = read_ls(curdirid, buf, openfilelist[curdirid].open_fcb.length);
    if(read_size == -1){
        free(buf);
        printf("ERROR:\n");
        printf("my_ls: read_ls error\n");
        return;
    }
    fcb dirfcb;
    for(int i = 0; i < read_size; i += sizeof(fcb)){
        memcpy(&dirfcb, buf + i, sizeof(fcb));
        if(dirfcb.free){
            continue;
        }
        if(dirfcb.attribute){
            printf(" %s", dirfcb.filename);
        }
        else{
            printf(" \x1B[32m%s\x1B[0m", dirfcb.filename);
        }
    }
    printf("\n");
    free(buf);
}

int my_read(int fd, int pos){
    if((fd < 0  || fd >= MAXOPENFILE) || (openfilelist[fd].topenfile == 0) || (openfilelist[fd].open_fcb.attribute == 0)){
        printf("my_read: fd invaild\n");
        return -1;
    }
    unsigned char *buf = (unsigned char *)malloc(SIZE);
    if(pos >= 0){
        if(pos <= openfilelist[fd].open_fcb.length){
            openfilelist[fd].count = pos;
        }
        else{
            printf("ERROR:\n");
            printf("my_read: pos is invalid!\n");
            return -1;
        }
    }
    else{
        openfilelist[fd].count = 0;
    }
    int len = openfilelist[fd].open_fcb.length - openfilelist[fd].count;
    int ret = do_read(fd, buf, len);
    if(ret == -1){
        free(buf);
        printf("my_read: do_read error\n");
        return -1;
    }
    buf[ret] = '\0';
    printf("%s\n", buf);
    return ret;
}

void my_reload(int fd){
    if(check_fd(fd) == 0){
        return ;
    }
    fat_read(openfilelist[fd].dirno, (unsigned char*)&openfilelist[fd].open_fcb, openfilelist[fd].diroff, sizeof(fcb));
    return;
}

int my_write(int fd, int pos){
    char content[SIZE];
    if((fd < 0 || fd >= MAXOPENFILE) || (openfilelist[fd].topenfile == 0) ||(openfilelist[fd].open_fcb.attribute == 0)){
        printf("ERROR:\n");
        printf("my_write: fd invaild\n");
        return -1;
    }
    useropen *file = &openfilelist[fd];
    printf("please input me the write style\n");
    printf("  a : append write\n");
    printf("  w : truncate write\n");
    printf("  o : overwrite write\n");
    char op[DIRLEN];
    scanf("%s", op);
    getchar();
    if(op[0] == 'a'){
        file->count = file->open_fcb.length;
    }
    else if(op[0] == 'w'){
        file->count = 0;
        file->open_fcb.length = 0;
        fatFree(fat1[file->open_fcb.first].id);
    }
    else if(op[0] != 'o'){
        printf("ERROR:\n");
        printf("my_write: invaild write style!\n");
        return -1;
    }
    if(op[0] != 'w'){
        if(pos >= 0){
            if(pos <= file->open_fcb.length){
                file->count = pos;
            }
            else{
                printf("ERROR:\n");
                printf("my_write: pos is invaild!\n");
                return -1;
            }
        }
    }
    int ret = 0;
    int tmp;
    printf("input content,end with input 'wq!'\n");
    while(fgets(content,SIZE*sizeof(char),stdin)){
        if(strncmp(content,"wq!",3) == 0){
            break;
        }
        int len = strlen(content);
        //content[len] = '\n';
        tmp = do_write(fd, (unsigned char*)content, len-1, op[0]);
        if(tmp == -1){
            printf("ERROR:\n");
            printf("my_write: do_write error\n");
            return -1;
        }
        file->count += tmp;
        ret += tmp;
        if(op[0] == 'o'){
            file->open_fcb.length -= tmp;
            if(tmp + pos > file->open_fcb.length){
                file->open_fcb.length = tmp + pos - openfilelist->open_fcb.length;
            }
        }
    }
    my_close(fd);
    return ret;
}

void my_rmdir(char *dirname){
    int fd = my_open(dirname);
    if(fd < 0 || fd >= MAXOPENFILE){
        printf("ERROR:\n");
        printf("my_rmdir: my_open error!");
        my_close(fd);
        return ;
    }
    if(openfilelist[fd].open_fcb.attribute){
        printf("my_rmdir: %s is a file, please use rm command\n", dirname);
        my_close(fd);
        return;
    }
    if(strcmp(openfilelist[fd].dir, openfilelist[curdirid].dir)==0){
        printf("my_rmdir: can not remove the current directory!\n");
        my_close(fd);
        return;
    }
    // 从磁盘中读出当前目录的信息
    int cnt = 0;
    unsigned char *buf = (unsigned char*)malloc(SIZE);
    int read_size = read_ls(fd, buf,openfilelist[fd].open_fcb.length);
    if(read_size == -1){
        my_close(fd);
        free(buf);
        printf("ERROR:\n");
        printf("my_rmdir: read_ls error\n");
        return;
    }
    fcb dirfcb;
    int flag = -1;
    for(int i = 0; i < read_size; i += sizeof(fcb)){
        memcpy(&dirfcb, buf + i, sizeof(fcb));
        if(dirfcb.free){
            continue;
        }
        cnt++;
    }
    if(cnt > 2){
        printf("ERROR:\n");
        printf("my_rmdir: %s is not empty\n", dirname);
        my_close(fd);
        return;
    }
    openfilelist[fd].open_fcb.free = 1;
    fatFree(openfilelist[fd].open_fcb.first);
    openfilelist[fd].fcbstate = 1;
    my_close(fd);
}

void my_rm(char *filename){
    int fd = my_open(filename);
    if(fd < 0 || fd >= MAXOPENFILE){
        printf("ERROR:\n");
        printf("my_rm: my_open error!\n");
        my_close(fd);
        return ;
    }
    if(openfilelist[fd].open_fcb.attribute == 0) {
        printf("ERROR:\n");
        printf("my_rm: %s is a directory, please use rmdir command\n", filename);
        my_close(fd);
        return;
    }
    openfilelist[fd].open_fcb.free = 1;
    fatFree(openfilelist[fd].open_fcb.first);
    openfilelist[fd].fcbstate = 1;
    my_close(fd);
}

void my_format(){
    printf("请输入磁盘块大小，和数量：\n");
    scanf("%d %d",&BLOCKSIZE,&BLOCKNUM);
    for(int i = 0; i < BLOCKNUM; i++){
        blockaddr[i] = i * BLOCKSIZE + myvhard;
    }
    for(int i = 0; i < MAXOPENFILE; i++){
        openfilelist[i].topenfile = 0;
    }
    initblock.root = 5;
    initblock.startblock = blockaddr[5];
    initblock.blocksize = BLOCKSIZE;
    initblock.blocknum = BLOCKNUM;
    for(int i = 0; i < 5; i++){
        fat1[i].id = END;
        fat2[i].id = END;
    }
    for(int i = 5; i < BLOCKNUM; i++){
        fat1[i].id = FREE;
        fat2[i].id = FREE;
    }
    fat1[5].id = END;
    fcb root;
    fcb_init(&root, ".", 5, 0);
    memcpy(blockaddr[5], &root, sizeof(fcb));
    strcpy(root.filename, "..");
    memcpy(blockaddr[5] + sizeof(fcb), &root, sizeof(fcb));
    printf("初始化完成\n");
}

int my_touch(char *filename, int attribute, int *rpafd){
    // 先打开file的上级目录，如果上级目录不存在就报错
    char split_dir[2][DIRLEN];
    splitLastDir(filename, split_dir);
    int pafd = my_open(split_dir[0]);
    if(pafd < 0 || pafd >= MAXOPENFILE){
        printf("ERROR\n");
        printf("my_creat: my_open error\n");
        return -1;
    }
    // 从磁盘中读出当前目录的信息，进行检查
    unsigned char *buf = (unsigned char*)malloc(SIZE);
    int read_size = read_ls(pafd, buf, openfilelist[pafd].open_fcb.length);
    if(read_size == -1){
        printf("ERROR:\n");
        printf("my_touch: read_ls error\n");
        return -1;
    }
    fcb dirfcb;
    for(int i = 0; i < read_size; i += sizeof(fcb)){
        memcpy(&dirfcb, buf + i, sizeof(fcb));
        if(dirfcb.free){
            continue;
        }
        if(!strcmp(dirfcb.filename, split_dir[1])){
            printf("%s is already exit\n", split_dir[1]);
            return -1;
        }
    }
    // 利用空闲磁盘块创建文件
    int fatid = getFreeFatid();
    if(fatid == -1){
        printf("ERROR:\n");
        printf("my_touch: no free fat\n");
        return -1;
    }
    fat1[fatid].id = END;
    fcb_init(&dirfcb, split_dir[1], fatid, attribute);
    // 写入父亲目录内存
    memcpy(buf, &dirfcb, sizeof(fcb));
    int write_size = do_write(pafd, buf, sizeof(fcb), 'o');
    if(write_size == -1){
        printf("ERROR:\n");
        printf("my_touch: do_write error\n");
        return -1;
    }
    openfilelist[pafd].count += write_size;

    // 创建自己的打开文件项
    int fd = getFreeOpenlist();
    if(fd < 0 || fd > MAXOPENFILE){
        printf("ERROR:\n");
        printf("my_touch: no free fat\n");
        return -1;
    }
    getPos(&openfilelist[fd].dirno, &openfilelist[fd].diroff, openfilelist[pafd].open_fcb.first, openfilelist[pafd].count - write_size);
    memcpy(&openfilelist[fd].open_fcb, &dirfcb, sizeof(fcb));
    if(attribute==1){
        openfilelist[fd].count = 0;
    }
    else{
        openfilelist[fd].count = openfilelist[fd].open_fcb.length;
    }
    openfilelist[fd].fcbstate = 1;
    openfilelist[fd].topenfile = 1;
    strcpy(openfilelist[fd].dir, openfilelist[pafd].dir);
    strcat(openfilelist[fd].dir, split_dir[1]);
    free(buf);
    *rpafd = pafd;
    return fd;
}

int my_create(char *filename){
    int pafd;
    int fd = my_touch(filename, 1, &pafd);
    if (check_fd(fd) == 0){
        printf("ERROR:\n");
        printf("my_create: check_fd error!\n");
    }
    my_close(pafd);
    return fd;
}

void my_mkdir(char *dirname){
    int pafd;
    int fd = my_touch(dirname, 0, &pafd);
    if(check_fd(fd) == 0){
        printf("ERROR:\n");
        printf("my_mkdir: check_fd error!\n");
        return;
    }
    unsigned char *buf = (unsigned char*)malloc(SIZE);
    // 把"."和".."装入自己的磁盘
    fcb dirfcb;
    memcpy(&dirfcb, &openfilelist[fd].open_fcb, sizeof(fcb));
    int fatid = dirfcb.first;
    strcpy(dirfcb.filename, ".");
    memcpy(blockaddr[fatid], &dirfcb, sizeof(fcb));
    memcpy(&dirfcb, &openfilelist[pafd].open_fcb, sizeof(fcb));
    strcpy(dirfcb.filename, "..");
    memcpy(blockaddr[fatid] + sizeof(fcb), &dirfcb, sizeof(fcb));
    my_close(pafd);
    my_close(fd);
    free(buf);
}

void startsys(){
    // 各种变量初始化
    myvhard = (unsigned char*)malloc(SIZE);
    // 准备读入 myfsys 文件信息
    FILE *fp = fopen("myfsys", "rb");
    char need_format = 0;
    // 判断是否需要格式化
    if(fp != NULL){
        unsigned char *buf = (unsigned char*)malloc(SIZE);
        fread(buf, 1, SIZE, fp);
        memcpy(myvhard, buf, SIZE);
        memcpy(&initblock, myvhard, sizeof(block0));
        BLOCKNUM = initblock.blocknum;
        BLOCKSIZE = initblock.blocksize;
        char choose;
        printf("初始化虚拟文件系统...\n");
        printf("您保存的文件系统空间大小为: %d\n磁盘块大小为: %d\n磁盘块数量为： %d\n", SIZE, BLOCKSIZE, BLOCKNUM);
        printf("您是否需要更改磁盘块大小？(y/n):\n");
        printf("\033[31m更改磁盘块大小会格式化磁盘空间，请谨慎考虑\033[0m\n");
        scanf("%c",&choose);
        if(choose == 'y'){
            need_format = 1;
        }
        free(buf);
        fclose(fp);
    }
    else {
        need_format = 1;
    }

    // 不需要格式化的话接着读入fat信息
    if(need_format == 0){
        for(int i = 0; i < BLOCKNUM; i++){
            blockaddr[i] = i * BLOCKSIZE + myvhard;
        }
        for(int i = 0; i < MAXOPENFILE; i++){
            openfilelist[i].topenfile = 0;
        }
        memcpy(fat1, blockaddr[1], BLOCKSIZE*sizeof(fat));
        memcpy(fat2, blockaddr[3], BLOCKSIZE*sizeof(fat));
        printf("载入文件系统成功...\n");
    }
    else {
        printf("创建文件系统...\n");
        my_format();
    }

    // 把根目录fcb放入打开文件表中，设定当前目录为根目录
    curdirid = 0;
    memcpy(&openfilelist[curdirid].open_fcb, blockaddr[5], sizeof(fcb));
    useropen_init(&openfilelist[curdirid], 5, 0, "~/");
}

void my_exitsys(){
    //关闭所有打开文件项
    for(int i = 0; i < MAXOPENFILE; i++){
        my_close(i);
    }
    memcpy(blockaddr[0], &initblock, sizeof(initblock));
    memcpy(blockaddr[1], fat1, BLOCKNUM*sizeof(fat));
    memcpy(blockaddr[3], fat1, BLOCKNUM*sizeof(fat));
    fcb *testfcb;
    //将内存中的磁盘块信息写入文件"myfsys"中保存。
    FILE *fp = fopen("myfsys", "wb");
    fwrite(myvhard, BLOCKSIZE, BLOCKNUM, fp);
    free(myvhard);
    fclose(fp);
}

void my_save(int fd){
    if(check_fd(fd) == 0){
        printf("ERROR:\n");
        printf("my_save: check_fd error!\n");
        return;
    }
    useropen *file = &openfilelist[fd];
    if(file->fcbstate == 1){
        fat_write(file->dirno, (unsigned char *)&file->open_fcb, file->diroff, sizeof(fcb));
    }
    file->fcbstate = 0;
    return;
}

void my_close(int fd){
    if(check_fd(fd) == 0){
        printf("ERROR:\n");
        printf("my_close: check_fd error!\n");
        return;
    }
    if(openfilelist[fd].topenfile == 0){
        return;
    }
    // 若内容有改变，把fcb内容写回父亲的磁盘块中
    if(openfilelist[fd].fcbstate == 1){
        my_save(fd);
    }
    openfilelist[fd].topenfile = 0;
    return;
}

void my_cd(char *dirname){
    int fd = my_open(dirname);
    if(check_fd(fd) == 0){
        printf("ERROR:\n");
        printf("my_cd: check_fd error!\n");
        return;
    }
    if(openfilelist[fd].open_fcb.attribute == 1){
        printf("ERROR:\n");
        printf("%s is a file, please use open command\n", openfilelist[fd].dir);
        my_close(fd);
        return;
    }
    my_close(curdirid);
    curdirid = fd;
}

void show_fat(){
    for(int i = 0 ; i < BLOCKNUM ; i += 16){
        for(int j = 0 ; j < 16 ; j++){
            if(i + j < BLOCKNUM){
                printf("%.4x ",fat1[i+j].id);
            }
        }
        printf("\n");
    }
}

void change_block_size(){
    // 各种变量初始化
    myvhard = (unsigned char*)malloc(SIZE);
    printf("\033[31m更改磁盘块大小会格式化磁盘空间，请谨慎考虑\033[0m\n");
    printf("您是否需要更改磁盘块大小？(y/n):\n");
    char choose;
    getchar();
    scanf("%c",&choose);
    if(choose != 'y'){
        return ;
    }
    my_format();
    // 把根目录fcb放入打开文件表中，设定当前目录为根目录
    curdirid = 0;
    memcpy(&openfilelist[curdirid].open_fcb, blockaddr[5], sizeof(fcb));
    useropen_init(&openfilelist[curdirid], 5, 0, "~/");
}
