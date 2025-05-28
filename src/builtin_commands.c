#include "header/shell/builtin_commands.h"

// Global variables
char current_working_directory[256] = "/";
uint32_t current_inode = 2; // Root directory inode
int current_output_row = 0; // Track current row for output

void print_string(const char *str, int row, int col)
{
    user_syscall(6, (uint32_t)str, row, col);
}

void print_char(char c, int row, int col)
{
    user_syscall(5, (uint32_t)&c, col, row);
}

// Helper function to print string and advance row
void print_line(const char *str)
{
    print_string(str, current_output_row++, 0);
}

// Utility function to output integers
void puts_integer_builtin(int number)
{
    char buffer[20];
    int i = 0;

    if (number == 0)
    {
        buffer[i++] = '0';
    }
    else
    {
        if (number < 0)
        {
            buffer[i++] = '-';
            number = -number;
        }

        int temp = number;
        while (temp > 0)
        {
            temp /= 10;
            i++;
        }

        buffer[i] = '\0';
        // Remove unused variable 'pos'
        // Convert number to string (reverse order)
        while (number > 0)
        {
            buffer[--i] = '0' + (number % 10);
            number /= 10;
        }
    }

    print_string(buffer, current_output_row, 0);
}

// Parse path into components
int parse_path(const char *path, char names[][256], int max_parts)
{
    int count = 0;
    int start = 0;
    int len = strlen(path);

    if (path[0] == '/')
    {
        start = 1; // Skip leading slash for absolute paths
    }

    for (int i = start; i <= len && count < max_parts; i++)
    {
        if (path[i] == '/' || path[i] == '\0')
        {
            int part_len = i - start;
            if (part_len > 0 && part_len < 256)
            {
                memcpy(names[count], &path[start], part_len);
                names[count][part_len] = '\0';
                count++;
            }
            start = i + 1;
        }
    }

    return count;
}

// Resolve path to inode number
uint32_t resolve_path(const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return 2; // Root inode
    }

    char path_parts[32][256];
    int part_count = parse_path(path, path_parts, 32);

    uint32_t current = (path[0] == '/') ? 2 : current_inode;

    for (int i = 0; i < part_count; i++)
    {
        if (strcmp(path_parts[i], "..") == 0)
        {
            // Go to parent directory
            user_syscall(20, (uint32_t)&current, 0, 0);
        }
        else if (strcmp(path_parts[i], ".") != 0)
        {
            // Navigate to subdirectory
            struct EXT2DriverRequest request = {
                .buf = NULL,
                .name_len = strlen(path_parts[i]),
                .parent_inode = current,
                .buffer_size = 0,
                .is_directory = 1};
            memcpy(request.name, path_parts[i], request.name_len);

            uint32_t result_inode;
            user_syscall(11, (uint32_t)&request, (uint32_t)&result_inode, 0);

            if (result_inode == 9999)
            { // Invalid path
                return 9999;
            }
            current = result_inode;
        }
    }

    return current;
}

// Check if file exists
bool file_exists(const char *name, uint32_t parent_inode, bool *is_directory)
{
    struct EXT2DriverRequest request = {
        .buf = NULL,
        .name_len = strlen(name),
        .parent_inode = parent_inode,
        .buffer_size = 0,
        .is_directory = 0};
    memcpy(request.name, name, request.name_len);

    int8_t retcode;
    user_syscall(1, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        *is_directory = true;
        return true;
    }
    else if (retcode == 1)
    {
        *is_directory = false;
        return true;
    }

    return false;
}

// Change directory command
void handle_cd(const char *path, int current_row)
{
    current_output_row = current_row;

    if (!path || strlen(path) == 0)
    {
        print_line("Error: Missing argument");
        return;
    }

    uint32_t target_inode = resolve_path(path);

    if (target_inode == 9999)
    {
        print_line("Error: Path not found");
        return;
    }

    // Update current directory
    current_inode = target_inode;
    user_syscall(18, current_inode, (uint32_t)path, true);

    // Update working directory string
    if (path[0] == '/')
    {
        strcpy(current_working_directory, path);
    }
    else
    {
        if (strcmp(current_working_directory, "/") != 0)
        {
            strcat(current_working_directory, "/");
        }
        strcat(current_working_directory, path);
    }
}

// List directory contents
void handle_ls(int current_row)
{
    current_output_row = current_row;

    print_line("name                type   size");
    print_line("================================");

    // Use user_syscall to get directory listing
    user_syscall(22, (uint32_t)NULL, current_inode, 0);
}

