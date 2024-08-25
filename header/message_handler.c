#include "message_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <json-c/json.h>

// Global variables
IndexEntry *index_table = NULL;
uint32_t index_table_size = 0;
FreeSpaceEntry *free_space_table = NULL;
uint32_t free_space_table_size = 0;

// 인덱스 테이블을 초기화하는 함수
void initialize_index_table()
{
    FILE *file = fopen(INDEX_FILE, "rb");
    if (file == NULL)
    {
        // 인덱스 파일이 존재하지 않는 경우, 새로운 파일을 생성합니다.
        file = fopen(INDEX_FILE, "wb");
        if (file == NULL)
        {
            fprintf(stderr, "Error creating index file: %s\n", INDEX_FILE);
            exit(EXIT_FAILURE);
        }
        // 새 파일에 초기 인덱스 테이블 크기(0)를 쓰고 닫습니다.
        uint32_t initial_size = 0;
        fwrite(&initial_size, sizeof(uint32_t), 1, file);
        fclose(file);

        // 인덱스 테이블 초기화
        index_table = malloc(sizeof(IndexEntry) * MAX_MESSAGES);
        if (index_table == NULL)
        {
            fprintf(stderr, "Error allocating memory for index table\n");
            exit(EXIT_FAILURE);
        }
        index_table_size = 0;
        printf("Created new index file and initialized index table\n");
        return;
    }

    // 기존 파일에서 인덱스 테이블 크기를 읽습니다.
    if (fread(&index_table_size, sizeof(uint32_t), 1, file) != 1)
    {
        fprintf(stderr, "Error reading index table size from file\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // 인덱스 테이블 메모리 할당
    index_table = malloc(sizeof(IndexEntry) * MAX_MESSAGES);
    if (index_table == NULL)
    {
        fprintf(stderr, "Error allocating memory for index table\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // 인덱스 테이블 데이터 읽기
    for (uint32_t i = 0; i < index_table_size; i++)
    {
        if (fread(&index_table[i].index, sizeof(uint32_t), 1, file) != 1 ||
            fread(&index_table[i].offset, sizeof(uint64_t), 1, file) != 1 ||
            fread(&index_table[i].length, sizeof(uint32_t), 1, file) != 1 ||
            fread(&index_table[i].forward_link_count, sizeof(uint32_t), 1, file) != 1 ||
            fread(&index_table[i].backward_link_count, sizeof(uint32_t), 1, file) != 1)
        {
            fprintf(stderr, "Error reading index table entry from file\n");
            fclose(file);
            free(index_table);
            exit(EXIT_FAILURE);
        }

        // 순방향 링크 배열 읽기
        if (fread(index_table[i].forward_links, sizeof(uint32_t), index_table[i].forward_link_count, file) != index_table[i].forward_link_count)
        {
            fprintf(stderr, "Error reading forward links for index table entry\n");
            fclose(file);
            free(index_table);
            exit(EXIT_FAILURE);
        }

        // 역방향 링크 배열 읽기
        if (fread(index_table[i].backward_links, sizeof(uint32_t), index_table[i].backward_link_count, file) != index_table[i].backward_link_count)
        {
            fprintf(stderr, "Error reading backward links for index table entry\n");
            fclose(file);
            free(index_table);
            exit(EXIT_FAILURE);
        }
    }

    fclose(file);
    printf("Loaded index table with %u entries\n", index_table_size);
}

// 인덱스 테이블을 파일에 저장하는 함수
void save_index_table()
{
    FILE *file = fopen(INDEX_FILE, "wb");
    if (file == NULL)
    {
        fprintf(stderr, "Error opening index file for writing: %s\n", INDEX_FILE);
        return;
    }

    // 인덱스 테이블 크기를 씁니다.
    fwrite(&index_table_size, sizeof(uint32_t), 1, file);

    // 인덱스 테이블 데이터를 씁니다.
    for (uint32_t i = 0; i < index_table_size; i++)
    {
        fwrite(&index_table[i].index, sizeof(uint32_t), 1, file);
        fwrite(&index_table[i].offset, sizeof(uint64_t), 1, file);
        fwrite(&index_table[i].length, sizeof(uint32_t), 1, file);
        fwrite(&index_table[i].forward_link_count, sizeof(uint32_t), 1, file);
        fwrite(&index_table[i].backward_link_count, sizeof(uint32_t), 1, file);
        fwrite(index_table[i].forward_links, sizeof(uint32_t), index_table[i].forward_link_count, file);
        fwrite(index_table[i].backward_links, sizeof(uint32_t), index_table[i].backward_link_count, file);
    }

    fclose(file);
    printf("Saved index table with %u entries\n", index_table_size);
}
void initialize_free_space_table()
{
    FILE *file = fopen(FREE_SPACE_FILE, "rb");
    if (file == NULL)
    {
        file = fopen(FREE_SPACE_FILE, "wb");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Error creating free space file: %s", FREE_SPACE_FILE);
            exit(EXIT_FAILURE);
        }
        uint32_t initial_size = 0;
        fwrite(&initial_size, sizeof(uint32_t), 1, file);
        fclose(file);

        free_space_table = malloc(sizeof(FreeSpaceEntry) * MAX_MESSAGES);
        if (free_space_table == NULL)
        {
            syslog(LOG_ERR, "Error allocating memory for free space table");
            exit(EXIT_FAILURE);
        }
        free_space_table_size = 0;
        syslog(LOG_INFO, "Created new free space file and initialized free space table");
        return;
    }

    if (fread(&free_space_table_size, sizeof(uint32_t), 1, file) != 1)
    {
        syslog(LOG_ERR, "Error reading free space table size from file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    free_space_table = malloc(sizeof(FreeSpaceEntry) * MAX_MESSAGES);
    if (free_space_table == NULL)
    {
        syslog(LOG_ERR, "Error allocating memory for free space table");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fread(free_space_table, sizeof(FreeSpaceEntry), free_space_table_size, file) != free_space_table_size)
    {
        syslog(LOG_ERR, "Error reading free space table from file");
        fclose(file);
        free(free_space_table);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    syslog(LOG_INFO, "Loaded free space table with %u entries", free_space_table_size);
}
// 인덱스 테이블을 파일에 저장하는 함수
// void save_index_table()
// {
//     FILE *file = fopen(INDEX_FILE, "wb");
//     if (file == NULL)
//     {
//         syslog(LOG_ERR, "Error opening index file for writing: %s", INDEX_FILE);
//         return;
//     }

//     // 인덱스 테이블 크기를 씁니다.
//     fwrite(&index_table_size, sizeof(uint32_t), 1, file);

//     // 인덱스 테이블 데이터를 씁니다.
//     fwrite(index_table, sizeof(IndexEntry), index_table_size, file);

//     fclose(file);
//     syslog(LOG_INFO, "Saved index table with %u entries", index_table_size);
// }
void save_free_space_table()
{
    FILE *file = fopen(FREE_SPACE_FILE, "wb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening free space file for writing: %s", FREE_SPACE_FILE);
        return;
    }

    fwrite(&free_space_table_size, sizeof(uint32_t), 1, file);
    fwrite(free_space_table, sizeof(FreeSpaceEntry), free_space_table_size, file);

    fclose(file);
    syslog(LOG_INFO, "Saved free space table with %u entries", free_space_table_size);
}
uint64_t find_free_space(uint32_t required_length)
{
    for (uint32_t i = 0; i < free_space_table_size; i++)
    {
        if (free_space_table[i].length >= required_length)
        {
            uint64_t offset = free_space_table[i].offset;

            if (free_space_table[i].length > required_length)
            {
                // 남은 공간을 다시 free space table에 추가
                free_space_table[i].offset += required_length;
                free_space_table[i].length -= required_length;
            }
            else
            {
                // 정확히 맞는 공간이면 해당 entry를 제거
                memmove(&free_space_table[i], &free_space_table[i + 1],
                        (free_space_table_size - i - 1) * sizeof(FreeSpaceEntry));
                free_space_table_size--;
            }

            save_free_space_table();
            return offset;
        }
    }
    return 0; // 적절한 free space를 찾지 못함
}
void add_free_space(uint64_t offset, uint32_t length)
{
    if (free_space_table_size >= MAX_MESSAGES)
    {
        syslog(LOG_ERR, "Free space table is full");
        return;
    }

    // 간단한 구현: 새로운 free space를 테이블 끝에 추가
    free_space_table[free_space_table_size].offset = offset;
    free_space_table[free_space_table_size].length = length;
    free_space_table_size++;

    save_free_space_table();
}
// 새로운 함수: 파일의 마지막 인덱스를 읽어오는 함수
uint32_t get_last_index()
{
    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL)
    {
        // 파일이 없으면 인덱스 0부터 시작
        return 0;
    }

    uint32_t last_index = 0;
    uint32_t current_index;
    time_t timestamp;
    uint32_t message_len;

    // 파일의 끝까지 읽어가며 마지막 인덱스를 찾음
    while (fread(&current_index, sizeof(uint32_t), 1, file) == 1)
    {
        last_index = current_index;

        // 타임스탬프와 메시지 길이를 읽고 건너뜀
        fread(&timestamp, sizeof(time_t), 1, file);
        fread(&message_len, sizeof(uint32_t), 1, file);
        fseek(file, message_len, SEEK_CUR);
    }

    fclose(file);
    return last_index;
}
// 새로운 함수: 2의 거듭제곱 중 주어진 크기보다 크거나 같은 최소값을 반환
uint32_t next_power_of_two(uint32_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v < 16 ? 16 : v; // 최소 크기를 16으로 설정
}
int add_forward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // 유효하지 않은 인덱스
    }

    IndexEntry *source_entry = &index_table[source_index - 1];
    IndexEntry *target_entry = &index_table[target_index - 1];

    if (source_entry->forward_link_count >= MAX_LINKS)
    {
        return 0; // 더 이상 링크를 추가할 수 없음
    }

    // 이미 존재하는 링크인지 확인
    for (uint32_t i = 0; i < source_entry->forward_link_count; i++)
    {
        if (source_entry->forward_links[i] == target_index)
        {
            return 1; // 이미 존재하는 링크
        }
    }

    // 새 순방향 링크 추가
    source_entry->forward_links[source_entry->forward_link_count++] = target_index;

    // 대상 메시지의 역방향 링크 추가
    if (target_entry->backward_link_count < MAX_LINKS)
    {
        target_entry->backward_links[target_entry->backward_link_count++] = source_index;
    }

    save_index_table();
    return 1; // 성공
}

int add_backward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // 유효하지 않은 인덱스
    }

    IndexEntry *source_entry = &index_table[source_index - 1];
    IndexEntry *target_entry = &index_table[target_index - 1];

    if (source_entry->backward_link_count >= MAX_LINKS)
    {
        return 0; // 더 이상 링크를 추가할 수 없음
    }

    // 이미 존재하는 링크인지 확인
    for (uint32_t i = 0; i < source_entry->backward_link_count; i++)
    {
        if (source_entry->backward_links[i] == target_index)
        {
            return 1; // 이미 존재하는 링크
        }
    }

    // 새 역방향 링크 추가
    source_entry->backward_links[source_entry->backward_link_count++] = target_index;

    // 대상 메시지의 순방향 링크 추가
    if (target_entry->forward_link_count < MAX_LINKS)
    {
        target_entry->forward_links[target_entry->forward_link_count++] = source_index;
    }

    save_index_table();
    return 1; // 성공
}
int remove_forward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // 유효하지 않은 인덱스
    }

    IndexEntry *source_entry = &index_table[source_index - 1];
    IndexEntry *target_entry = &index_table[target_index - 1];

    int found = 0;

    // 순방향 링크 제거
    for (uint32_t i = 0; i < source_entry->forward_link_count; i++)
    {
        if (source_entry->forward_links[i] == target_index)
        {
            // 링크 제거 및 배열 정리
            for (uint32_t j = i; j < source_entry->forward_link_count - 1; j++)
            {
                source_entry->forward_links[j] = source_entry->forward_links[j + 1];
            }
            source_entry->forward_link_count--;
            found = 1;
            break;
        }
    }

    if (found)
    {
        // 대상 메시지의 역방향 링크 제거
        for (uint32_t i = 0; i < target_entry->backward_link_count; i++)
        {
            if (target_entry->backward_links[i] == source_index)
            {
                for (uint32_t j = i; j < target_entry->backward_link_count - 1; j++)
                {
                    target_entry->backward_links[j] = target_entry->backward_links[j + 1];
                }
                target_entry->backward_link_count--;
                break;
            }
        }
        save_index_table();
        return 1; // 성공
    }

    return 0; // 링크를 찾지 못함
}

