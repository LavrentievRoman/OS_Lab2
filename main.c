#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <pthread.h>

#define MAX_CONNECTIONS 10

int server_socket;
int client_sockets[MAX_CONNECTIONS];
pthread_mutex_t mutex;

void handle_connection(int client_socket) {
    // Обработка данных от клиента
    // В данном примере просто выводим количество полученных данных
    char buffer[1024];
    ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
    if (bytes_received > 0) {
        pthread_mutex_lock(&mutex);
        printf("Received data from client. Bytes: %zd\n", bytes_received);
        pthread_mutex_unlock(&mutex);
    }
}

void* connection_handler(void* args) {
    int client_socket = *((int*)args);
    free(args);

    // Оставляем одно соединение, остальные закрываем
    pthread_mutex_lock(&mutex);
    close(server_socket);
    pthread_mutex_unlock(&mutex);

    // Обработка данных от клиента
    handle_connection(client_socket);

    // Закрываем соединение после обработки
    close(client_socket);
    return NULL;
}

void sighup_handler(int signum) {
    // Обработка сигнала SIGHUP
    pthread_mutex_lock(&mutex);
    printf("Received SIGHUP signal.\n");
    pthread_mutex_unlock(&mutex);
}

int main() {
    struct sockaddr_in server_addr;
    socklen_t client_addr_len = sizeof(struct sockaddr_in);

    // Инициализация мьютекса
    pthread_mutex_init(&mutex, NULL);

    // Создание серверного сокета
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Error creating server socket");
        exit(EXIT_FAILURE);
    }

    // Настройка серверного адреса
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8888);

    // Привязка серверного сокета к адресу
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error binding server socket");
        exit(EXIT_FAILURE);
    }

    // Переход в режим прослушивания
    if (listen(server_socket, 5) == -1) {
        perror("Error listening on server socket");
        exit(EXIT_FAILURE);
    }

    // Регистрация обработчика сигнала SIGHUP
    struct sigaction sa;
    sa.sa_handler = sighup_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGHUP, &sa, NULL) == -1) {
        perror("Error setting up SIGHUP handler");
        exit(EXIT_FAILURE);
    }

    // Основной цикл с использованием pselect
    while (1) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket, &read_fds);

        // Вызов pselect с таймером
        struct timespec timeout;
        timeout.tv_sec = 5;
        timeout.tv_nsec = 0;

        int ready_fds = pselect(server_socket + 1, &read_fds, NULL, NULL, &timeout, NULL);
        if (ready_fds == -1) {
            perror("Error in pselect");
            exit(EXIT_FAILURE);
        } else if (ready_fds == 0) {
            // Таймаут, можно добавить дополнительные действия
            continue;
        }

        // Принятие нового соединения
        if (FD_ISSET(server_socket, &read_fds)) {
            int client_socket = accept(server_socket, (struct sockaddr*)&server_addr, &client_addr_len);
            if (client_socket == -1) {
                perror("Error accepting connection");
                continue;
            }

            // Создание потока для обработки соединения
            pthread_t thread;
            int* client_socket_ptr = (int*)malloc(sizeof(int));
            *client_socket_ptr = client_socket;

            if (pthread_create(&thread, NULL, connection_handler, (void*)client_socket_ptr) != 0) {
                perror("Error creating thread");
                free(client_socket_ptr);
                close(client_socket);
            }
        }
    }

    // Освобождение ресурсов и завершение
    pthread_mutex_destroy(&mutex);
    close(server_socket);

    return 0;
}
