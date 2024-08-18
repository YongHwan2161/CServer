#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <pthread.h>
#include <signal.h> // 이 줄을 추가합니다
#include <syslog.h>
#include <errno.h> // errno를 사용하므로 이 헤더도 추가합니다
#include <json-c/json.h>
#include <dirent.h>
#include <sys/stat.h>
#include <linux/limits.h>
#include <time.h>

#define MAX_CLIENTS 10
#define BUFFER_SIZE 4096
#define MAX_FILENAME_LENGTH 100
#define MESSAGE_FILE "binary file/messages.bin"
#define INDEX_FILE "binary file/index.bin"
#define FREE_SPACE_FILE "binary file/free_space.bin"
#define MAX_MESSAGES 1000000 // 최대 메시지 수를 정의합니다
#define MIN_BLOCK_SIZE 64  // 최소 블록 크기 (바이트)
#define MAX_ORDER 10       // 최대 오더 (2^10 = 1024 * MIN_BLOCK_SIZE = 64KB)

typedef struct
{
    int port;
    char *cert_file;
    char *key_file;
} ServerConfig;
typedef struct
{
    uint32_t index;
    uint64_t offset;
    uint32_t length; // 메시지 길이 추가
} IndexEntry;
typedef struct
{
    uint64_t offset;
    uint32_t length;
} FreeSpaceEntry;

ServerConfig config = {8443, "cert.pem", "key.pem"};
volatile sig_atomic_t keep_running = 1;
// 전역 변수로 인덱스 테이블을 선언합니다
IndexEntry *index_table = NULL;
uint32_t index_table_size = 0;
FreeSpaceEntry *free_space_table = NULL;
uint32_t free_space_table_size = 0;

void handle_signal(int sig)
{
    keep_running = 0;
}
void setup_signal_handlers()
{
    if (signal(SIGINT, handle_signal) == SIG_ERR)
    {
        perror("Error setting up SIGINT handler");
        exit(EXIT_FAILURE);
    }
    if (signal(SIGTERM, handle_signal) == SIG_ERR)
    {
        perror("Error setting up SIGTERM handler");
        exit(EXIT_FAILURE);
    }
}
void log_error(const char *msg)
{
    syslog(LOG_ERR, "%s: %s", msg, strerror(errno));
    fprintf(stderr, "%s: %s\n", msg, strerror(errno));
}
int create_socket(int port)
{
    int s;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        log_error("Unable to create socket");
        exit(EXIT_FAILURE);
    }

    int enable = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
    {
        log_error("setsockopt(SO_REUSEADDR) failed");
    }

    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        log_error("Unable to bind");
        exit(EXIT_FAILURE);
    }

    if (listen(s, MAX_CLIENTS) < 0)
    {
        log_error("Unable to listen");
        exit(EXIT_FAILURE);
    }

    return s;
}

