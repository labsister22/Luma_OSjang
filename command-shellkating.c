#include "command-shellkating.h"

static int8_t copy_status = 0;  // 0 for success, 1 for failure

void syscall(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx) {
    __asm__ volatile("mov %0, %%ebx" : /* <Empty> */ : "r"(ebx));
    __asm__ volatile("mov %0, %%ecx" : /* <Empty> */ : "r"(ecx));
    __asm__ volatile("mov %0, %%edx" : /* <Empty> */ : "r"(edx));
    __asm__ volatile("mov %0, %%eax" : /* <Empty> */ : "r"(eax));
    // Note : gcc usually use %eax as intermediate register,
    //        so it need to be the last one to mov
    __asm__ volatile("int $0x30");
}

void puts(char* val, uint32_t color)
{
    syscall(6, (uint32_t) val, strlen(val), color);
}

void puts_integer(int number){
    char buffer[20];
    int i = 0;
    do {
        buffer[i++] = '0' + (number % 10);
        number /= 10;
    } while (number != 0);

    buffer[i] = '\0';

    int len = i;
    for (int j = 0; j < len / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[len - j - 1];
        buffer[len - j - 1] = temp;
    }
    puts(buffer,0xF);
}

void remove(char* name, char* ext, uint32_t parent_number)
{
    int8_t ret;

    struct ClusterBuffer data_buf;
    struct FAT32DriverRequest request = {
        .buf = &data_buf,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .buffer_size = 0,
        .parent_cluster_number = parent_number
    };

    memcpy(request.name, name, 8);
    memcpy(request.ext, ext, 3);
    syscall(3, (uint32_t)&request, (uint32_t)&ret, 0);

    memcpy(request.name, name, 8);
    memcpy(request.ext, ext, 3);
    request.parent_cluster_number = parent_number;
    syscall(3, (uint32_t)&request, (uint32_t)&ret, 0);
}

