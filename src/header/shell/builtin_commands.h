#ifndef _BUILTIN_COMMANDS_H
#define _BUILTIN_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>

// Command status codes
#define CMD_SUCCESS 0
#define CMD_ERROR_INVALID_ARGS -1
#define CMD_ERROR_FILE_NOT_FOUND -2
#define CMD_ERROR_PERMISSION_DENIED -3
#define CMD_ERROR_IO_ERROR -4
#define CMD_ERROR_OUT_OF_MEMORY -5
#define MAX_PATH_LENGTH 256
#define MAX_FILENAME_LENGTH 255
#define BUFFER_SIZE 4096
// Utility functions
void print_error(const char* message);
void print_success(const char* message);
bool file_exists(const char* path);
bool is_directory_path(const char* path);

// Built-in command function prototypes
int8_t cmd_cp(char* args[], int argc, int* current_row);
int8_t cmd_mv(char* args[], int argc, int* current_row);
int8_t cmd_rm(char* args[], int argc, int* current_row);
int8_t cmd_mkdir(char* args[], int argc, int* current_row);
int8_t cmd_rmdir(char* args[], int argc, int* current_row);
int8_t cmd_ls(char* args[], int argc, int* current_row);
int8_t cmd_cat(char* args[], int argc, int* current_row);
int8_t cmd_touch(char* args[], int argc, int* current_row);
int8_t cmd_pwd(char* args[], int argc, int* current_row);
int8_t cmd_cd(char* args[], int argc, int* current_row);
int8_t cmd_help(char* args[], int argc, int* current_row);

// Command parser
int8_t execute_command(const char* cmd_line, int* current_row);
int parse_command(const char* cmd_line, char* cmd, char* args[], int* argc);

#endif