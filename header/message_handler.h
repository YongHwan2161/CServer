#ifndef MESSAGE_HANDLER_H
#define MESSAGE_HANDLER_H

#include <stdint.h>
#include <time.h>

#define MESSAGE_FILE "binary file/messages.bin"
#define INDEX_FILE "binary file/index.bin"
#define FREE_SPACE_FILE "binary file/free_space.bin"
#define MAX_MESSAGES 1000000
#define MAX_LINKS 5  // 각 메시지당 최대 링크 수

typedef struct {
    uint32_t index;
    uint64_t offset;
    uint32_t length;
    // uint32_t link; // 새로 추가된 필드: 링크된 메시지의 인덱스
    uint32_t links[MAX_LINKS];  // 링크 배열
    uint32_t link_count;        // 현재 설정된 링크 수
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
// 새로운 함수 선언
//int set_message_link(uint32_t source_index, uint32_t target_index);
//uint32_t get_message_link(uint32_t index);
// 수정된 함수 선언
int add_message_link(uint32_t source_index, uint32_t target_index);
int remove_message_link(uint32_t source_index, uint32_t target_index);
uint32_t* get_message_links(uint32_t index, uint32_t* count);

#endif // MESSAGE_HANDLER_H