void copy(char* src_name, char* src_ext, uint32_t src_parent_number, char* target_name, char* target_ext, uint32_t target_parent_number) {
    struct ClusterBuffer cl = {0};
    int8_t t_retcode;
    struct FAT32DriverRequest t_request = {
        .buf = &cl,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .parent_cluster_number = target_parent_number,
        .buffer_size = sizeof(struct FAT32DirectoryEntry)
    };

    memcpy(t_request.name, target_name, 8);
    memcpy(t_request.ext, target_ext, 3);
    syscall(1, (uint32_t)&t_request, (uint32_t)&t_retcode, 0);

    if (t_retcode != 2) {
        remove(target_name, target_ext, target_parent_number);
    }
    uint32_t src_size;
    bool is_dir = 0;
    struct FAT32DirectoryTable src_table;
    uint32_t src_cluster_number;
    struct FAT32DirectoryTable src_parent_table;
    syscall(12, (uint32_t)&src_parent_table, src_parent_number, 0);
    
    for (int32_t i = 0; i < (int32_t)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)); i++) {
        // puts("aaa", 0x07);
        // puts((char*) (src_parent_table.table[i].name), 0x07);
        if (memcmp(src_parent_table.table[i].name, src_name, 4) == 0 &&
            memcmp(src_parent_table.table[i].ext, src_ext, 3) == 0) {
            src_size = src_parent_table.table[i].filesize;
            // puts((char*) (src_size), 0x07);
            src_cluster_number = (src_parent_table.table[i].cluster_high << 16) | src_parent_table.table[i].cluster_low;
            is_dir = src_parent_table.table[i].attribute == ATTR_SUBDIRECTORY;
            if (is_dir){
                src_size = sizeof(struct FAT32DirectoryTable);
            break;
            }
        }
    }

    // struct ClusterBuffer data_buf[(src_size + CLUSTER_SIZE - 1) / CLUSTER_SIZE];
    uint32_t num_clusters = (src_size + CLUSTER_SIZE - 1) / CLUSTER_SIZE;
    // puts((char*) (num_clusters), 0x07);

    // Allocate an array of ClusterBuffer of the correct size
    struct ClusterBuffer data_buf[num_clusters];


    struct FAT32DriverRequest request = {
        .buf = &data_buf,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .buffer_size = src_size,
        .parent_cluster_number = src_parent_number
    };

    memcpy(request.name, src_name, 8);
    memcpy(request.ext, src_ext, 3);
    
    int8_t retcode;
    if (is_dir) {
        request.buf = &src_table;
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
    } else{
        syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);}

    if (is_dir) {
        memcpy(request.name, target_name, 8);
        memcpy(request.ext, target_ext, 3);
        request.parent_cluster_number = target_parent_number;
        request.buffer_size = 0;
        syscall(2, (uint32_t)&request, (uint32_t)&retcode, 0);

        uint32_t target_cluster_number;
        struct FAT32DirectoryTable target_parent_table;

        syscall(12, (uint32_t)&target_parent_table, target_parent_number, 0);

        for (int32_t i = 0; i < (int32_t)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)); i++) {
            if (memcmp(target_parent_table.table[i].name, target_name, 8) == 0 &&
                memcmp(target_parent_table.table[i].ext, target_ext, 3) == 0 &&
                target_parent_table.table[i].user_attribute == UATTR_NOT_EMPTY) {
                target_cluster_number = (target_parent_table.table[i].cluster_high << 16) | target_parent_table.table[i].cluster_low;
            }
        }

        for (int32_t i = 1; i < (int32_t)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)); i++) {
            if (src_table.table[i].user_attribute == UATTR_NOT_EMPTY) {
                copy(src_table.table[i].name, src_table.table[i].ext, src_cluster_number, src_table.table[i].name, src_table.table[i].ext, target_cluster_number);
            }
        }
    } else {
        memcpy(request.name, target_name, 8);
        memcpy(request.ext, target_ext, 3);
        request.parent_cluster_number = target_parent_number;
        syscall(2, (uint32_t)&request, (uint32_t)&retcode, 0);
        if (retcode != 0){
            puts("Error: ",0x04);
            puts("Error writing to file", 0x07);
            puts_integer(retcode);
        }
    }

    if (true) {  
        copy_status = 0;  // Success
    } else {
        copy_status = 1;  // Failure
    }
}

