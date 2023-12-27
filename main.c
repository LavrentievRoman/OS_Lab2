#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>

int server_socket;

void handle_connection(int client_socket) {
    // Обработка данных поступающих от клиента
    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        printf("Received data from client. Bytes: %zd\n", bytes_received);
    }
    // Закрытее клиентского сокета после обработки
    close(client_socket);
}

void sighup_handler(int signum) {
    // Обработка сигналов для SIGHUP
    printf("Received SIGHUP signal.\n");
}

int main() {
    struct sockaddr_in server_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Создание сокета сервера
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    // Установка адреса сервера
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    // Привязка серверного сокета к адресу
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding server socket");
        exit(EXIT_FAILURE);
    }

    // Начало прослушивания входящего соединения
    if (listen(server_socket, 5) == -1) {
        perror("Error listening on server socket");
        exit(EXIT_FAILURE);
    }

    // Регестрация обработчика сигнала SIGHUP
    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("Error setting up SIGHUP handler");
        exit(EXIT_FAILURE);
    }

    // Основной цикл с pselect
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        // Использование pselect с тайм-аутом
        sigset_t mask;
        struct timespec timeout;
        timeout.tv_sec = 5;
        timeout.tv_nsec = 0;

        int ready_fds = pselect(server_socket + 1, &read_fds, NULL, NULL, &timeout, &mask);
        if (ready_fds == -1) {
            if (errno == EINTR) {
                printf("pselect was interrupted by a signal.\n");
                continue;
            } else {
                perror("Error in pselect");
                break;
            }
        } 

        // Принятие нового соединения
        int client_socket = accept(server_socket, (struct sockaddr*)&server_addr, &client_addr_len);
        if (client_socket == -1) {
            perror("Error accepting connection");
            continue;
        }

        //Обработка подключения
        handle_connection(client_socket);
    }

    // Закрытее серверного сокета после завершения работы
    close(server_socket);

    return 0;
}