int remove_backward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // 유효하지 않은 인덱스
    }

    IndexEntry *source_entry = &index_table[source_index - 1];
    IndexEntry *target_entry = &index_table[target_index - 1];

    int found = 0;

    // 역방향 링크 제거
    for (uint32_t i = 0; i < source_entry->backward_link_count; i++)
    {
        if (source_entry->backward_links[i] == target_index)
        {
            // 링크 제거 및 배열 정리
            for (uint32_t j = i; j < source_entry->backward_link_count - 1; j++)
            {
                source_entry->backward_links[j] = source_entry->backward_links[j + 1];
            }
            source_entry->backward_link_count--;
            found = 1;
            break;
        }
    }

    if (found)
    {
        // 대상 메시지의 순방향 링크 제거
        for (uint32_t i = 0; i < target_entry->forward_link_count; i++)
        {
            if (target_entry->forward_links[i] == source_index)
            {
                for (uint32_t j = i; j < target_entry->forward_link_count - 1; j++)
                {
                    target_entry->forward_links[j] = target_entry->forward_links[j + 1];
                }
                target_entry->forward_link_count--;
                break;
            }
        }
        save_index_table();
        return 1; // 성공
    }

    return 0; // 링크를 찾지 못함
}
uint32_t *get_forward_links(uint32_t index, uint32_t *count)
{
    if (index == 0 || index > index_table_size)
    {
        *count = 0;
        return NULL; // 유효하지 않은 인덱스
    }

    IndexEntry *entry = &index_table[index - 1];
    *count = entry->forward_link_count;

    if (entry->forward_link_count == 0)
    {
        return NULL;
    }

    uint32_t *links = malloc(sizeof(uint32_t) * entry->forward_link_count);
    if (links == NULL)
    {
        *count = 0;
        return NULL; // 메모리 할당 실패
    }

    memcpy(links, entry->forward_links, sizeof(uint32_t) * entry->forward_link_count);
    return links;
}

