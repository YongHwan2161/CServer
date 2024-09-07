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
        file = fopen(INDEX_FILE, "wb");
        if (file == NULL)
        {
            fprintf(stderr, "Error creating index file: %s\n", INDEX_FILE);
            exit(EXIT_FAILURE);
        }
        uint32_t initial_size = 0;
        fwrite(&initial_size, sizeof(uint32_t), 1, file);
        fclose(file);

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

    if (fread(&index_table_size, sizeof(uint32_t), 1, file) != 1)
    {
        fprintf(stderr, "Error reading index table size from file\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    index_table = malloc(sizeof(IndexEntry) * MAX_MESSAGES);
    if (index_table == NULL)
    {
        fprintf(stderr, "Error allocating memory for index table\n");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    if (fread(index_table, sizeof(IndexEntry), index_table_size, file) != index_table_size)
    {
        fprintf(stderr, "Error reading index table entries from file\n");
        fclose(file);
        free(index_table);
        exit(EXIT_FAILURE);
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

    fwrite(&index_table_size, sizeof(uint32_t), 1, file);
    fwrite(index_table, sizeof(IndexEntry), index_table_size, file);

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
// Link management functions
int add_forward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // Invalid indices
    }

    FILE *file = fopen(MESSAGE_FILE, "r+b");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        return 0;
    }

    // Read source message header
    fseek(file, index_table[source_index - 1].offset, SEEK_SET);
    MessageHeader source_header;
    fread(&source_header, sizeof(MessageHeader), 1, file);

    // Check if link already exists
    for (uint32_t i = 0; i < source_header.forward_link_count; i++)
    {
        if (source_header.forward_links[i] == target_index)
        {
            fclose(file);
            return 1; // Link already exists
        }
    }

    // Add forward link
    if (source_header.forward_link_count < MAX_LINKS)
    {
        source_header.forward_links[source_header.forward_link_count++] = target_index;
        fseek(file, index_table[source_index - 1].offset, SEEK_SET);
        fwrite(&source_header, sizeof(MessageHeader), 1, file);
    }
    else
    {
        fclose(file);
        return 0; // Max links reached
    }

    // Read target message header
    fseek(file, index_table[target_index - 1].offset, SEEK_SET);
    MessageHeader target_header;
    fread(&target_header, sizeof(MessageHeader), 1, file);

    // Add backward link to target
    if (target_header.backward_link_count < MAX_LINKS)
    {
        target_header.backward_links[target_header.backward_link_count++] = source_index;
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        fwrite(&target_header, sizeof(MessageHeader), 1, file);
    }

    fclose(file);
    return 1; // Success
}

int add_backward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // Invalid indices
    }

    FILE *file = fopen(MESSAGE_FILE, "r+b");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        return 0;
    }

    // Read source message header
    fseek(file, index_table[source_index - 1].offset, SEEK_SET);
    MessageHeader source_header;
    fread(&source_header, sizeof(MessageHeader), 1, file);

    // Check if link already exists
    for (uint32_t i = 0; i < source_header.backward_link_count; i++)
    {
        if (source_header.backward_links[i] == target_index)
        {
            fclose(file);
            return 1; // Link already exists
        }
    }

    // Add backward link
    if (source_header.backward_link_count < MAX_LINKS)
    {
        source_header.backward_links[source_header.backward_link_count++] = target_index;
        fseek(file, index_table[source_index - 1].offset, SEEK_SET);
        fwrite(&source_header, sizeof(MessageHeader), 1, file);
    }
    else
    {
        fclose(file);
        return 0; // Max links reached
    }

    // Read target message header
    fseek(file, index_table[target_index - 1].offset, SEEK_SET);
    MessageHeader target_header;
    fread(&target_header, sizeof(MessageHeader), 1, file);

    // Add forward link to target
    if (target_header.forward_link_count < MAX_LINKS)
    {
        target_header.forward_links[target_header.forward_link_count++] = source_index;
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        fwrite(&target_header, sizeof(MessageHeader), 1, file);
    }

    fclose(file);
    return 1; // Success
}