void cp(char* command) {
    uint16_t n_words = countWords(command);
    if (n_words != 3) {
        puts("Error: ",0x04);
        puts("Invalid syntax\ncp <source_file> <dest_filen>\n", 0x07);
        return;
    }
    int16_t recursive = -1;
    int8_t retcode = 0;
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);

    struct FAT32DirectoryTable table_buf = {0};
    struct FAT32DriverRequest request = {
        .buf = &table_buf,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .parent_cluster_number = 2,
        .buffer_size = sizeof(struct FAT32DirectoryEntry)
    };
    for (uint16_t i = 1; i < n_words; i++) {
        uint16_t n = wordLen(command, i);
        char word[n + 1];
        getWord(command, i, word);
        if (strcmp(word, "-r"))
            recursive = i;
    }

    if ((recursive == -1 && n_words < 3) || (recursive != -1 && n_words < 4)) {
        // puts("hi", 0x07);
        // puts((char*) recursive, 0x07);
        puts("Error: ",0x04);
        puts(": missing file operands\n", 0x07);
        return;
    }
    uint16_t target_idx;
    if (recursive == n_words - 1)
        target_idx = n_words - 2;
    else
        target_idx = n_words - 1;

    uint16_t target_n = wordLen(command, target_idx);
    char target_filename[target_n + 1];
    getWord(command, target_idx, target_filename);

    if (2 == 2 && strcmp(target_filename, "..")) {
        puts("Error: ",0x04);
        puts("root folder does not have parent\n", 0x07);
        return;
    }
    // check if all source exists
    for (uint16_t i = 1; i < n_words; i++) {
        if (recursive == i || target_idx == i) continue;
        char filename[12];
        getWord(command, i, filename);

        if (strcmp(filename, "..")) {
            puts("Error: ",0x04);
            puts("cannot copy a directory, '..', into itself\n", 0x07);
            return;
        }

        char name[9];
        char ext[4];

        // if filename is too long
        if (parseFileName(filename, name, ext)) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" filename invalid, name or extension may be too long\n", 0x07);
            return;
        }

        memcpy(request.name, name, 8);
        memcpy(request.ext, ext, 3);
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
        if (retcode == 2) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" file not found\n", 0x07);
            return;
        }
        if (retcode == 0 && recursive == -1) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" is a directory;  -r not specified\n", 0x07);
            return;
        }
        if (retcode == 0 && strcmp(target_filename, "..") && memcmp(name, cwd.table[0].name, 8) == 0) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts("cannot copy into itself\n", 0x07);
            return;
        } 
    }
    char target_name[9];
    char target_ext[4];

    if (strcmp(target_filename, "..")) {
        retcode = 0;
    } else {
        if (parseFileName(target_filename, target_name, target_ext)) {
            puts("Error: ",0x04);
            puts(target_filename, 0x07);
            puts(" filename invalid, name or extension may be too long\n", 0x07);
            return;
        }

        memcpy(request.name, target_name, 8);
        memcpy(request.ext, target_ext, 3);
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
    }
    // target is an existing directory
    if (retcode == 0) {
        uint32_t target_cluster_number;
        struct FAT32DirectoryTable cwd = {0};
        syscall(22,(uint32_t)&cwd,0,0);
        if (!strcmp(target_filename, "..")) {
            for (int32_t i = 0; i < (int32_t)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)); i++) {
                if (memcmp(cwd.table[i].name, target_name, 8) == 0 &&
                    memcmp(cwd.table[i].ext, target_ext, 3) == 0) {
                    target_cluster_number = (cwd.table[i].cluster_high << 16) | cwd.table[i].cluster_low;
                }
            }
        } else {
            target_cluster_number = (cwd.table[0].cluster_high << 16) | cwd.table[0].cluster_low;
        }

        for (int16_t i = 1; i < n_words; i++) {
            if (i == recursive || i == target_idx) continue;
            char filename[12];
            getWord(command, i, filename);
            char name[9];
            char ext[4];
            parseFileName(filename, name, ext);

            copy(name, ext, 2, name, ext, target_cluster_number);
        }
    } else if (retcode == 1 || retcode == 2) {
        if ((recursive == -1 && n_words > 3) || (recursive != -1 && n_words > 4)) {
            puts("Error: ",0x04);
            puts(target_filename, 0x07);
            puts(" is not a folder\n", 0x07);
            return;
        } else {
            for (int16_t i = 1; i < n_words; i++) {
                if (i == recursive || i == target_idx) continue;
                char filename[12];
                getWord(command, i, filename);
                char name[9];
                char ext[4];
                parseFileName(filename, name, ext);
                copy(name, ext, 2, target_name, target_ext, 2);
            }
        }
        int cur_cluster = 2;
        syscall(19,(uint32_t)&cur_cluster,0,0);
        syscall(18,(uint32_t)cur_cluster,(uint32_t)"",(uint32_t)true);
    }
    
}

