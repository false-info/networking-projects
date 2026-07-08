#include "chat_common.h"

#define PORT 9000

int main(int argc, char *argv[]) {
    char username[32];
    if (argc == 3) {
        strncpy(username, argv[2], 31);
        username[31] = '\0';
    } else if (argc == 2) {
        printf("Enter username: ");
        fflush(stdout);
        if (!fgets(username, sizeof(username), stdin)) return 1;
        username[strcspn(username, "\n")] = '\0';
    } else {
        fprintf(stderr, "Usage: %s <server_ip> [username]\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        perror("Bad address"); return 1;
    }
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect"); return 1;
    }

    // Send username registration
    char handshake[64];
    snprintf(handshake, sizeof(handshake), "USERNAME %s\n", username);
    send_all(sock, handshake, strlen(handshake));

    printf("Connected. Type messages or /quit.\n");
    fprintf(stderr, "%s> ", username);

    fd_set readfds;
    char buf[1024];

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        if (select(sock+1, &readfds, NULL, NULL, NULL) < 0) {
            perror("select"); break;
        }

        if (FD_ISSET(sock, &readfds)) {
            int n = recv(sock, buf, sizeof(buf)-1, 0);
            if (n <= 0) {
                printf("\nDisconnected.\n");
                break;
            }
            buf[n] = '\0';
            printf("\r\033[K%s", buf);       // erase line, print message
            fprintf(stderr, "%s> ", username); // re-print prompt
            fflush(stdout);
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (!fgets(buf, sizeof(buf), stdin)) break;
            buf[strcspn(buf, "\n")] = '\0';
            if (strcmp(buf, "/quit") == 0) break;
            strcat(buf, "\n");
            if (send_all(sock, buf, strlen(buf)) < 0) break;
            fprintf(stderr, "%s> ", username);
        }
    }
    close(sock);
    return 0;
}