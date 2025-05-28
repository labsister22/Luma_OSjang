#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// Definisi Tipe Node Sistem File
typedef enum {
    FILE_TYPE,
    DIRECTORY_TYPE
} NodeType;

typedef struct FileSystemNode {
    char name[256];
    NodeType type;
    char* content; // Hanya untuk FILE_TYPE
    struct FileSystemNode* parent;
    struct FileSystemNode** children; // Array of pointers to children (for DIRECTORY_TYPE)
    int num_children;
    int max_children; // Capacity for children array
} FileSystemNode;

// Global variable for the current working directory
FileSystemNode* current_directory;
FileSystemNode* root_directory; // Global variable for the root

// --- Fungsi Helper (akan diimplementasikan) ---
FileSystemNode* create_node(const char* name, NodeType type);
void add_child(FileSystemNode* parent, FileSystemNode* child);
void remove_child(FileSystemNode* parent, FileSystemNode* child);
FileSystemNode* find_child(FileSystemNode* parent, const char* name);
void free_node(FileSystemNode* node); // Untuk membebaskan memori secara rekursif

// --- Implementasi Perintah Built-in (akan diimplementasikan) ---
void shell_cd(char* args);
void shell_ls();
void shell_mkdir(char* args);
void shell_cat(char* args);
void shell_cp(char* args); // Implementasi lebih kompleks
void shell_rm(char* args);
void shell_mv(char* args); // Implementasi lebih kompleks
void shell_find(char* args);

// --- Fungsi Shell Utama ---
void display_prompt();
void parse_command(char* input, char** command, char** args);

int main() {
    // Inisialisasi sistem file: Buat root
    root_directory = create_node("/", DIRECTORY_TYPE);
    current_directory = root_directory;

    printf("--- Simple Shell (In-Memory File System) ---\n");
    printf("Type 'exit' to quit.\n");

    char input[1024];
    char* command;
    char* args;

    while (true) {
        display_prompt();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break; // Handle EOF
        }

        // Hapus newline character
        input[strcspn(input, "\n")] = 0;

        parse_command(input, &command, &args);

        if (command == NULL || strlen(command) == 0) {
            continue;
        }

        if (strcmp(command, "exit") == 0) {
            break;
        } else if (strcmp(command, "cd") == 0) {
            shell_cd(args);
        } else if (strcmp(command, "ls") == 0) {
            shell_ls();
        } else if (strcmp(command, "mkdir") == 0) {
            shell_mkdir(args);
        } else if (strcmp(command, "cat") == 0) {
            shell_cat(args);
        } else if (strcmp(command, "cp") == 0) {
            shell_cp(args);
        } else if (strcmp(command, "rm") == 0) {
            shell_rm(args);
        } else if (strcmp(command, "mv") == 0) {
            shell_mv(args);
        } else if (strcmp(command, "find") == 0) {
            shell_find(args);
        } else {
            printf("Command not found: %s\n", command);
        }
    }

    // Bersihkan memori sebelum keluar
    free_node(root_directory);

    printf("Exiting shell. Goodbye!\n");
    return 0;
}

// --- Implementasi Fungsi Helper dan Perintah (akan ditambahkan di sini) ---

FileSystemNode* create_node(const char* name, NodeType type) {
    FileSystemNode* new_node = (FileSystemNode*)malloc(sizeof(FileSystemNode));
    if (new_node == NULL) {
        perror("Failed to allocate memory for FileSystemNode");
        exit(EXIT_FAILURE);
    }
    strncpy(new_node->name, name, sizeof(new_node->name) - 1);
    new_node->name[sizeof(new_node->name) - 1] = '\0';
    new_node->type = type;
    new_node->parent = NULL;
    new_node->num_children = 0;
    new_node->max_children = 0; // Akan dialokasikan saat dibutuhkan

    if (type == FILE_TYPE) {
        new_node->content = NULL; // Konten akan dialokasikan saat 'cat' atau 'cp' menulis ke sana
    } else { // DIRECTORY_TYPE
        new_node->children = NULL;
        new_node->max_children = 4; // Ukuran awal untuk anak-anak
        new_node->children = (FileSystemNode**)malloc(sizeof(FileSystemNode*) * new_node->max_children);
        if (new_node->children == NULL) {
            perror("Failed to allocate memory for directory children");
            exit(EXIT_FAILURE);
        }
    }
    return new_node;
}

void add_child(FileSystemNode* parent, FileSystemNode* child) {
    if (parent == NULL || parent->type != DIRECTORY_TYPE) {
        return;
    }

    // Reallocate if capacity is full
    if (parent->num_children >= parent->max_children) {
        parent->max_children *= 2;
        parent->children = (FileSystemNode**)realloc(parent->children, sizeof(FileSystemNode*) * parent->max_children);
        if (parent->children == NULL) {
            perror("Failed to reallocate memory for directory children");
            exit(EXIT_FAILURE);
        }
    }
    parent->children[parent->num_children++] = child;
    child->parent = parent;
}

