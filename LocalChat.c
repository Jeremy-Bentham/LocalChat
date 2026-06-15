#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <windows.h>
    typedef SOCKET socket_t;
    #define CLOSE_SOCKET closesocket
    #define SHUTDOWN_SOCKET shutdown
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/in.h>
    #include <unistd.h>
    #include <pthread.h>
    typedef int socket_t;
    #define CLOSE_SOCKET close
    #define SHUTDOWN_SOCKET shutdown
    #define INVALID_SOCKET -1
#endif

#define PORT 6847
#define BUF_SIZE 8192
#define MAX_FILENAME 256

socket_t listen_sock = INVALID_SOCKET;
socket_t client_sock = INVALID_SOCKET;
int running = 1;

#ifdef _WIN32
HANDLE recv_thread;
#else
pthread_t recv_thread;
#endif

// Receive exactly 'size' bytes (handles partial receives)
int recv_exact(socket_t sock, char* buffer, size_t size) {
    size_t received = 0;
    while (received < size) {
        int r = recv(sock, buffer + received, size - received, 0);
        if (r <= 0) return r;
        received += r;
    }
    return received;
}

void* receive_thread(void* arg) {
    char buffer[BUF_SIZE];

    while (running) {
        // Read first byte (type)
        if (recv_exact(client_sock, buffer, 1) <= 0) break;

        if (buffer[0] == 'T') {  // Text
            int bytes = recv(client_sock, buffer, BUF_SIZE - 1, 0);
            if (bytes <= 0) break;
            buffer[bytes] = '\0';
            printf("\n[Peer]: %s\n> ", buffer);
        }
        else if (buffer[0] == 'F') {  // File
            uint32_t name_len, file_size;

            if (recv_exact(client_sock, (char*)&name_len, 4) <= 0) break;
            if (recv_exact(client_sock, (char*)&file_size, 4) <= 0) break;

            name_len = ntohl(name_len);
            file_size = ntohl(file_size);

            if (name_len >= MAX_FILENAME) {
                printf("\n[Error: Filename too long]\n> ");
                break;
            }

            char filename[MAX_FILENAME];
            if (recv_exact(client_sock, filename, name_len) <= 0) break;
            filename[name_len] = '\0';

            printf("\n[Receiving file: %s (%u bytes)]\n", filename, file_size);

            FILE* f = fopen(filename, "wb");
            if (!f) {
                printf("[Failed to open file for writing]\n> ");
                // drain remaining data
                while (file_size > 0) {
                    int r = recv(client_sock, buffer, BUF_SIZE, 0);
                    if (r <= 0) break;
                    file_size -= r;
                }
                continue;
            }

            uint32_t received = 0;
            while (received < file_size) {
                int r = recv(client_sock, buffer, BUF_SIZE, 0);
                if (r <= 0) break;
                fwrite(buffer, 1, r, f);
                received += r;
            }
            fclose(f);
            printf("[File received successfully: %s]\n> ", filename);
        }
        fflush(stdout);
    }
    printf("\n[Connection closed by peer]\n");
    running = 0;
    return NULL;
}

void send_text(const char* msg) {
    char buffer[BUF_SIZE];
    buffer[0] = 'T';
    strncpy(buffer + 1, msg, BUF_SIZE - 2);
    send(client_sock, buffer, strlen(msg) + 1, 0);
}

void send_file(const char* filepath) {
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        printf("Cannot open file: %s\n", filepath);
        return;
    }

    fseek(f, 0, SEEK_END);
    uint32_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    const char* filename = strrchr(filepath, '/');
    if (!filename) filename = strrchr(filepath, '\\');
    if (!filename) filename = filepath;
    else filename++;

    uint32_t name_len = strlen(filename);

    // Send header
    char header[BUF_SIZE];
    header[0] = 'F';
    uint32_t nl = htonl(name_len);
    uint32_t fs = htonl(file_size);
    memcpy(header + 1, &nl, 4);
    memcpy(header + 5, &fs, 4);
    memcpy(header + 9, filename, name_len);

    send(client_sock, header, 9 + name_len, 0);

    // Send file data
    char buffer[BUF_SIZE];
    size_t bytes;
    while ((bytes = fread(buffer, 1, BUF_SIZE, f)) > 0) {
        send(client_sock, buffer, bytes, 0);
    }
    fclose(f);
    printf("[File sent: %s]\n", filename);
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        goto cleanup;
    }

    if (argc == 1) {  // Server
        addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Bind failed (port %d may be in use)\n", PORT);
            goto cleanup;
        }
        listen(listen_sock, 1);
        printf("Waiting for connection on port %d...\n", PORT);

        struct sockaddr_in client_addr;
        socklen_t len = sizeof(client_addr);
        client_sock = accept(listen_sock, (struct sockaddr*)&client_addr, &len);
        if (client_sock == INVALID_SOCKET) goto cleanup;
        printf("Peer connected from %s!\n", inet_ntoa(client_addr.sin_addr));
    } 
    else {  // Client
        addr.sin_addr.s_addr = inet_addr(argv[1]);
        client_sock = listen_sock;
        if (connect(client_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            printf("Connection failed to %s\n", argv[1]);
            goto cleanup;
        }
        printf("Connected to %s\n", argv[1]);
    }

    // Start receiver thread
#ifdef _WIN32
    recv_thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)receive_thread, NULL, 0, NULL);
#else
    pthread_create(&recv_thread, NULL, receive_thread, NULL);
#endif

    printf("\nChat ready! Type messages or use /send <file> or /quit\n> ");

    char input[BUF_SIZE];
    while (running) {
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "/quit") == 0) break;
        if (strncmp(input, "/send ", 6) == 0) {
            send_file(input + 6);
        } else if (strlen(input) > 0) {
            send_text(input);
        }
        if (running) printf("> ");
    }

cleanup:
    running = 0;
    if (client_sock != INVALID_SOCKET) {
        SHUTDOWN_SOCKET(client_sock, SHUT_RDWR);
        CLOSE_SOCKET(client_sock);
    }
    if (listen_sock != INVALID_SOCKET && listen_sock != client_sock)
        CLOSE_SOCKET(listen_sock);

#ifdef _WIN32
    WSACleanup();
#else
    pthread_join(recv_thread, NULL);
#endif

    printf("\nChat closed.\n");
    return 0;
}