// Make directory command
void handle_mkdir(const char *name, int current_row)
{
    current_output_row = current_row;

    if (!name || strlen(name) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("mkdir <dir_name>");
        return;
    }

    struct EXT2DriverRequest request = {
        .buf = NULL,
        .name_len = strlen(name),
        .parent_inode = current_inode,
        .buffer_size = 0,
        .is_directory = 1};
    memcpy(request.name, name, request.name_len);

    int8_t retcode;
    user_syscall(2, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        print_line("Directory created successfully");
        user_syscall(18, current_inode, (uint32_t)"", true);
    }
    else
    {
        print_string("Failed to create directory, error code: ", current_output_row, 0);
        puts_integer_builtin(retcode);
        current_output_row++;
    }
}

// Display file contents
// Display file contents - Fixed version to prevent stack overflow
void handle_cat(const char *filename, int current_row)
{
    current_output_row = current_row;

    if (!filename || strlen(filename) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("cat <file_name>");
        return;
    }

    bool is_directory;
    if (!file_exists(filename, current_inode, &is_directory))
    {
        print_line("Error: File not found");
        return;
    }

    if (is_directory)
    {
        print_string("Error: ", current_output_row, 0);
        print_string(filename, current_output_row, 7);
        print_string(" is a directory", current_output_row, 7 + strlen(filename));
        current_output_row++;
        return;
    }

    // Get file size first
    struct EXT2DriverRequest size_request = {
        .buf = NULL,
        .name_len = strlen(filename),
        .parent_inode = current_inode,
        .buffer_size = 0,
        .is_directory = 0};
    memcpy(size_request.name, filename, size_request.name_len);

    uint32_t file_size;
    user_syscall(21, (uint32_t)&size_request, (uint32_t)&file_size, 0);

    // Check if file is too large to display (prevent stack overflow)
    const uint32_t MAX_CAT_SIZE = 4096; // 4KB limit for cat command
    if (file_size > MAX_CAT_SIZE)
    {
        print_line("Error: File too large to display");
        print_string("File size: ", current_output_row, 0);
        puts_integer_builtin(file_size);
        print_string(" bytes (max: ", current_output_row, 20);
        puts_integer_builtin(MAX_CAT_SIZE);
        print_string(" bytes)", current_output_row, 32);
        current_output_row++;
        return;
    }

    // Use fixed-size buffer instead of VLA
    static char buffer[4096 + 1]; // Static allocation to avoid stack issues

    if (file_size == 0)
    {
        print_line("(empty file)");
        return;
    }

    // Read file content
    struct EXT2DriverRequest request = {
        .buf = buffer,
        .name_len = strlen(filename),
        .parent_inode = current_inode,
        .buffer_size = file_size,
        .is_directory = 0};
    memcpy(request.name, filename, request.name_len);

    int8_t retcode;
    user_syscall(0, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        buffer[file_size] = '\0'; // Null terminate

        // Print file content line by line to handle long files better
        char *line_start = buffer;
        char *line_end;

        while ((line_end = strchr(line_start, '\n')) != NULL)
        {
            *line_end = '\0'; // Temporarily null-terminate the line
            print_string(line_start, current_output_row++, 0);
            *line_end = '\n'; // Restore newline
            line_start = line_end + 1;
        }

        // Print remaining content if no final newline
        if (*line_start != '\0')
        {
            print_string(line_start, current_output_row++, 0);
        }
    }
    else
    {
        print_line("Error reading file");
    }
}

// Copy files/directories
void handle_cp(const char *source, const char *destination, int current_row)
{
    current_output_row = current_row;

    if (!source || !destination || strlen(source) == 0 || strlen(destination) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("cp <source> <destination>");
        return;
    }

    bool src_is_dir;
    if (!file_exists(source, current_inode, &src_is_dir))
    {
        print_line("Error: Source file not found");
        return;
    }

    // Implementation similar to the FAT32 version but adapted for EXT2
    // This would involve reading the source file and writing to destination
    print_line("Copy operation not fully implemented for EXT2");
}

// Remove files/directories
void handle_rm(const char *path, int current_row)
{
    current_output_row = current_row;

    if (!path || strlen(path) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("rm <file/folder>");
        return;
    }

    bool is_directory;
    if (!file_exists(path, current_inode, &is_directory))
    {
        print_line("Error: File not found");
        return;
    }

    struct EXT2DriverRequest request = {
        .buf = NULL,
        .name_len = strlen(path),
        .parent_inode = current_inode,
        .buffer_size = 0,
        .is_directory = is_directory};
    memcpy(request.name, path, request.name_len);

    int8_t retcode;
    user_syscall(3, (uint32_t)&request, (uint32_t)&retcode, 0);

    if (retcode == 0)
    {
        print_line("File/directory removed successfully");
        user_syscall(18, current_inode, (uint32_t)"", true);
    }
    else
    {
        print_line("Error removing file/directory");
    }
}