FileSystemNode* find_child(FileSystemNode* parent, const char* name) {
    if (parent == NULL || parent->type != DIRECTORY_TYPE) {
        return NULL;
    }
    for (int i = 0; i < parent->num_children; i++) {
        if (strcmp(parent->children[i]->name, name) == 0) {
            return parent->children[i];
        }
    }
    return NULL;
}

void remove_child(FileSystemNode* parent, FileSystemNode* child) {
    if (parent == NULL || parent->type != DIRECTORY_TYPE || child == NULL) {
        return;
    }

    for (int i = 0; i < parent->num_children; i++) {
        if (parent->children[i] == child) {
            // Geser elemen setelah yang dihapus
            for (int j = i; j < parent->num_children - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->num_children--;
            return;
        }
    }
}

void free_node(FileSystemNode* node) {
    if (node == NULL) {
        return;
    }
    if (node->type == DIRECTORY_TYPE) {
        for (int i = 0; i < node->num_children; i++) {
            free_node(node->children[i]);
        }
        free(node->children);
    } else { // FILE_TYPE
        free(node->content);
    }
    free(node);
}

void display_prompt() {
    // Bangun path CWD dari root
    char path[1024] = "";
    FileSystemNode* temp = current_directory;
    while (temp != NULL && temp != root_directory) {
        char temp_path[256];
        snprintf(temp_path, sizeof(temp_path), "/%s%s", temp->name, path);
        strncpy(path, temp_path, sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        temp = temp->parent;
    }
    if (strcmp(path, "") == 0) { // Jika di root
        printf("/ $ ");
    } else {
        printf("%s $ ", path);
    }
}

void parse_command(char* input, char** command, char** args) {
    *command = strtok(input, " ");
    *args = strtok(NULL, ""); // Ambil sisa baris sebagai argumen
}

// --- Implementasi Perintah Built-in ---

void shell_cd(char* args) {
    if (args == NULL || strlen(args) == 0) {
        current_directory = root_directory; // Kembali ke root jika tidak ada argumen
        return;
    }

    if (strcmp(args, "..") == 0) {
        if (current_directory->parent != NULL) {
            current_directory = current_directory->parent;
        }
    } else if (strcmp(args, "/") == 0) {
        current_directory = root_directory;
    } else {
        FileSystemNode* target = find_child(current_directory, args);
        if (target != NULL && target->type == DIRECTORY_TYPE) {
            current_directory = target;
        } else {
            printf("cd: No such directory: %s\n", args);
        }
    }
}

void shell_ls() {
    if (current_directory->num_children == 0) {
        printf(".\n");
        return;
    }
    for (int i = 0; i < current_directory->num_children; i++) {
        printf("%s%s\n", current_directory->children[i]->name,
               (current_directory->children[i]->type == DIRECTORY_TYPE ? "/" : ""));
    }
}

void shell_mkdir(char* args) {
    if (args == NULL || strlen(args) == 0) {
        printf("mkdir: missing operand\n");
        return;
    }

    if (find_child(current_directory, args) != NULL) {
        printf("mkdir: cannot create directory '%s': File exists\n", args);
        return;
    }

    FileSystemNode* new_dir = create_node(args, DIRECTORY_TYPE);
    add_child(current_directory, new_dir);
}

void shell_cat(char* args) {
    if (args == NULL || strlen(args) == 0) {
        printf("cat: missing operand\n");
        return;
    }

    FileSystemNode* file = find_child(current_directory, args);
    if (file == NULL) {
        printf("cat: %s: No such file or directory\n", args);
    } else if (file->type == DIRECTORY_TYPE) {
        printf("cat: %s: Is a directory\n", args);
    } else {
        if (file->content != NULL) {
            printf("%s\n", file->content);
        }
    }
}

void shell_cp(char* args) {
    if (args == NULL) {
        printf("cp: missing file operand\n");
        return;
    }

    char* source_name = strtok(args, " ");
    char* dest_name = strtok(NULL, " ");

    if (source_name == NULL || dest_name == NULL) {
        printf("cp: missing destination file operand after '%s'\n", source_name ? source_name : "");
        return;
    }

    FileSystemNode* source_node = find_child(current_directory, source_name);
    if (source_node == NULL) {
        printf("cp: cannot stat '%s': No such file or directory\n", source_name);
        return;
    }

    FileSystemNode* dest_parent = current_directory;
    char* actual_dest_name = dest_name;

    // Check if dest_name is an existing directory
    FileSystemNode* potential_dest_dir = find_child(current_directory, dest_name);
    if (potential_dest_dir != NULL && potential_dest_dir->type == DIRECTORY_TYPE) {
        dest_parent = potential_dest_dir;
        actual_dest_name = source_node->name; // Use source name if copying into a directory
    }

    if (find_child(dest_parent, actual_dest_name) != NULL) {
        printf("cp: cannot overwrite '%s': File exists\n", actual_dest_name);
        return;
    }

    FileSystemNode* new_node = create_node(actual_dest_name, source_node->type);
    if (source_node->type == FILE_TYPE) {
        if (source_node->content != NULL) {
            new_node->content = strdup(source_node->content);
            if (new_node->content == NULL) {
                perror("cp: Failed to allocate content");
                free_node(new_node);
                return;
            }
        }
    } else { // DIRECTORY_TYPE - Bonus: Rekursif copy folder
        for (int i = 0; i < source_node->num_children; ++i) {
            // Ini akan membutuhkan implementasi rekursif yang lebih kompleks untuk cp
            // Untuk kesederhanaan, hanya membuat folder kosong jika itu folder
            // Atau Anda bisa memanggil shell_cp secara rekursif di sini.
            printf("cp: Warning: Recursive directory copy not fully implemented for '%s'\n", source_node->name);
        }
    }
    add_child(dest_parent, new_node);
}

void shell_rm(char* args) {
    if (args == NULL || strlen(args) == 0) {
        printf("rm: missing operand\n");
        return;
    }

    FileSystemNode* target = find_child(current_directory, args);
    if (target == NULL) {
        printf("rm: cannot remove '%s': No such file or directory\n", args);
        return;
    }

    if (target->type == DIRECTORY_TYPE && target->num_children > 0) {
        printf("rm: cannot remove '%s': Directory not empty\n", args);
        return; // Tidak menghapus folder non-kosong untuk kesederhanaan
    }

    remove_child(current_directory, target);
    free_node(target); // Bebaskan memori node yang dihapus
}

void shell_mv(char* args) {
    if (args == NULL) {
        printf("mv: missing file operand\n");
        return;
    }

    char* source_name = strtok(args, " ");
    char* dest_name = strtok(NULL, " ");

    if (source_name == NULL || dest_name == NULL) {
        printf("mv: missing destination file operand after '%s'\n", source_name ? source_name : "");
        return;
    }

    FileSystemNode* source_node = find_child(current_directory, source_name);
    if (source_node == NULL) {
        printf("mv: cannot stat '%s': No such file or directory\n", source_name);
        return;
    }

    FileSystemNode* dest_target = find_child(current_directory, dest_name);

    if (dest_target != NULL && dest_target->type == DIRECTORY_TYPE) {
        // Pindah ke direktori lain
        if (find_child(dest_target, source_name) != NULL) {
            printf("mv: cannot move to '%s': File exists in destination\n", dest_target->name);
            return;
        }
        remove_child(current_directory, source_node);
        add_child(dest_target, source_node);
    } else if (dest_target != NULL && dest_target->type == FILE_TYPE) {
        // Mengganti nama file yang sudah ada
        if (source_node->type == DIRECTORY_TYPE) {
            printf("mv: cannot overwrite non-directory '%s' with directory '%s'\n", dest_target->name, source_node->name);
            return;
        }
        // Hapus yang lama, ganti dengan yang baru
        remove_child(current_directory, dest_target);
        free_node(dest_target); // Hapus node lama
        remove_child(current_directory, source_node); // Hapus dari lokasi lama
        strncpy(source_node->name, dest_name, sizeof(source_node->name) - 1);
        source_node->name[sizeof(source_node->name) - 1] = '\0';
        add_child(current_directory, source_node); // Tambahkan dengan nama baru
    } else {
        // Mengganti nama di direktori yang sama
        strncpy(source_node->name, dest_name, sizeof(source_node->name) - 1);
        source_node->name[sizeof(source_node->name) - 1] = '\0';
    }
}

// Helper untuk find
void find_recursive(FileSystemNode* node, const char* target_name, char* current_path, int* found_count) {
    if (node == NULL) {
        return;
    }

    char path_buffer[1024];
    if (node->parent == NULL) { // Jika root
        snprintf(path_buffer, sizeof(path_buffer), "/%s", node->name);
    } else if (node->parent == root_directory && strcmp(node->parent->name, "/") == 0) {
         snprintf(path_buffer, sizeof(path_buffer), "/%s", node->name);
    }
    else {
        snprintf(path_buffer, sizeof(path_buffer), "%s/%s", current_path, node->name);
    }


    if (strcmp(node->name, target_name) == 0) {
        printf("%s\n", path_buffer);
        (*found_count)++;
    }

    if (node->type == DIRECTORY_TYPE) {
        for (int i = 0; i < node->num_children; i++) {
            find_recursive(node->children[i], target_name, path_buffer, found_count);
        }
    }
}


void shell_find(char* args) {
    if (args == NULL || strlen(args) == 0) {
        printf("find: missing operand\n");
        return;
    }

    int found_count = 0;
    char initial_path[2] = ""; // Start with empty path for root traversal
    find_recursive(root_directory, args, initial_path, &found_count);

    if (found_count == 0) {
        printf("find: '%s': No such file or directory found.\n", args);
    }
}