void rm(char* command) {
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nrm <file/folder>\n", 0x07);
        return;
    }
    int16_t recursive = -1;
    int8_t retcode = 0;

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);
    struct FAT32DirectoryTable table_buf = {0};
    struct FAT32DriverRequest request = {
        .buf = &table_buf,
        .name = "\0\0\0\0\0\0\0",
        .ext = "\0\0",
        .parent_cluster_number = 2, // could be a variable
        .buffer_size = sizeof(struct FAT32DirectoryTable)
    };

    for (uint16_t i = 1; i < n_words; i++) {
        uint16_t n = wordLen(command, i);
        char word[n + 1];
        getWord(command, i, word);
        if (strcmp(word, "-r"))
            recursive = i;
    }

    if ((recursive == -1 && n_words < 2) || (recursive != -1 && n_words < 3)) {
        puts("Error: ",0x04);
        puts(": missing file operands\n", 0x07);
        return;
    }

    // check if all files exists
    for (uint16_t i = 1; i < n_words; i++) {
        if (recursive == i) continue;
        char filename[12];
        getWord(command, i, filename);

        char name[9];
        char ext[4];

        // if filename is too long
        if (parseFileName(filename, name, ext)) {
            puts(filename, 0x07);
            puts(": filename invalid, name or extension may be too long\n", 0x07);
            return;
        }

        memcpy(request.name, name, 8);
        memcpy(request.ext, ext, 3);
        syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);
        if (retcode == 2) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" not found\n", 0x07);
            return;
        }
        if (retcode == 0 && recursive == -1) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" is a directory;  -r not specified\n", 0x07);
            return;
        }
    }

    for (uint16_t i = 1; i < n_words; i++) {
        if (recursive == i) continue;
        char filename[12];
        getWord(command, i, filename);

        char name[9];
        char ext[4];

        // if filename is too long
        if (parseFileName(filename, name, ext)) {
            puts("Error: ",0x04);
            puts(filename, 0x07);
            puts(" filename invalid, name or extension may be too long\n", 0x07);
            return;
        }

        remove(name, ext, 2); // 2 could be a variable
        syscall(18,(uint32_t)cur_cluster,(uint32_t)"",(uint32_t)true);
    }
}

void mkdir(char *command) {
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nmkdir <dir_name>\n", 0x07);
        return;
    }

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);

    char filename[12];
    getWord(command, 1, filename);  
    
    char name[9], ext[4];
    if (parseFileName(filename, name, ext)) {
        puts("Error: ",0x04);
        puts(filename, 0x07);
        puts("invalid directory name\n", 0x07);
        return;
    }

    struct FAT32DriverRequest request = {
        .buf = NULL,
        .name = "\0\0\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .buffer_size = 0,
        .parent_cluster_number = cur_cluster
    };
    memcpy(request.name, name, 8);
    memcpy(request.ext, ext, 3);

    int8_t retcode;
    syscall(2, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0) {
        puts("Directory created successfully\n", 0x07);
        syscall(18,(uint32_t)cur_cluster,(uint32_t)"",(uint32_t)true);
    } else {
        puts("Failed to create directory, error code: ", 0x07);
        puts("\n", 0x07);
    }
}
void getDir(char* path, char res[8],char delimiter,int *idx){
    int i = *idx,length=0;
    while(path[i]!=delimiter && path[i]!='\0'){
        res[length] = path[i];
        i++;
        length+=1;
    }
    *idx = i+=1;
    for(int j=length;j<8;j++){
        res[j] = '\0';
    }
}

int countPath(char* path){
    int count = 0,i=0;
    while(path[i]!='\0'){
        if(path[i]=='/'){
            count++;
        }
        i++;
    }
    return count+1;
}

int getLastCluster(char* path,int isUpdate){
    int last_cluster;
    int lastIdx = 0,nPath = countPath(path);
    char dir[8];

    syscall(19,(uint32_t)&last_cluster,0,0);
    getDir(path,dir,'/',&lastIdx);

    for(int i=0;i<nPath;i++){
        struct FAT32DriverRequest request = {
            .buf = NULL,
            .name = "\0\0\0\0\0\0\0\0",
            .ext = "\0\0\0",
            .parent_cluster_number = last_cluster,
            .buffer_size = 0,
        };
        memcpy(request.name, dir, 8);
        syscall(11,(uint32_t)&request,(uint32_t)&last_cluster,0);
        if(isUpdate==1 && last_cluster!=9999){
            syscall(18,(uint32_t)last_cluster,(uint32_t)dir,(uint32_t)true);
        }
        getDir(path,dir,'/',&lastIdx);
    }
    return last_cluster;
}