int remove_forward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // Invalid indices
    }

    FILE *file = fopen(MESSAGE_FILE, "r+b");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        return 0;
    }

    // Read source message header
    fseek(file, index_table[source_index - 1].offset, SEEK_SET);
    MessageHeader source_header;
    fread(&source_header, sizeof(MessageHeader), 1, file);

    // Find and remove forward link
    int found = 0;
    for (uint32_t i = 0; i < source_header.forward_link_count; i++)
    {
        if (source_header.forward_links[i] == target_index)
        {
            // Remove link by shifting remaining links
            for (uint32_t j = i; j < source_header.forward_link_count - 1; j++)
            {
                source_header.forward_links[j] = source_header.forward_links[j + 1];
            }
            source_header.forward_link_count--;
            found = 1;
            break;
        }
    }

    if (found)
    {
        // Write updated source header
        fseek(file, index_table[source_index - 1].offset, SEEK_SET);
        fwrite(&source_header, sizeof(MessageHeader), 1, file);

        // Read target message header
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        MessageHeader target_header;
        fread(&target_header, sizeof(MessageHeader), 1, file);

        // Remove backward link from target
        for (uint32_t i = 0; i < target_header.backward_link_count; i++)
        {
            if (target_header.backward_links[i] == source_index)
            {
                // Remove link by shifting remaining links
                for (uint32_t j = i; j < target_header.backward_link_count - 1; j++)
                {
                    target_header.backward_links[j] = target_header.backward_links[j + 1];
                }
                target_header.backward_link_count--;
                break;
            }
        }

        // Write updated target header
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        fwrite(&target_header, sizeof(MessageHeader), 1, file);
    }

    fclose(file);
    return found; // Return 1 if link was found and removed, 0 otherwise
}

int remove_backward_link(uint32_t source_index, uint32_t target_index)
{
    if (source_index == 0 || source_index > index_table_size ||
        target_index == 0 || target_index > index_table_size)
    {
        return 0; // Invalid indices
    }

    FILE *file = fopen(MESSAGE_FILE, "r+b");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        return 0;
    }

    // Read source message header
    fseek(file, index_table[source_index - 1].offset, SEEK_SET);
    MessageHeader source_header;
    fread(&source_header, sizeof(MessageHeader), 1, file);

    // Find and remove backward link
    int found = 0;
    for (uint32_t i = 0; i < source_header.backward_link_count; i++)
    {
        if (source_header.backward_links[i] == target_index)
        {
            // Remove link by shifting remaining links
            for (uint32_t j = i; j < source_header.backward_link_count - 1; j++)
            {
                source_header.backward_links[j] = source_header.backward_links[j + 1];
            }
            source_header.backward_link_count--;
            found = 1;
            break;
        }
    }

    if (found)
    {
        // Write updated source header
        fseek(file, index_table[source_index - 1].offset, SEEK_SET);
        fwrite(&source_header, sizeof(MessageHeader), 1, file);

        // Read target message header
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        MessageHeader target_header;
        fread(&target_header, sizeof(MessageHeader), 1, file);

        // Remove forward link from target
        for (uint32_t i = 0; i < target_header.forward_link_count; i++)
        {
            if (target_header.forward_links[i] == source_index)
            {
                // Remove link by shifting remaining links
                for (uint32_t j = i; j < target_header.forward_link_count - 1; j++)
                {
                    target_header.forward_links[j] = target_header.forward_links[j + 1];
                }
                target_header.forward_link_count--;
                break;
            }
        }

        // Write updated target header
        fseek(file, index_table[target_index - 1].offset, SEEK_SET);
        fwrite(&target_header, sizeof(MessageHeader), 1, file);
    }

    fclose(file);
    return found; // Return 1 if link was found and removed, 0 otherwise
}

uint32_t *get_forward_links(uint32_t index, uint32_t *count)
{
    if (index == 0 || index > index_table_size)
    {
        *count = 0;
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        *count = 0;
        return NULL;
    }

    fseek(file, index_table[index - 1].offset, SEEK_SET);
    MessageHeader header;
    fread(&header, sizeof(MessageHeader), 1, file);
    fclose(file);

    *count = header.forward_link_count;
    if (*count == 0)
    {
        return NULL;
    }

    uint32_t *links = malloc(sizeof(uint32_t) * (*count));
    if (links == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for forward links");
        *count = 0;
        return NULL;
    }

    memcpy(links, header.forward_links, sizeof(uint32_t) * (*count));
    return links;
}