// Move files/directories
void handle_mv(const char *source, const char *destination, int current_row)
{
    current_output_row = current_row;

    if (!source || !destination || strlen(source) == 0 || strlen(destination) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("mv <source> <destination>");
        return;
    }

    // For EXT2, this would typically involve updating directory entries
    print_line("Move operation not fully implemented for EXT2");
}

// Find files/directories
void handle_find(const char *name, int current_row)
{
    current_output_row = current_row;

    if (!name || strlen(name) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("find <file/folder>");
        return;
    }

    char search_name[256];
    strcpy(search_name, name);

    user_syscall(9, (uint32_t)search_name, 0, 0);
}

// Execute program
void handle_exec(const char *path, int current_row)
{
    current_output_row = current_row;

    if (!path || strlen(path) == 0)
    {
        print_line("Error: Invalid syntax");
        print_line("exec <exe_name>");
        return;
    }

    bool is_directory;
    if (!file_exists(path, current_inode, &is_directory))
    {
        print_line("Error: File not found");
        return;
    }

    if (is_directory)
    {
        print_line("Error: Cannot execute directory");
        return;
    }

    struct EXT2DriverRequest request = {
        .buf = (uint8_t *)0,
        .name_len = strlen(path),
        .parent_inode = current_inode,
        .buffer_size = 0,
        .is_directory = 0};
    memcpy(request.name, path, request.name_len);

    uint32_t retcode;
    user_syscall(13, (uint32_t)&request, (uint32_t)&retcode, 0);

    switch (retcode)
    {
    case PROCESS_CREATE_SUCCESS:
        print_line("Process created and active");
        break;
    case PROCESS_CREATE_FAIL_MAX_PROCESS_EXCEEDED:
        print_line("Error: Maximum number of processes reached");
        break;
    case PROCESS_CREATE_FAIL_INVALID_ENTRYPOINT:
        print_line("Error: Invalid entrypoint");
        break;
    case PROCESS_CREATE_FAIL_NOT_ENOUGH_MEMORY:
        print_line("Error: Not enough memory");
        break;
    case PROCESS_CREATE_FAIL_FS_READ_FAILURE:
        print_line("Error: File system read failure");
        break;
    default:
        print_line("Error: Unknown error occurred");
        break;
    }
}

// List running processes
void handle_ps(int current_row)
{
    current_output_row = current_row;

    struct ProcessControlBlock list_pcb[PROCESS_COUNT_MAX];
    int count_active = 0;

    user_syscall(15, (uint32_t)&list_pcb, (uint32_t)&count_active, 0);

    print_line("PID   Name    State");
    print_line("===================");

    for (int i = 0; i < count_active; i++)
    {
        puts_integer_builtin(list_pcb[i].metadata.pid);
        print_string("     ", current_output_row, 5);
        print_string(list_pcb[i].metadata.name, current_output_row, 10);

        int state_col = 10 + strlen(list_pcb[i].metadata.name);
        switch (list_pcb[i].metadata.cur_state)
        {
        case READY:
            print_string("   READY", current_output_row, state_col);
            break;
        case RUNNING:
            print_string("   RUNNING", current_output_row, state_col);
            break;
        case BLOCKED:
            print_string("   BLOCKED", current_output_row, state_col);
            break;
        }
        current_output_row++;
    }
}

// Kill process
void handle_kill(int pid, int current_row)
{
    current_output_row = current_row;

    if (pid <= 0)
    {
        print_line("Error: Invalid PID");
        return;
    }

    struct ProcessControlBlock list_pcb[PROCESS_COUNT_MAX];
    int count_active = 0;
    user_syscall(15, (uint32_t)&list_pcb, (uint32_t)&count_active, 0);

    bool process_found = false;
    for (int i = 0; i < PROCESS_COUNT_MAX; i++)
    {
        if (list_pcb[i].metadata.pid == (uint32_t)pid && list_pcb[i].metadata.active)
        {
            process_found = true;
            break;
        }
    }

    if (!process_found)
    {
        print_line("Error: No such process with matching PID");
        return;
    }

    user_syscall(14, pid, 0, 0);
    print_line("Process destroyed successfully");
}