void mv(char* command) {
    uint16_t n_words = countWords(command);
    if (n_words != 3) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nmv <source> <destination>\n", 0x07);
        return;
    }

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);

    char source_filename[12],pathDest[100];
    getWord(command, 1, source_filename);
    getWord(command, 2, pathDest);

    char source_name[9], source_ext[4];

    if (parseFileName(source_filename, source_name, source_ext)) {
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts(": Invalid source file name or extension.\n", 0x07);
        return;
    }
    int dest_cluster = getLastCluster(pathDest,false);
    if(dest_cluster==9999){
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts("Invalid Destination Path.\n", 0x07);
    }
    puts_integer(dest_cluster);
    copy(source_name, source_ext, cur_cluster, source_name, source_ext, dest_cluster);

    if (copy_status == 0) {  // '0' is success
        remove(source_name, source_ext,cur_cluster);

        puts("Move successful.\n", 0x07);
    } else {
        puts("Move failed: unable to verify copy.\n", 0x07);
    }
}

int findFileFolder(char name[8],char ext[3]){
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);
    for (int i=2;i < (int)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry));i++)
    {   
        if(cwd.table[i].user_attribute!=UATTR_NOT_EMPTY){
            continue;
        }
        if (memcmp(cwd.table[i].name, name, 8) == 0 && memcmp(cwd.table[i].ext, ext, 3) == 0)
        {
            return i;
        }
    }
    return -9999;
}

void traverseFolder(int cluster, int destination,int isDelete){
    struct FAT32DirectoryTable cwd = {0};
    syscall(12,(uint32_t)&cwd,cluster,0);
    for(int i=2;i<64;i++){
        if(cwd.table[i].user_attribute!=UATTR_NOT_EMPTY){
            continue;
        }
        struct FAT32DirectoryEntry entry = cwd.table[i];
        if(cwd.table[i].attribute==ATTR_SUBDIRECTORY){
            traverseFolder((cwd.table[i].cluster_high<<16)|(cwd.table[i].cluster_low),destination,isDelete);
        }else{
            copy(entry.name,entry.ext,cluster,entry.name,entry.ext,destination);
            if(isDelete==1){
                remove(entry.name, entry.ext,cluster);
            }
        }
    }
}

void mvPath(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 3) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nmv <source> <destination>\n", 0x07);
        return;
    }

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);

    char source_filename[12],pathDest[100];
    getWord(command, 1, source_filename);
    getWord(command, 2, pathDest);

    char source_name[9], source_ext[4];

    if (parseFileName(source_filename, source_name, source_ext)) {
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts(": Invalid source file name or extension.\n", 0x07);
        return;
    }
    int dest_cluster = getLastCluster(pathDest,0);
    if(dest_cluster==9999){
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts("Invalid Destination Path.\n", 0x07);
    }

    int idx = findFileFolder(source_name,source_ext);
    if(idx==-9999){
        puts("Error: No such file/folder in this directory",0xF);
    }
    if(cwd.table[idx].attribute!=ATTR_SUBDIRECTORY){
        copy(source_name, source_ext, cur_cluster, source_name, source_ext, dest_cluster);
        remove(source_name, source_ext,cur_cluster);
    }else{
        int clusterFolder = (cwd.table[idx].cluster_high<<16)|(cwd.table[idx].cluster_low);
        traverseFolder(clusterFolder,dest_cluster,1);
    }
    if (copy_status == 0) {  // '0' is success
        puts("Move successful.\n", 0x07);
    } else {
        puts("Move failed: unable to verify copy.\n", 0x07);
    }
}


void ls() {
    puts("name    ext type   size\n",0xF);
    puts("==============================\n",0xF);
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);
    for(int32_t i = 2; i <(int32_t)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry)); i++){
        if(cwd.table[i].user_attribute!=UATTR_NOT_EMPTY){
            continue;
        }
        puts(cwd.table[i].name,0x02);
        
        int leftSpace = 8 - strlen(cwd.table[i].name);

        if (cwd.table[i].attribute == ATTR_SUBDIRECTORY){
            leftSpace += 4;

            char spaces[leftSpace];
            for(int i = 0; i < leftSpace; i++){
                spaces[i] = ' ';
            }
            syscall(6,(uint32_t)spaces,leftSpace,0xF);

            puts("folder 0",0xF);
            puts("\n",0xF);
        } else {
            char spaces[leftSpace];
            for(int i = 0; i < leftSpace; i++){
                spaces[i] = ' ';
            }
            puts(spaces,0xF);
            puts(cwd.table[i].ext,0xF);
            if(strcmp(cwd.table[i].ext,"\0\0\0")){
                puts("   ",0xF);
            }
            puts(" file   ",0xF);
            puts_integer(cwd.table[i].filesize);
            puts("\n",0xF);

        }
    }
}

