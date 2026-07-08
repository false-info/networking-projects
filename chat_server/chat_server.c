#include <time.h>
#include "chat_common.h"

#define PORT 9000
#define MAX_CLIENTS 10
#define BUF_SIZE 1024

typedef struct {
    int fd;
    char username[32];
    struct sockaddr_in addr;
    time_t connected_at;
} Client;

Client clients[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].username[0] = '\0';
    }
}

int find_by_username(const char *name) {
    for (int i = 0; i < MAX_CLIENTS; i++)
        if (clients[i].fd != -1 && strcmp(clients[i].username, name) == 0)
            return i;
    return -1;
}

void handle_admin_command(int sender_idx, const char *cmd) {
    char command[32], arg[64];
    if (sscanf(cmd, "/%31s %63[^\n]", command, arg) < 1) return;

    if (strcmp(command, "info") == 0 && arg[0]) {
        int target = find_by_username(arg);
        char reply[BUF_SIZE];
        if (target == -1) {
            snprintf(reply, sizeof(reply), "No user '%s'\n", arg);
        } else {
            Client *c = &clients[target];
            snprintf(reply, sizeof(reply),
                "User: %s\nIP: %s\nPort: %d\nConnected: %lds ago\n",
                c->username, inet_ntoa(c->addr.sin_addr),
                ntohs(c->addr.sin_port), time(NULL) - c->connected_at);
        }
        send_all(clients[sender_idx].fd, reply, strlen(reply));
    }
    else if (strcmp(command, "list") == 0) {
        char reply[BUF_SIZE] = "Online users:\n";
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                char line[64];
                snprintf(line, sizeof(line), "- %s\n", clients[i].username);
                strcat(reply, line);
            }
        }
        send_all(clients[sender_idx].fd, reply, strlen(reply));
    }
    else {
        char *msg = "Unknown admin command. Try /info <user> or /list\n";
        send_all(clients[sender_idx].fd, msg, strlen(msg));
    }
}

int main() {
    init_clients();
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) { perror("socket"); exit(1); }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind"); exit(1);
    }
    if (listen(listen_fd, 5) < 0) { perror("listen"); exit(1); }
    printf("Server listening on port %d\n", PORT);

    fd_set readfds;
    int max_fd = listen_fd;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(listen_fd, &readfds);
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd != -1) {
                FD_SET(clients[i].fd, &readfds);
                if (clients[i].fd > max_fd) max_fd = clients[i].fd;
            }
        }
        if (select(max_fd+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select"); continue;
        }

        if (FD_ISSET(listen_fd, &readfds)) {
            struct sockaddr_in caddr;
            socklen_t clen = sizeof(caddr);
            int new_fd = accept(listen_fd, (struct sockaddr*)&caddr, &clen);
            if (new_fd < 0) continue;

            int slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) { slot = i; break; }
            }
            if (slot == -1) {
                close(new_fd);
                continue;
            }
            clients[slot].fd = new_fd;
            clients[slot].addr = caddr;
            clients[slot].connected_at = time(NULL);
            clients[slot].username[0] = '\0';
            printf("New connection in slot %d\n", slot);
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int fd = clients[i].fd;
            if (fd == -1 || !FD_ISSET(fd, &readfds)) continue;

            char buf[BUF_SIZE];
            int n = recv(fd, buf, BUF_SIZE-1, 0);
            if (n <= 0) {
                close(fd);
                clients[i].fd = -1;
                printf("%s disconnected.\n", clients[i].username);
                continue;
            }
            buf[n] = '\0';
            char *nl = strchr(buf, '\n'); if (nl) *nl = '\0';

            if (clients[i].username[0] == '\0') {
                if (strncmp(buf, "USERNAME ", 9) == 0) {
                    strncpy(clients[i].username, buf+9, 31);
                    clients[i].username[31] = '\0';
                    char welcome[128];
                    int len = snprintf(welcome, sizeof(welcome),
                        "Welcome %s! You are in slot %d.\n", clients[i].username, i);
                    send_all(fd, welcome, len);
                    printf("Registered: %s\n", clients[i].username);
                } else {
                    close(fd);
                    clients[i].fd = -1;
                }
                continue;
            }

            if (buf[0] == '/') {
                handle_admin_command(i, buf);
                continue;
            }

            char msg[BUF_SIZE+64];
            int len = snprintf(msg, sizeof(msg), "[%s]: %s\n",
                               clients[i].username, buf);
            for (int j = 0; j < MAX_CLIENTS; j++) {
                if (clients[j].fd != -1 && j != i)
                    send_all(clients[j].fd, msg, len);
            }
        }
    }
    return 0;
}
