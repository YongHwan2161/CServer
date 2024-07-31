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
#include <signal.h>
#include <syslog.h>
#include <errno.h>
#include <json-c/json.h>
#include <dirent.h>

// ... (기존 코드 유지)

void handle_list_files(SSL *ssl) {
    DIR *d;
    struct dirent *dir;
    d = opendir(".");
    if (d) {
        json_object *file_array = json_object_new_array();

        while ((dir = readdir(d)) != NULL) {
            if (dir->d_type == DT_REG) {  // 일반 파일만 포함
                json_object_array_add(file_array, json_object_new_string(dir->d_name));
            }
        }
        closedir(d);

        json_object *response_obj = json_object_new_object();
        json_object_object_add(response_obj, "action", json_object_new_string("file_list"));
        json_object_object_add(response_obj, "files", file_array);

        const char *response_str = json_object_to_json_string(response_obj);
        websocket_write(ssl, response_str, strlen(response_str));

        json_object_put(response_obj);
    } else {
        websocket_write(ssl, "{\"action\":\"file_list\",\"files\":[]}", -1);
    }
}

void handle_file_read(SSL *ssl, const char *filename) {
    // ... (기존 코드 유지)
}

void handle_file_save(SSL *ssl, const char *filename, const char *content) {
    // ... (기존 코드 유지)
}

void *handle_client(void *ssl_ptr) {
    SSL *ssl = (SSL *)ssl_ptr;
    char buf[BUFFER_SIZE];
    int bytes;

    // ... (기존 WebSocket 핸드셰이크 코드 유지)

    while (keep_running) {
        bytes = websocket_read(ssl, buf, sizeof(buf) - 1);
        if (bytes <= 0) {
            if (bytes == 0) {
                syslog(LOG_INFO, "WebSocket connection closed by client");
            } else {
                log_error("Error reading from WebSocket");
            }
            break;
        }

        struct json_object *parsed_json;
        parsed_json = json_tokener_parse(buf);

        struct json_object *action_obj;
        if (json_object_object_get_ex(parsed_json, "action", &action_obj)) {
            const char *action = json_object_get_string(action_obj);

            if (strcmp(action, "list_files") == 0) {
                handle_list_files(ssl);
            } else if (strcmp(action, "read_file") == 0) {
                struct json_object *filename_obj;
                if (json_object_object_get_ex(parsed_json, "filename", &filename_obj)) {
                    const char *filename = json_object_get_string(filename_obj);
                    handle_file_read(ssl, filename);
                }
            } else if (strcmp(action, "save_file") == 0) {
                struct json_object *filename_obj, *content_obj;
                if (json_object_object_get_ex(parsed_json, "filename", &filename_obj) &&
                    json_object_object_get_ex(parsed_json, "content", &content_obj)) {
                    const char *filename = json_object_get_string(filename_obj);
                    const char *content = json_object_get_string(content_obj);
                    handle_file_save(ssl, filename, content);
                }
            } else if (strcmp(action, "build") == 0) {
                handle_build(ssl);
            } else if (strcmp(action, "run") == 0) {
                handle_run(ssl);
            }
        }

        json_object_put(parsed_json);
    }

    SSL_shutdown(ssl);
    SSL_free(ssl);
    pthread_exit(NULL);
}

// ... (main 함수와 나머지 코드 유지)