void findShell(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nfind <file/folder>\n", 0x07);
        return;
    }
    char name[8] = "\0\0\0\0\0\0\0\0", ext[3] = "\0\0\0";
    char filename[12];
    getWord(command,1,filename);
    if (parseFileName(filename, name, ext)) {
        puts("Error: ",0x04);
        puts(filename, 0x07);
        puts(": invalid directory name\n", 0x07);
        return;
    }

    syscall(9,(uint32_t)name,(uint32_t)ext,0);
}

void cd(char *command){
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\ncd <dir_name>\n", 0x07);
        return;
    }
    char path[100]="";
    getWord(command,1,path);

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);

    if(strcmp(path,"")){
        puts("Error: ",0x04);
        puts("Missing argument",0x07);
        return;
    }
    if(strcmp(path,"..")){
        if(cur_cluster==2){
            puts("Error: ",0x04);
            puts("Current Directory is Already in Root\n",0x07);
            return;
        }
        int parentClus = 0;
        syscall(20,(uint32_t)&parentClus,0,0);
        syscall(18,(uint32_t)parentClus,(uint32_t)"",false);
        return;
    }

    int idx = getLastCluster(path,true);
    if(idx==9999){
        puts("Error: ",0x04);
        puts("Path not valid",0x07);
    }
}

void cat(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\ncat <file_name>\n", 0x07);
        return;
    }
    char name[9],ext[4],filename[12];
    getWord(command,1,filename);
    parseFileName(filename,name,ext);
    int idx = -9999;
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);
    for (int i=2;i < (int)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry));i++)
    {   
        if(cwd.table[i].user_attribute!=UATTR_NOT_EMPTY){
            continue;
        }
        if (memcmp(cwd.table[i].name, name, 8) == 0 && memcmp(cwd.table[i].ext, ext, 3) == 0)
        {
            idx = i;
        }
    }
    if(idx==-9999){
        puts("Error: ",0x04);
        puts("File not found\n",0x07);
        return;
    }

    char buff[cwd.table[idx].filesize];

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);
    struct FAT32DriverRequest request = {
        .buf = buff,
        .name = "\0\0\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = cur_cluster,
        .buffer_size = cwd.table[idx].filesize
    };

    memcpy(request.name, name, 8);
    memcpy(request.ext, ext, 3);
    int8_t retcode;
    syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);
    if (retcode == 1) {
        puts("Error: ",0x04);
        puts(name,0x07);
        puts(" is a subdirectory\n",0x07);
    }else {
        puts(buff,0xF);
    }
}   

void exec(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nexec <exe_name>\n", 0x07);
        return;
    }
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);

    int cur_cluster = 0;
    syscall(19,(uint32_t)&cur_cluster,0,0);
    char name[9],ext[4],filename[12];
    getWord(command,1,filename);
    parseFileName(filename,name,ext);
    int idx = -9999;
    for (int i=2;i < (int)(CLUSTER_SIZE / sizeof(struct FAT32DirectoryEntry));i++)
    {   
        if(cwd.table[i].user_attribute!=UATTR_NOT_EMPTY){
            continue;
        }
        if (memcmp(cwd.table[i].name, name, 8) == 0 && memcmp(cwd.table[i].ext, ext, 3) == 0)
        {
            idx = i;
        }
    }
    if(idx==-9999){
        puts("Error: ",0x04);
        puts("File not found\n",0x07);
        return;
    }

    struct FAT32DriverRequest request = {
        .buf = (uint8_t*) 0,
        .name = "\0\0\0\0\0\0\0\0",
        .ext = "\0\0\0",
        .parent_cluster_number = cur_cluster,
        .buffer_size = cwd.table[idx].filesize
    };
    memcpy(request.name, name, 8);
    memcpy(request.ext, ext, 3);

    uint32_t retcode = -1;
    syscall(13,(uint32_t)&request,(uint32_t)&retcode,0);

    if(retcode == 1){
        puts("Error: Number of active process is maximum\n",0x07);
    }else if(retcode == 2){
        puts("Error: virtual address entrypoint is not valid\n",0x07);
    }else if(retcode == 3){
        puts("Error: Memory available is not enough\n",0x07);
    }else{
        puts("Process has been created and active\n",0x07);
    }

}