uint32_t *get_backward_links(uint32_t index, uint32_t *count)
{
    if (index == 0 || index > index_table_size)
    {
        *count = 0;
        return NULL; // 유효하지 않은 인덱스
    }

    IndexEntry *entry = &index_table[index - 1];
    *count = entry->backward_link_count;

    if (entry->backward_link_count == 0)
    {
        return NULL;
    }

    uint32_t *links = malloc(sizeof(uint32_t) * entry->backward_link_count);
    if (links == NULL)
    {
        *count = 0;
        return NULL; // 메모리 할당 실패
    }

    memcpy(links, entry->backward_links, sizeof(uint32_t) * entry->backward_link_count);
    return links;
}

// append_message_to_file 함수 수정
uint32_t append_message_to_file(const char *message)
{
    if (index_table_size >= MAX_MESSAGES)
    {
        syslog(LOG_ERR, "Error: Maximum number of messages reached");
        return 0;
    }

    uint32_t message_len = strlen(message);
    uint32_t total_len = sizeof(time_t) + sizeof(uint32_t) + message_len;
    uint32_t allocated_len = next_power_of_two(total_len); // 2의 거듭제곱 크기로 할당
    uint64_t offset = find_free_space(allocated_len);

    FILE *file;
    if (offset == 0)
    {
        file = fopen(MESSAGE_FILE, "ab");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Error opening message file for appending: %s", MESSAGE_FILE);
            return 0;
        }
        fseek(file, 0, SEEK_END);
        offset = ftell(file);
    }
    else
    {
        file = fopen(MESSAGE_FILE, "r+b");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Error opening message file for writing: %s", MESSAGE_FILE);
            return 0;
        }
        fseek(file, offset, SEEK_SET);
    }

    uint32_t index = index_table_size + 1;
    time_t now = time(NULL);

    fwrite(&now, sizeof(time_t), 1, file);
    fwrite(&message_len, sizeof(uint32_t), 1, file);
    fwrite(message, 1, message_len, file);

    // 남은 공간을 0으로 채움
    uint32_t padding = allocated_len - total_len;
    char *zero_pad = calloc(padding, 1);
    fwrite(zero_pad, 1, padding, file);
    free(zero_pad);

    if (ferror(file))
    {
        syslog(LOG_ERR, "Error writing to message file: %s", MESSAGE_FILE);
        fclose(file);
        return 0;
    }

    fclose(file);

    index_table[index_table_size].index = index;
    index_table[index_table_size].offset = offset;
    index_table[index_table_size].length = allocated_len;
    index_table[index_table_size].forward_link_count = 0; // 새 메시지는 링크가 없음
    index_table[index_table_size].backward_link_count = 0;
    memset(index_table[index_table_size].forward_links, 0, sizeof(uint32_t) * MAX_LINKS);  // 링크 배열 초기화
    memset(index_table[index_table_size].backward_links, 0, sizeof(uint32_t) * MAX_LINKS); // 링크 배열 초기화
    index_table_size++;

    save_index_table();

    syslog(LOG_INFO, "Message appended to file: %s (Index: %u, Allocated Length: %u)", MESSAGE_FILE, index, allocated_len);
    return index;
}
// modify_message_by_index 함수 수정
int modify_message_by_index(uint32_t target_index, const char *new_message)
{
    if (target_index == 0 || target_index > index_table_size)
    {
        return 0;
    }

    uint32_t new_message_len = strlen(new_message);
    uint32_t new_total_len = sizeof(time_t) + sizeof(uint32_t) + new_message_len;
    uint32_t new_allocated_len = next_power_of_two(new_total_len);

    if (new_allocated_len <= index_table[target_index - 1].length)
    {
        // 새 메시지가 기존 공간에 맞는 경우
        FILE *file = fopen(MESSAGE_FILE, "r+b");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Error opening file for modification: %s", MESSAGE_FILE);
            return 0;
        }

        uint64_t offset = index_table[target_index - 1].offset;
        fseek(file, offset, SEEK_SET);

        time_t now = time(NULL);
        fwrite(&now, sizeof(time_t), 1, file);
        fwrite(&new_message_len, sizeof(uint32_t), 1, file);
        fwrite(new_message, 1, new_message_len, file);

        // 남은 공간을 0으로 채움
        uint32_t padding = index_table[target_index - 1].length - new_total_len;
        char *zero_pad = calloc(padding, 1);
        fwrite(zero_pad, 1, padding, file);
        free(zero_pad);

        fclose(file);
    }
    else
    {
        // 새 메시지가 기존 공간보다 큰 경우
        uint64_t new_offset = find_free_space(new_allocated_len);
        if (new_offset == 0)
        {
            FILE *file = fopen(MESSAGE_FILE, "ab");
            if (file == NULL)
            {
                syslog(LOG_ERR, "Error opening message file for appending: %s", MESSAGE_FILE);
                return 0;
            }
            fseek(file, 0, SEEK_END);
            new_offset = ftell(file);
            fclose(file);
        }

        FILE *file = fopen(MESSAGE_FILE, "r+b");
        if (file == NULL)
        {
            syslog(LOG_ERR, "Error opening file for modification: %s", MESSAGE_FILE);
            return 0;
        }

        fseek(file, new_offset, SEEK_SET);

        time_t now = time(NULL);
        fwrite(&now, sizeof(time_t), 1, file);
        fwrite(&new_message_len, sizeof(uint32_t), 1, file);
        fwrite(new_message, 1, new_message_len, file);

        // 남은 공간을 0으로 채움
        uint32_t padding = new_allocated_len - new_total_len;
        char *zero_pad = calloc(padding, 1);
        fwrite(zero_pad, 1, padding, file);
        free(zero_pad);

        fclose(file);

        // 기존 공간을 free space로 추가
        add_free_space(index_table[target_index - 1].offset, index_table[target_index - 1].length);

        // 인덱스 테이블 업데이트
        index_table[target_index - 1].offset = new_offset;
        index_table[target_index - 1].length = new_allocated_len;
    }

    save_index_table();
    return 1; // 수정 성공
}
// 인덱스 테이블 정보를 JSON 형식으로 반환하는 함수
// get_index_table_info 함수 수정
char *get_index_table_info()
{
    json_object *index_array = json_object_new_array();

    for (uint32_t i = 0; i < index_table_size; i++)
    {
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "index", json_object_new_int(index_table[i].index));
        json_object_object_add(entry, "offset", json_object_new_int64(index_table[i].offset));
        json_object_object_add(entry, "length", json_object_new_int(index_table[i].length));

        json_object *forward_links_array = json_object_new_array();
        for (uint32_t j = 0; j < index_table[i].forward_link_count; j++)
        {
            json_object_array_add(forward_links_array, json_object_new_int(index_table[i].forward_links[j]));
        }
        json_object_object_add(entry, "forward_links", forward_links_array);

        json_object *backward_links_array = json_object_new_array();
        for (uint32_t j = 0; j < index_table[i].backward_link_count; j++)
        {
            json_object_array_add(backward_links_array, json_object_new_int(index_table[i].backward_links[j]));
        }
        json_object_object_add(entry, "backward_links", backward_links_array);

        json_object_array_add(index_array, entry);
    }

    json_object *result = json_object_new_object();
    json_object_object_add(result, "action", json_object_new_string("index_table_info"));
    json_object_object_add(result, "data", index_array);

    const char *json_string = json_object_to_json_string(result);
    char *response = strdup(json_string);

    json_object_put(result);
    return response;
}
// Free space 테이블 정보를 JSON 형식으로 반환하는 함수
char *get_free_space_table_info()
{
    json_object *free_space_array = json_object_new_array();

    for (uint32_t i = 0; i < free_space_table_size; i++)
    {
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "offset", json_object_new_int64(free_space_table[i].offset));
        json_object_object_add(entry, "length", json_object_new_int(free_space_table[i].length));
        json_object_array_add(free_space_array, entry);
    }

    json_object *result = json_object_new_object();
    json_object_object_add(result, "action", json_object_new_string("free_space_table_info"));
    json_object_object_add(result, "data", free_space_array);

    const char *json_string = json_object_to_json_string(result);
    char *response = strdup(json_string);

    json_object_put(result);
    return response;
}
// 새로운 함수: 특정 인덱스의 바이너리 데이터를 16진수 문자열로 반환
char *get_binary_data_by_index(uint32_t target_index)
{
    if (target_index == 0 || target_index > index_table_size)
    {
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening file for reading: %s", MESSAGE_FILE);
        return NULL;
    }

    uint64_t offset = index_table[target_index - 1].offset;
    uint32_t length = index_table[target_index - 1].length;
    fseek(file, offset, SEEK_SET);

    unsigned char *buffer = malloc(length);
    if (buffer == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length)
    {
        syslog(LOG_ERR, "Error reading full message data");
        free(buffer);
        return NULL;
    }

    char *hex_string = malloc(length * 2 + 1);
    if (hex_string == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for hex string");
        free(buffer);
        return NULL;
    }

    for (uint32_t i = 0; i < length; i++)
    {
        sprintf(hex_string + (i * 2), "%02x", buffer[i]);
    }
    hex_string[length * 2] = '\0';

    free(buffer);
    return hex_string;
}
// 수정된 함수: 특정 인덱스의 메시지를 지정된 형식으로 반환
char *get_message_by_index_and_format(uint32_t target_index, const char *format)
{
    if (target_index == 0 || target_index > index_table_size)
    {
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening file for reading: %s", MESSAGE_FILE);
        return NULL;
    }

    uint64_t offset = index_table[target_index - 1].offset;
    uint32_t length = index_table[target_index - 1].length;
    fseek(file, offset, SEEK_SET);

    unsigned char *buffer = malloc(length);
    if (buffer == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length)
    {
        syslog(LOG_ERR, "Error reading full message data");
        free(buffer);
        return NULL;
    }

    char *result;
    if (strcmp(format, "text") == 0)
    {
        time_t timestamp;
        uint32_t message_length;
        memcpy(&timestamp, buffer, sizeof(time_t));
        memcpy(&message_length, buffer + sizeof(time_t), sizeof(uint32_t));

        result = malloc(message_length + 1);
        if (result == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for text result");
            free(buffer);
            return NULL;
        }
        memcpy(result, buffer + sizeof(time_t) + sizeof(uint32_t), message_length);
        result[message_length] = '\0';
    }
    else if (strcmp(format, "binary") == 0 || strcmp(format, "hex") == 0)
    {
        result = malloc(length * 2 + 1);
        if (result == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for hex result");
            free(buffer);
            return NULL;
        }
        for (uint32_t i = 0; i < length; i++)
        {
            sprintf(result + (i * 2), "%02x", buffer[i]);
        }
        result[length * 2] = '\0';
    }
    else
    {
        syslog(LOG_ERR, "Unknown format requested");
        free(buffer);
        return NULL;
    }

    free(buffer);
    return result;
}
// 새로운 함수: 최대 인덱스 반환
uint32_t get_max_index()
{
    return index_table_size;
}