SSL_CTX *create_context()
{
    const SSL_METHOD *method;
    SSL_CTX *ctx;

    method = TLS_server_method();
    ctx = SSL_CTX_new(method);
    if (!ctx)
    {
        log_error("Unable to create SSL context");
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

void configure_context(SSL_CTX *ctx)
{
    if (SSL_CTX_use_certificate_file(ctx, config.cert_file, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    if (SSL_CTX_use_PrivateKey_file(ctx, config.key_file, SSL_FILETYPE_PEM) <= 0)
    {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }
}

// ... (rest of the helper functions like send_file, generate_websocket_key, etc. remain the same)
void send_file(SSL *ssl, const char *filename)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        const char *not_found = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        SSL_write(ssl, not_found, strlen(not_found));
        return;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(fsize + 1);
    if (content == NULL)
    {
        fclose(file);
        log_error("Failed to allocate memory for file content");
        return;
    }

    size_t bytes_read = fread(content, 1, fsize, file);
    fclose(file);

    if (bytes_read != fsize)
    {
        free(content);
        log_error("Failed to read entire file");
        return;
    }

    content[fsize] = 0;

    const char *content_type;
    if (strstr(filename, ".html") != NULL)
    {
        content_type = "text/html";
    }
    else if (strstr(filename, ".js") != NULL)
    {
        content_type = "text/javascript"; // Changed from "application/javascript"
    }
    else
    {
        content_type = "text/plain";
    }

    char header[1024];
    int header_len = snprintf(header, sizeof(header),
                              "HTTP/1.1 200 OK\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %ld\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "\r\n",
                              content_type, fsize);

    if (SSL_write(ssl, header, header_len) <= 0)
    {
        log_error("Failed to send HTTP header");
        free(content);
        return;
    }

    if (SSL_write(ssl, content, fsize) <= 0)
    {
        log_error("Failed to send file content");
    }

    free(content);
}
void generate_websocket_key(const char *client_key, char *accept_key)
{
    const char *magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char combined[1024];
    unsigned char hash[SHA_DIGEST_LENGTH];
    char base64_hash[32];

    snprintf(combined, sizeof(combined), "%s%s", client_key, magic);
    SHA1((unsigned char *)combined, strlen(combined), hash);
    EVP_EncodeBlock((unsigned char *)base64_hash, hash, SHA_DIGEST_LENGTH);

    strcpy(accept_key, base64_hash);
}
int handle_websocket_handshake(SSL *ssl, const char *buf)
{
    char *key_start = strstr(buf, "Sec-WebSocket-Key: ") + 19;
    char *key_end = strstr(key_start, "\r\n");
    char client_key[25];
    char accept_key[29];
    char response[1024];

    strncpy(client_key, key_start, key_end - key_start);
    client_key[key_end - key_start] = '\0';

    generate_websocket_key(client_key, accept_key);

    snprintf(response, sizeof(response),
             "HTTP/1.1 101 Switching Protocols\r\n"
             "Upgrade: websocket\r\n"
             "Connection: Upgrade\r\n"
             "Sec-WebSocket-Accept: %s\r\n\r\n",
             accept_key);

    return SSL_write(ssl, response, strlen(response));
}
int websocket_read(SSL *ssl, char *buf, int len)
{
    unsigned char header[2];
    int header_len = SSL_read(ssl, header, 2);
    if (header_len <= 0)
        return header_len;

    int opcode = header[0] & 0x0F;
    int mask = header[1] & 0x80;
    int payload_len = header[1] & 0x7F;
    unsigned char mask_key[4];

    if (payload_len == 126)
    {
        unsigned char extended_len[2];
        SSL_read(ssl, extended_len, 2);
        payload_len = (extended_len[0] << 8) | extended_len[1];
    }
    else if (payload_len == 127)
    {
        unsigned char extended_len[8];
        SSL_read(ssl, extended_len, 8);
        // 64-bit integer handling omitted for simplicity
    }

    if (mask)
    {
        SSL_read(ssl, mask_key, 4);
    }

    int bytes = SSL_read(ssl, buf, payload_len);
    if (mask)
    {
        for (int i = 0; i < bytes; i++)
        {
            buf[i] ^= mask_key[i % 4];
        }
    }

    buf[bytes] = '\0';
    return bytes;
}
int websocket_write(SSL *ssl, const char *buf, int len)
{
    unsigned char header[2] = {0x81, 0x00};
    if (len <= 125)
    {
        header[1] = len;
    }
    else if (len <= 65535)
    {
        header[1] = 126;
    }
    else
    {
        header[1] = 127;
    }

    SSL_write(ssl, header, 2);

    if (len > 125 && len <= 65535)
    {
        unsigned char extended_len[2];
        extended_len[0] = (len >> 8) & 0xFF;
        extended_len[1] = len & 0xFF;
        SSL_write(ssl, extended_len, 2);
    }
    else if (len > 65535)
    {
        // 64-bit integer handling omitted for simplicity
    }

    return SSL_write(ssl, buf, len);
}
json_object *list_directory_contents(const char *base_path, const char *rel_path)
{
    char full_path[PATH_MAX];
    size_t base_len = strlen(base_path);
    size_t rel_len = strlen(rel_path);

    if (base_len + rel_len + 2 > sizeof(full_path))
    {
        // Path too long, return error
        return NULL;
    }

    if (rel_len == 0)
    {
        strncpy(full_path, base_path, sizeof(full_path) - 1);
        full_path[sizeof(full_path) - 1] = '\0';
    }
    else
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", base_path, rel_path);
    }

    DIR *dir = opendir(full_path);
    if (!dir)
    {
        return NULL;
    }

    json_object *items_array = json_object_new_array();
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            continue;
        }

        char item_path[PATH_MAX];
        size_t full_path_len = strlen(full_path);
        size_t entry_name_len = strlen(entry->d_name);

        if (full_path_len + entry_name_len + 2 > sizeof(item_path))
        {
            // Path would be too long, skip this item
            continue;
        }

        memcpy(item_path, full_path, full_path_len);
        item_path[full_path_len] = '/';
        memcpy(item_path + full_path_len + 1, entry->d_name, entry_name_len);
        item_path[full_path_len + entry_name_len + 1] = '\0';

        struct stat st;
        if (stat(item_path, &st) == 0)
        {
            json_object *item_obj = json_object_new_object();
            json_object_object_add(item_obj, "name", json_object_new_string(entry->d_name));

            char rel_item_path[PATH_MAX];
            size_t rel_path_len = strlen(rel_path);

            if (rel_path_len + entry_name_len + 2 > sizeof(rel_item_path))
            {
                // Relative path would be too long, skip this item
                json_object_put(item_obj);
                continue;
            }

            if (rel_path_len > 0)
            {
                memcpy(rel_item_path, rel_path, rel_path_len);
                rel_item_path[rel_path_len] = '/';
                memcpy(rel_item_path + rel_path_len + 1, entry->d_name, entry_name_len);
                rel_item_path[rel_path_len + entry_name_len + 1] = '\0';
            }
            else
            {
                memcpy(rel_item_path, entry->d_name, entry_name_len);
                rel_item_path[entry_name_len] = '\0';
            }

            json_object_object_add(item_obj, "path", json_object_new_string(rel_item_path));

            if (S_ISDIR(st.st_mode))
            {
                json_object_object_add(item_obj, "type", json_object_new_string("directory"));
            }
            else if (S_ISREG(st.st_mode))
            {
                json_object_object_add(item_obj, "type", json_object_new_string("file"));
            }

            json_object_array_add(items_array, item_obj);
        }
    }

    closedir(dir);
    return items_array;
}
void handle_list_files(SSL *ssl, const char *path)
{
    json_object *response_obj = json_object_new_object();
    json_object_object_add(response_obj, "action", json_object_new_string("file_list"));
    json_object_object_add(response_obj, "path", json_object_new_string(path));

    char full_path[PATH_MAX];
    if (strlen(path) == 0)
    {
        strcpy(full_path, ".");
    }
    else
    {
        snprintf(full_path, sizeof(full_path), "./%s", path);
    }

    json_object *items = list_directory_contents(".", path);
    if (items)
    {
        json_object_object_add(response_obj, "items", items);
    }
    else
    {
        json_object_object_add(response_obj, "error", json_object_new_string("Unable to list directory contents"));
    }

    const char *response_str = json_object_to_json_string(response_obj);
    websocket_write(ssl, response_str, strlen(response_str));

    json_object_put(response_obj);
}
void handle_file_read(SSL *ssl, const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        char response[1024];
        snprintf(response, sizeof(response), "{\"action\":\"file_content\",\"content\":\"Error: Unable to read file %s\"}", filename);
        websocket_write(ssl, response, strlen(response));
        return;
    }

    fseek(file, 0, SEEK_END);
    long fsize = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *content = malloc(fsize + 1);
    fread(content, fsize, 1, file);
    fclose(file);

    content[fsize] = 0;

    const char *escaped_content = json_object_to_json_string_ext(json_object_new_string(content), JSON_C_TO_STRING_PLAIN);
    char *response = malloc(strlen(escaped_content) + 100);
    snprintf(response, strlen(escaped_content) + 100, "{\"action\":\"file_content\",\"content\":%s}", escaped_content);

    websocket_write(ssl, response, strlen(response));

    free(content);
    free(response);
}
void handle_file_save(SSL *ssl, const char *filename, const char *content)
{
    FILE *file = fopen(filename, "w");
    if (file == NULL)
    {
        char response[1024];
        snprintf(response, sizeof(response), "{\"action\":\"save_result\",\"content\":\"Error: Unable to save file %s\"}", filename);
        websocket_write(ssl, response, strlen(response));
        return;
    }

    fputs(content, file);
    fclose(file);

    char response[1024];
    snprintf(response, sizeof(response), "{\"action\":\"save_result\",\"content\":\"File %s saved successfully\"}", filename);
    websocket_write(ssl, response, strlen(response));
}
void handle_build(SSL *ssl)
{
    char command[1024];
    snprintf(command, sizeof(command), "gcc -o server server.c -lssl -lcrypto -lpthread -ljson-c 2>&1");

    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        websocket_write(ssl, "{\"action\":\"build_result\",\"content\":\"Error: Unable to run build command\"}", -1);
        return;
    }

    char buffer[4096] = {0};  // 더 큰 버퍼 사용
    char *output = malloc(1); // 동적으로 할당된 출력 버퍼
    output[0] = '\0';
    size_t output_size = 1;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        output_size += strlen(buffer);
        output = realloc(output, output_size);
        strcat(output, buffer);
    }

    pclose(fp);

    const char *escaped_output = json_object_to_json_string_ext(json_object_new_string(output), JSON_C_TO_STRING_PLAIN);
    char *response = malloc(strlen(escaped_output) + 100);
    snprintf(response, strlen(escaped_output) + 100, "{\"action\":\"build_result\",\"content\":%s}", escaped_output);

    websocket_write(ssl, response, strlen(response));

    free(output);
    free(response);
}
void handle_run(SSL *ssl)
{
    char command[1024];
    snprintf(command, sizeof(command), "./server 2>&1");

    FILE *fp = popen(command, "r");
    if (fp == NULL)
    {
        websocket_write(ssl, "{\"action\":\"run_output\",\"content\":\"Error: Unable to run server\"}", -1);
        return;
    }

    char buffer[4096] = {0};  // 더 큰 버퍼 사용
    char *output = malloc(1); // 동적으로 할당된 출력 버퍼
    output[0] = '\0';
    size_t output_size = 1;

    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        output_size += strlen(buffer);
        output = realloc(output, output_size);
        strcat(output, buffer);
    }

    pclose(fp);

    const char *escaped_output = json_object_to_json_string_ext(json_object_new_string(output), JSON_C_TO_STRING_PLAIN);
    char *response = malloc(strlen(escaped_output) + 100);
    snprintf(response, strlen(escaped_output) + 100, "{\"action\":\"run_output\",\"content\":%s}", escaped_output);

    websocket_write(ssl, response, strlen(response));

    free(output);
    free(response);
}
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
            syslog(LOG_ERR, "Error creating index file: %s", INDEX_FILE);
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
            syslog(LOG_ERR, "Error allocating memory for index table");
            exit(EXIT_FAILURE);
        }
        index_table_size = 0;
        syslog(LOG_INFO, "Created new index file and initialized index table");
        return;
    }

    // 기존 파일에서 인덱스 테이블 크기를 읽습니다.
    if (fread(&index_table_size, sizeof(uint32_t), 1, file) != 1)
    {
        syslog(LOG_ERR, "Error reading index table size from file");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // 인덱스 테이블 메모리 할당
    index_table = malloc(sizeof(IndexEntry) * MAX_MESSAGES);
    if (index_table == NULL)
    {
        syslog(LOG_ERR, "Error allocating memory for index table");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // 인덱스 테이블 데이터 읽기
    if (fread(index_table, sizeof(IndexEntry), index_table_size, file) != index_table_size)
    {
        syslog(LOG_ERR, "Error reading index table from file");
        fclose(file);
        free(index_table);
        exit(EXIT_FAILURE);
    }

    fclose(file);
    syslog(LOG_INFO, "Loaded index table with %u entries", index_table_size);
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
void save_index_table()
{
    FILE *file = fopen(INDEX_FILE, "wb");
    if (file == NULL)
    {
        syslog(LOG_ERR, "Error opening index file for writing: %s", INDEX_FILE);
        return;
    }

    // 인덱스 테이블 크기를 씁니다.
    fwrite(&index_table_size, sizeof(uint32_t), 1, file);

    // 인덱스 테이블 데이터를 씁니다.
    fwrite(index_table, sizeof(IndexEntry), index_table_size, file);

    fclose(file);
    syslog(LOG_INFO, "Saved index table with %u entries", index_table_size);
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
uint32_t next_power_of_two(uint32_t v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v < 16 ? 16 : v;  // 최소 크기를 16으로 설정
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
    uint32_t allocated_len = next_power_of_two(total_len);  // 2의 거듭제곱 크기로 할당
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
char* get_index_table_info() {
    json_object *index_array = json_object_new_array();
    
    for (uint32_t i = 0; i < index_table_size; i++) {
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
char* get_free_space_table_info() {
    json_object *free_space_array = json_object_new_array();
    
    for (uint32_t i = 0; i < free_space_table_size; i++) {
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
char* get_binary_data_by_index(uint32_t target_index) {
    if (target_index == 0 || target_index > index_table_size) {
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file for reading: %s", MESSAGE_FILE);
        return NULL;
    }

    uint64_t offset = index_table[target_index - 1].offset;
    uint32_t length = index_table[target_index - 1].length;
    fseek(file, offset, SEEK_SET);

    unsigned char *buffer = malloc(length);
    if (buffer == NULL) {
        syslog(LOG_ERR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length) {
        syslog(LOG_ERR, "Error reading full message data");
        free(buffer);
        return NULL;
    }

    char *hex_string = malloc(length * 2 + 1);
    if (hex_string == NULL) {
        syslog(LOG_ERR, "Memory allocation failed for hex string");
        free(buffer);
        return NULL;
    }

    for (uint32_t i = 0; i < length; i++) {
        sprintf(hex_string + (i * 2), "%02x", buffer[i]);
    }
    hex_string[length * 2] = '\0';

    free(buffer);
    return hex_string;
}
// 수정된 함수: 특정 인덱스의 메시지를 지정된 형식으로 반환
char* get_message_by_index_and_format(uint32_t target_index, const char* format) {
    if (target_index == 0 || target_index > index_table_size) {
        return NULL;
    }

    FILE *file = fopen(MESSAGE_FILE, "rb");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file for reading: %s", MESSAGE_FILE);
        return NULL;
    }

    uint64_t offset = index_table[target_index - 1].offset;
    uint32_t length = index_table[target_index - 1].length;
    fseek(file, offset, SEEK_SET);

    unsigned char *buffer = malloc(length);
    if (buffer == NULL) {
        syslog(LOG_ERR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }

    size_t read_size = fread(buffer, 1, length, file);
    fclose(file);

    if (read_size != length) {
        syslog(LOG_ERR, "Error reading full message data");
        free(buffer);
        return NULL;
    }

    char *result;
    if (strcmp(format, "text") == 0) {
        result = malloc(length + 1);
        if (result == NULL) {
            syslog(LOG_ERR, "Memory allocation failed for text result");
            free(buffer);
            return NULL;
        }
        memcpy(result, buffer, length);
        result[length] = '\0';
    } else if (strcmp(format, "binary") == 0 || strcmp(format, "hex") == 0) {
        result = malloc(length * 2 + 1);
        if (result == NULL) {
            syslog(LOG_ERR, "Memory allocation failed for hex result");
            free(buffer);
            return NULL;
        }
        for (uint32_t i = 0; i < length; i++) {
            sprintf(result + (i * 2), "%02x", buffer[i]);
        }
        result[length * 2] = '\0';
    } else {
        syslog(LOG_ERR, "Unknown format requested");
        free(buffer);
        return NULL;
    }

    free(buffer);
    return result;
}
void handle_message(SSL *ssl, const char *message)
{
    char *response;
    
    if (strcmp(message, "get_index_table_info") == 0) {
        response = get_index_table_info();
    } else if (strcmp(message, "get_free_space_table_info") == 0) {
        response = get_free_space_table_info();
    }  else if (strncmp(message, "get:", 4) == 0) {
        char *index_str = strtok((char *)message + 4, ":");
        char *format = strtok(NULL, ":");
        if (index_str != NULL && format != NULL) {
            uint32_t index = atoi(index_str);
            char *content = get_message_by_index_and_format(index, format);
            if (content != NULL) {
                response = malloc(strlen(content) + 256);
                snprintf(response, strlen(content) + 256, 
                         "{\"action\":\"message_response\",\"content\":\"%s\",\"format\":\"%s\"}", 
                         content, format);
                free(content);
            } else {
                response = strdup("{\"action\":\"message_response\",\"content\":\"Error: Message not found\",\"format\":\"text\"}");
            }
        } else {
            response = strdup("{\"action\":\"message_response\",\"content\":\"Error: Invalid get command format\",\"format\":\"text\"}");
        }
    } else if (message[0] == '/') {
        // '/'로 시작하는 경우, '/' 다음의 문자열을 파일에 저장
        uint32_t saved_index = append_message_to_file(message + 1);
        if (saved_index > 0) {
            response = malloc(256);
            snprintf(response, 256, "{\"action\":\"message_response\",\"content\":\"Message saved to file with index: %u\"}", saved_index);
        } else {
            response = strdup("{\"action\":\"message_response\",\"content\":\"Error: Failed to save message\"}");
        }
    } else if (strncmp(message, "modify:", 7) == 0) {
        // "modify:" 접두사로 시작하는 경우, 해당 인덱스의 메시지를 수정
        char *index_str = strtok((char *)message + 7, ":");
        char *new_message = strtok(NULL, "");
        if (index_str != NULL && new_message != NULL) {
            uint32_t index = atoi(index_str);
            if (modify_message_by_index(index, new_message)) {
                response = malloc(256);
                snprintf(response, 256, "{\"action\":\"message_response\",\"content\":\"Message with index %u modified successfully\"}", index);
            } else {
                response = malloc(256);
                snprintf(response, 256, "{\"action\":\"message_response\",\"content\":\"Error: Failed to modify message with index %u\"}", index);
            }
        } else {
            response = strdup("{\"action\":\"message_response\",\"content\":\"Error: Invalid modify command format\"}");
        }
    } else {
        // 그 외의 경우, echo 응답
        response = malloc(strlen(message) + 256);
        snprintf(response, strlen(message) + 256, "{\"action\":\"message_response\",\"content\":\"Server received: %s\"}", message);
    }
    
    websocket_write(ssl, response, strlen(response));
    free(response);
}

void *handle_client(void *ssl_ptr)
{
    SSL *ssl = (SSL *)ssl_ptr;
    char buf[BUFFER_SIZE];
    int bytes;

    bytes = SSL_read(ssl, buf, sizeof(buf) - 1);
    if (bytes <= 0)
    {
        log_error("Error reading from client");
        goto cleanup;
    }
    buf[bytes] = '\0';

    if (strstr(buf, "GET / ") && strstr(buf, "HTTP/1.1"))
    {
        send_file(ssl, "assets/html/index2.html");
    }
    else if (strstr(buf, "GET /assets/js/app.js") && strstr(buf, "HTTP/1.1"))
    {
        send_file(ssl, "assets/js/app.js");
    }
    else if (strstr(buf, "GET") && strstr(buf, "Upgrade: websocket"))
    {
        if (handle_websocket_handshake(ssl, buf) > 0)
        {
            syslog(LOG_INFO, "WebSocket connection established");
            while (keep_running)
            {
                bytes = websocket_read(ssl, buf, sizeof(buf) - 1);
                if (bytes <= 0)
                {
                    if (bytes == 0)
                    {
                        syslog(LOG_INFO, "WebSocket connection closed by client");
                    }
                    else
                    {
                        log_error("Error reading from WebSocket");
                    }
                    break;
                }
                struct json_object *parsed_json;
                parsed_json = json_tokener_parse(buf);

                struct json_object *action_obj;
                if (json_object_object_get_ex(parsed_json, "action", &action_obj))
                {
                    const char *action = json_object_get_string(action_obj);

                    if (strcmp(action, "list_files") == 0)
                    {
                        struct json_object *path_obj;
                        const char *path = "";
                        if (json_object_object_get_ex(parsed_json, "path", &path_obj))
                        {
                            path = json_object_get_string(path_obj);
                        }
                        handle_list_files(ssl, path);
                    }
                    else if (strcmp(action, "read_file") == 0)
                    {
                        struct json_object *filename_obj;
                        if (json_object_object_get_ex(parsed_json, "filename", &filename_obj))
                        {
                            const char *filename = json_object_get_string(filename_obj);
                            handle_file_read(ssl, filename);
                        }
                    }
                    else if (strcmp(action, "save_file") == 0)
                    {
                        struct json_object *filename_obj, *content_obj;
                        if (json_object_object_get_ex(parsed_json, "filename", &filename_obj) &&
                            json_object_object_get_ex(parsed_json, "content", &content_obj))
                        {
                            const char *filename = json_object_get_string(filename_obj);
                            const char *content = json_object_get_string(content_obj);
                            handle_file_save(ssl, filename, content);
                        }
                    }
                    else if (strcmp(action, "build") == 0)
                    {
                        handle_build(ssl);
                    }
                    else if (strcmp(action, "run") == 0)
                    {
                        handle_run(ssl);
                    }
                    else if (strcmp(action, "message") == 0)
                    {
                        struct json_object *content_obj;
                        if (json_object_object_get_ex(parsed_json, "content", &content_obj))
                        {
                            const char *content = json_object_get_string(content_obj);
                            handle_message(ssl, content);
                        }
                    }
                }
                json_object_put(parsed_json);
            }
        }
        else
        {
            log_error("WebSocket handshake failed");
        }
    }
    else
    {
        const char *response =
            "HTTP/1.1 404 Not Found\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 13\r\n"
            "\r\n"
            "404 Not Found";
        SSL_write(ssl, response, strlen(response));
    }

cleanup:
    SSL_shutdown(ssl);
    SSL_free(ssl);
    pthread_exit(NULL);
}
// 메모리 해제 함수
void cleanup()
{
    if (index_table != NULL)
    {
        free(index_table);
        index_table = NULL;
    }
    if (free_space_table != NULL)
    {
        free(free_space_table);
        free_space_table = NULL;
    }
    syslog(LOG_INFO, "Cleaned up resources");
}

int main()
{
    int sock;
    SSL_CTX *ctx;
    // 인덱스 테이블과 free space 테이블 초기화
    initialize_index_table();
    initialize_free_space_table();

    // 메시지 파일이 존재하지 않으면 생성
    FILE *msg_file = fopen(MESSAGE_FILE, "ab");
    if (msg_file == NULL)
    {
        syslog(LOG_ERR, "Error creating message file: %s", MESSAGE_FILE);
        exit(EXIT_FAILURE);
    }
    fclose(msg_file);

    openlog("https_websocket_server", LOG_PID | LOG_CONS, LOG_USER);
    syslog(LOG_INFO, "Server starting...");

    setup_signal_handlers();

    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();

    ctx = create_context();
    configure_context(ctx);

    sock = create_socket(config.port);

    syslog(LOG_INFO, "Server started on port %d", config.port);

    while (keep_running)
    {
        struct sockaddr_in addr;
        unsigned int len = sizeof(addr);
        SSL *ssl;

        int client = accept(sock, (struct sockaddr *)&addr, &len);
        if (client < 0)
        {
            log_error("Unable to accept");
            continue;
        }

        ssl = SSL_new(ctx);
        SSL_set_fd(ssl, client);

        if (SSL_accept(ssl) <= 0)
        {
            ERR_print_errors_fp(stderr);
        }
        else
        {
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_client, ssl) != 0)
            {
                log_error("Failed to create thread");
                SSL_free(ssl);
                close(client);
            }
            else
            {
                pthread_detach(thread);
            }
        }
    }

    syslog(LOG_INFO, "Server shutting down...");
    // 프로그램 종료 시 정리 작업 수행
    atexit(cleanup);
    close(sock);
    SSL_CTX_free(ctx);
    EVP_cleanup();
    closelog();

    return 0;
}