void ps(){
    struct ProcessControlBlock list_pcb[PROCESS_COUNT_MAX];
    int countActive = 0;
    syscall(15,(uint32_t)&list_pcb,(uint32_t)&countActive,0);
    puts("PID   Name    State\n",0xF);
    puts("===================\n",0xF);
    for(int i=0;i<countActive;i++){
        puts_integer(list_pcb[i].metadata.pid);
        puts("     ",0xF);
        puts(list_pcb[i].metadata.name,0xF);
        if(list_pcb[i].metadata.cur_state==READY){
            puts("   READY",0xF);
        }else if(list_pcb[i].metadata.cur_state==RUNNING){
            puts("   RUNNING",0xF);
        }else{
            puts("   BLOCKED",0xF);
        }
        puts("\n",0xF);
    }
}

void kill(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 2) {
        puts("Error: ",0x04);
        puts("Invalid syntax\nkill <pid>\n", 0x07);
        return;
    }
    char pidStr[3] = "\0\0\0";
    getWord(command,1,pidStr); 
    int pid = string_to_int(pidStr);
    struct ProcessControlBlock list_pcb[PROCESS_COUNT_MAX];
    int countActive = 0;
    syscall(15,(uint32_t)&list_pcb,(uint32_t)&countActive,0);
    
    for(int i=0;i<PROCESS_COUNT_MAX;i++){
        if(list_pcb[i].metadata.pid==(uint32_t)pid){
            if(list_pcb[i].metadata.active){
                syscall(14,pid,0,0);
                puts("Process Destroy Successful\n\n",0x07);
                return;
            }
        }
    }
    puts("Error: ",0x04);
    puts("No such Process with Match PID\n\n",0x07);
}

void copyPath(char* command){
    uint16_t n_words = countWords(command);
    if (n_words != 3) {
        puts("Error: ",0x04);
        puts("Invalid syntax\ncp <source> <destination>\n", 0x07);
        return;
    }

    int cur_cluster = 2;
    syscall(19,(uint32_t)&cur_cluster,0,0);
    struct FAT32DirectoryTable cwd = {0};
    syscall(22,(uint32_t)&cwd,0,0);

    char source_filename[12],pathDest[100];
    getWord(command, 1, source_filename);
    getWord(command, 2, pathDest);

    char source_name[9], source_ext[4];

    if (parseFileName(source_filename, source_name, source_ext)) {
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts(": Invalid source file name or extension.\n", 0x07);
        return;
    }
    int dest_cluster = getLastCluster(pathDest,false);
    if(dest_cluster==9999){
        puts("Error: ",0x04);
        puts(source_filename, 0x07);
        puts("Invalid Destination Path.\n", 0x07);
    }

    int idx = findFileFolder(source_name,source_ext);
    if(idx==-9999){
        puts("Error: No such file/folder in this directory",0xF);
    }
    if(cwd.table[idx].attribute!=ATTR_SUBDIRECTORY){
        copy(source_name, source_ext, cur_cluster, source_name, source_ext, dest_cluster);
    }else{
        int clusterFolder = (cwd.table[idx].cluster_high<<16)|(cwd.table[idx].cluster_low);
        // puts_integer(clusterFolder);
        traverseFolder(clusterFolder,dest_cluster,0);
    }
    if (copy_status == 0) {  // '0' is success
        puts("Copy successful.\n", 0x07);
    } else {
        puts("Copy failed: unable to verify copy.\n", 0x07);
    }
}
