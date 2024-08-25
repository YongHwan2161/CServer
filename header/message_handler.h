#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <stdint.h>
#include <time.h>

#define MESSAGE_FILE "binary file/messages.bin"
#define INDEX_FILE "binary file/index.bin"
#define FREE_SPACE_FILE "binary file/free_space.bin"
#define MAX_MESSAGES 1000000
#define MAX_LINKS 20  // 각 메시지당 최대 링크 수

typedef struct {
    uint32_t index;
    uint64_t offset;
    uint32_t length;
    uint32_t forward_link_count;
    uint32_t backward_link_count;
    uint32_t forward_links[MAX_LINKS];
    uint32_t backward_links[MAX_LINKS];
} IndexEntry;

typedef struct {
    uint64_t offset;
    uint32_t length;
} FreeSpaceEntry;
extern IndexEntry *index_table;
extern uint32_t index_table_size;
extern FreeSpaceEntry *free_space_table;
extern uint32_t free_space_table_size;

// Function declarations
void initialize_index_table();
void initialize_free_space_table();
void save_index_table();
void save_free_space_table();
uint64_t find_free_space(uint32_t required_length);
void add_free_space(uint64_t offset, uint32_t length);
uint32_t get_last_index();
uint32_t next_power_of_two(uint32_t v);
uint32_t append_message_to_file(const char *message);
int modify_message_by_index(uint32_t target_index, const char *new_message);
char* get_index_table_info();
char* get_free_space_table_info();
char* get_binary_data_by_index(uint32_t target_index);
char* get_message_by_index_and_format(uint32_t target_index, const char* format);
uint32_t get_max_index();
// 수정된 함수 선언
int add_forward_link(uint32_t source_index, uint32_t target_index);
int add_backward_link(uint32_t source_index, uint32_t target_index);
int remove_forward_link(uint32_t source_index, uint32_t target_index);
int remove_backward_link(uint32_t source_index, uint32_t target_index);
uint32_t* get_forward_links(uint32_t index, uint32_t* count);
uint32_t* get_backward_links(uint32_t index, uint32_t* count);

#endif // MESSAGE_HANDLER_H