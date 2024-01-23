#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <strings.h>
#include <netinet/tcp.h>

#define BUFFER_SIZE 256
#define MAX_EVENTS 2

typedef struct {
    int message_type; // 1: subscribe, 2: unsubscribe, 3: exit
    char topic[50];
    int sf;
} client_message_t;

typedef struct udp_messagge {
    char ip[16];
    short port;
    char mesaj[1560];
} udp_messagge_t;

void error(const char *msg) {
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd, portno, n;
    struct sockaddr_in serv_addr;
    struct hostent *server;

    char buffer[BUFFER_SIZE];

    if (argc < 4) {
        fprintf(stderr, "Usage: %s ID IP-address port\n", argv[0]);
        exit(1);
    }

    int id = atoi(argv[1]);

    if (server == NULL) {
        fprintf(stderr, "Eroare, nu s-a gasit hostul\n");
        exit(1);
    }

    portno = atoi(argv[3]);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("Eroare la deschiderea socket-ului");
    }

    bzero((char *)&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(argv[2]);
    serv_addr.sin_port = htons(portno);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        error("Eroare la conectare");
    }

    int flag = 1;
    setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    char id_message[BUFFER_SIZE];
    sprintf(id_message, "%s", argv[1]);
    n = send(sockfd, id_message, strlen(id_message), 0);
    if (n < 0) {
        error("Eroare la trimiterea ID-ului catre server");
    }

    // Set up epoll
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        error("Eroare la crearea epoll");
    }

    struct epoll_event event_stdin, event_sockfd, events[MAX_EVENTS];
    event_stdin.events = EPOLLIN;
    event_stdin.data.fd = STDIN_FILENO;
    event_sockfd.events = EPOLLIN;
    event_sockfd.data.fd = sockfd;

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, STDIN_FILENO, &event_stdin) == -1) {
        error("Eroare la adaugarea stdin in epoll");
    }

        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd, &event_sockfd) == -1) {
        error("Eroare la adaugarea sockfd in epoll");
    }

    fcntl(sockfd, F_SETFL, O_NONBLOCK);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

    while (1) {
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            error("Eroare la asteptarea epoll");
        }

        for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == STDIN_FILENO) {

                bzero(buffer, BUFFER_SIZE);
                fgets(buffer, BUFFER_SIZE - 1, stdin);

                if (strncmp(buffer, "exit", 4) == 0) {
                    close(sockfd);
                    return 0;
                }

                n = send(sockfd, buffer, strlen(buffer), 0);
                if (n < 0) {
                    error("Eroare la scrierea in socket");
                }
                if (strncmp(buffer, "subscribe", 9)==0) {
                    printf("Subscribed to topic.\n");
                } else if (strncmp(buffer, "unsubscribe", 11)==0){
                    printf("Unsubscribed from topic.\n");
                }
            } else if (events[i].data.fd == sockfd) {
                udp_messagge_t mesaj;
                bzero(&mesaj, sizeof(udp_messagge_t));
                int n = read(sockfd, &mesaj, sizeof(udp_messagge_t));
                if (n > 0) {
                    char nume_topic[51];
                    bzero(nume_topic, 51);
                    memcpy(nume_topic, mesaj.mesaj, 50);
                    char tip = *(mesaj.mesaj + 50);

                    switch (tip) {
                        case 0: {
                            char semn = *(mesaj.mesaj + 51);
                            int numar = 0;
                            memcpy(&numar, mesaj.mesaj + 52, 4);
                            
                            if (semn == 1) {
                                printf("%s:%hu - %s - INT - %d\n", mesaj.ip, mesaj.port, nume_topic, -ntohl(numar));
                            } else {
                                printf("%s:%hu - %s - INT - %d\n", mesaj.ip, mesaj.port, nume_topic, ntohl(numar));
                            }
                            break;
                        }

                        case 1: {
                            short numar = 0;
                            memcpy(&numar, mesaj.mesaj + 51, 2);

                            printf("%s:%hu - %s - SHORT_REAL - %.2f\n", mesaj.ip, mesaj.port, nume_topic, ntohs(numar)/100.0);
                            break;
                        }

                        case 2: {
                            char semn = *(mesaj.mesaj + 51);
                            int numar = 0;
                            memcpy(&numar, mesaj.mesaj + 52, 4);

                            char putere = *(mesaj.mesaj + 56);
                            int numar_cifre = putere;

                            float rezultat = ntohl(numar);
                            while (putere > 0) {
                                rezultat = rezultat / 10.0;
                                putere = putere -1;
                            }

                            if (semn == 1) {
                                printf("%s:%hu - %s - FLOAT - %.*f\n", mesaj.ip, mesaj.port, nume_topic, numar_cifre, -rezultat);
                            } else {
                                printf("%s:%hu - %s - FLOAT - %.*f\n", mesaj.ip, mesaj.port, nume_topic, numar_cifre, rezultat);
                            }
                            break;
                        }

                        case 3: {
                            printf("%s:%hu - %s - STRING - %s\n", mesaj.ip, mesaj.port, nume_topic, mesaj.mesaj + 51);
                            break;
                        }

                    }
                } else if (n == 0) {

                    close(sockfd);
                    close(epoll_fd);
                    return 0;
                } else {
                    error("Eroare la citirea din socket");
                }
            }
        }

        
    }

    close(sockfd);
    close(epoll_fd);
    return 0;
}