uint32_t *get_backward_links(uint32_t index, uint32_t *count)
{
    if (index == 0 || index > index_table_size)
    {
        *count = 0;
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening message file: %s", MESSAGE_FILE);
        *count = 0;
        return NULL;
    }

    fseek(file, index_table[index - 1].offset, SEEK_SET);
    MessageHeader header;
    fread(&header, sizeof(MessageHeader), 1, file);
    fclose(file);

    *count = header.backward_link_count;
    if (*count == 0)
    {
        return NULL;
    }

    uint32_t *links = malloc(sizeof(uint32_t) * (*count));
    if (links == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for backward links");
        *count = 0;
        return NULL;
    }

    memcpy(links, header.backward_links, sizeof(uint32_t) * (*count));
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
    uint32_t total_len = sizeof(MessageHeader) + message_len;
    uint32_t allocated_len = next_power_of_two(total_len);
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

    MessageHeader header = {
        .timestamp = time(NULL),
        .message_length = message_len,
        .forward_link_count = 0,
        .backward_link_count = 0
    };
    memset(header.forward_links, 0, sizeof(header.forward_links));
    memset(header.backward_links, 0, sizeof(header.backward_links));

    fwrite(&header, sizeof(MessageHeader), 1, file);
    fwrite(message, 1, message_len, file);

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

    uint32_t index = index_table_size + 1;
    index_table[index_table_size].index = index;
    index_table[index_table_size].offset = offset;
    index_table[index_table_size].length = allocated_len;
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
    uint32_t new_total_len = sizeof(MessageHeader) + new_message_len;
    uint32_t new_allocated_len = next_power_of_two(new_total_len);

    FILE *file = fopen(MESSAGE_FILE, "r+b");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening file for modification: %s", MESSAGE_FILE);
        return 0;
    }

    uint64_t offset = index_table[target_index - 1].offset;
    fseek(file, offset, SEEK_SET);

    MessageHeader header;
    fread(&header, sizeof(MessageHeader), 1, file);

    if (new_allocated_len <= index_table[target_index - 1].length)
    {
        // New message fits in the existing space
        header.message_length = new_message_len;
        fseek(file, offset, SEEK_SET);
        fwrite(&header, sizeof(MessageHeader), 1, file);
        fwrite(new_message, 1, new_message_len, file);

        // Fill remaining space with zeros
        uint32_t padding = index_table[target_index - 1].length - new_total_len;
        char *zero_pad = calloc(padding, 1);
        fwrite(zero_pad, 1, padding, file);
        free(zero_pad);
    }
    else
    {
        // New message doesn't fit, need to allocate new space
        uint64_t new_offset = find_free_space(new_allocated_len);
        if (new_offset == 0)
        {
            fclose(file);
            file = fopen(MESSAGE_FILE, "ab");
            if (file == NULL)
            {
                syslog(LOG_ERR, "Error opening message file for appending: %s", MESSAGE_FILE);
                return 0;
            }
            fseek(file, 0, SEEK_END);
            new_offset = ftell(file);
        }

        header.message_length = new_message_len;
        fwrite(&header, sizeof(MessageHeader), 1, file);
        fwrite(new_message, 1, new_message_len, file);

        // Fill remaining space with zeros
        uint32_t padding = new_allocated_len - new_total_len;
        char *zero_pad = calloc(padding, 1);
        fwrite(zero_pad, 1, padding, file);
        free(zero_pad);

        // Add old space to free space table
        add_free_space(index_table[target_index - 1].offset, index_table[target_index - 1].length);

        // Update index table
        index_table[target_index - 1].offset = new_offset;
        index_table[target_index - 1].length = new_allocated_len;
        save_index_table();
    }

    fclose(file);
    return 1; // Modification successful
}

// 인덱스 테이블 정보를 JSON 형식으로 반환하는 함수
//get_index_table_info 함수 수정
char *get_index_table_info()
{
    json_object *index_array = json_object_new_array();

    for (uint32_t i = 0; i < index_table_size; i++)
    {
        json_object *entry = json_object_new_object();
        json_object_object_add(entry, "index", json_object_new_int(index_table[i].index));
        json_object_object_add(entry, "offset", json_object_new_int64(index_table[i].offset));
        json_object_object_add(entry, "length", json_object_new_int(index_table[i].length));
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
char *get_message_by_index_and_format(uint32_t target_index, const char* format)
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
    uint32_t total_length = index_table[target_index - 1].length;

    if (fseek(file, offset, SEEK_SET) != 0)
    {
        syslog(LOG_ERR, "Error seeking to message position");
        fclose(file);
        return NULL;
    }

    // Read the entire allocated space
    char *buffer = malloc(total_length);
    if (buffer == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for message buffer");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, total_length, file);
    fclose(file);

    if (read_size != total_length)
    {
        syslog(LOG_ERR, "Error reading message data: expected %u bytes, got %zu bytes", total_length, read_size);
        free(buffer);
        return NULL;
    }

    // Extract header and message from the buffer
    MessageHeader *header = (MessageHeader *)buffer;
    char *message = buffer + sizeof(MessageHeader);

    // Sanity check
    if (header->message_length > total_length - sizeof(MessageHeader))
    {
        syslog(LOG_ERR, "Corrupted message header: message length exceeds allocated space");
        free(buffer);
        return NULL;
    }

    // Format the output as requested
    char *result;
    if (strcmp(format, "text") == 0)
    {
        result = malloc(header->message_length + 1);
        if (result == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for result string");
            free(buffer);
            return NULL;
        }
        memcpy(result, message, header->message_length);
        result[header->message_length] = '\0';
    }
    else if (strcmp(format, "binary") == 0 || strcmp(format, "hex") == 0)
    {
        result = malloc(header->message_length * 2 + 1);
        if (result == NULL)
        {
            syslog(LOG_ERR, "Memory allocation failed for hex string");
            free(buffer);
            return NULL;
        }
        for (uint32_t i = 0; i < header->message_length; i++)
        {
            sprintf(result + (i * 2), "%02x", (unsigned char)message[i]);
        }
        result[header->message_length * 2] = '\0';
    }
    else
    {
        syslog(LOG_ERR, "Unknown format requested: %s", format);
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