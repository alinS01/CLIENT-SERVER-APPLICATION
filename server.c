#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <strings.h>
#include <netinet/tcp.h>

#define BUFFER_SIZE 256
#define MAX_TOPIC_LEN 51
#define MAX_ID_LEN 12

typedef struct udp_messagge {
    char ip[16];
    short port;
    char mesaj[1560];
} udp_messagge_t;

typedef struct subscription {
    char client_id[MAX_ID_LEN];
    int sf;
    int online;
    int fd;
    char topicuri_sf[20][MAX_TOPIC_LEN];
    int numar_sf;
    udp_messagge_t ratari_sf[10];
    int numar_ratari;
} subscription_t;

typedef struct topic_subscribers {
    char topic[MAX_TOPIC_LEN];
    subscription_t subscriptions[20];
    int number_of_subscribers;
} topic_subscribers_t;


typedef struct input_data {
    int fd;                   
    char buffer[BUFFER_SIZE];        
} input_data_t;



void error(const char *msg) {
    perror(msg);
    exit(1);
}


// Funcție pentru a verifica dacă inputul este comanda "exit"
int is_exit_command(input_data_t *input_data) {
    return strncmp(input_data->buffer, "exit", 4) == 0;
}


int global_client_id = 0;


void print_client_message(const char *message, int client_id, struct sockaddr_in client_addr) {
    printf("%s %d connected from %s:%d.\n", message, client_id, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
}


void handle_new_client(int sockfd_tcp, int epoll_fd, struct epoll_event *events, int *events_number, subscription_t *subscribers, int *clients_number);
void handle_client(int sockfd_tcp, int epoll_fd, struct epoll_event *events, int *events_number, subscription_t *subscribers, int *clients_number, topic_subscribers_t *topics, int *topic_number);
void handle_udp(int sockfd_udp, subscription_t *subscribers, int client_number, topic_subscribers_t *topics, int topic_number);


int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd_tcp, sockfd_udp, portno;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    char buffer[256];
    int n;
    
    topic_subscribers_t topics[20];
    subscription_t subscribers[20];

    if (argc < 2) {
        fprintf(stderr, "Eroare, nu s-a specificat portul\n");
        exit(1);
    }
    portno = atoi(argv[1]);

    //initializam socket-ul TCP:
    sockfd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd_tcp < 0) {
        error("Eroare la deschiderea socket-ului TCP");
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);//htons schimba ordinea de la host la netowrk 

    if (bind(sockfd_tcp, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("Eroare la bind la socket-ul TCP");
    }
    listen(sockfd_tcp, 30);

    //initializam socket-ul UDP : 
    sockfd_udp = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd_udp < 0) {
        error("Eroare la deschiderea socket-ului UDP");
    }

    if (bind(sockfd_udp, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("Eroare la bind la socket-ul UDP");
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        error("Eroare la crearea epoll");
    }

    struct epoll_event events[10];

    events[0].events = EPOLLIN;
    events[0].data.fd = sockfd_tcp;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd_tcp, &events[0]) == -1) {
        error("Eroare la adaugarea socket-ului TCP in epoll");
    }

    events[1].events = EPOLLIN;
    events[1].data.fd = sockfd_udp;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sockfd_udp, &events[1]) == -1) {
        error("Eroare la adaugarea socket-ului UDP in epoll");
    }

    // struct epoll_event event_stdin;
    events[2].events = EPOLLIN;
    events[2].data.fd = STDIN_FILENO; // stdin
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, 0, &events[2]) == -1) {
        error("Eroare la adaugarea stdin in epoll");
    }
     
    input_data_t input_data;
    input_data.fd = 0;
    int clients_number = 0;
    int topic_number = 0;

    int events_number = 3;

    while (1) {
        int ready = epoll_wait(epoll_fd, events, events_number, -1);
        for (int i = 0; i < ready; i++) {
        if (events[i].data.fd == sockfd_tcp) {
            handle_new_client(sockfd_tcp, epoll_fd, events, &events_number, subscribers, &clients_number);
        } else if (events[i].data.fd == sockfd_udp) {
            handle_udp(sockfd_udp, subscribers, clients_number, topics, topic_number);
        } else if (events[i].data.fd == input_data.fd) {
            bzero(input_data.buffer, 256);
            fgets(input_data.buffer, 255, stdin);

            if (is_exit_command(&input_data)) {
                for (int j = 1; j < events_number; j++) {
                    close(events[j].data.fd);
                }
                close(epoll_fd);
                return 0;
            }
            } else {
                handle_client(events[i].data.fd, epoll_fd, events, &events_number, subscribers, &clients_number, topics, &topic_number);
            }
        }
    }
}


void handle_new_client(int sockfd_tcp, int epoll_fd, struct epoll_event *events, int *events_number, subscription_t *subscribers, int *clients_number) {
    int socket_cli;
    struct sockaddr_in cli_addr;
    int size_client = sizeof(cli_addr);

    bzero(&cli_addr, size_client);
    socket_cli = accept(sockfd_tcp, (struct sockaddr*)&cli_addr, &size_client);

    int flag = 1;
    setsockopt(socket_cli, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

    if (socket_cli < 0) {
        error("Eroare la conectare");
    }

    events[*events_number].data.fd = socket_cli;
    events[*events_number].events = EPOLLIN;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, socket_cli, &events[*events_number]) == -1) {
        error("Eroare la adaugare client in epoll");
    }

    *events_number = *events_number + 1;
    char id_buffer[BUFFER_SIZE];
    bzero(id_buffer, BUFFER_SIZE);
    int n = recv(socket_cli, id_buffer, BUFFER_SIZE, 0);
    if (n<0) {
        error("Eroare la citirea Id-ului");
    }

    for (int i = 0; i < *clients_number; i++) {
        if (strncmp(subscribers[i].client_id, id_buffer, strlen(id_buffer)) == 0) {
            if (subscribers[i].online == 1) {
                printf("Client %s already connected.\n", id_buffer);
                close(socket_cli);
                *events_number = *events_number - 1;
                return;
            } else {
                subscribers[i].online = 1;
                subscribers[i].fd = socket_cli;
                printf("New client %s connected from %s:%d.\n", id_buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                for (int j = 0; j < subscribers[i].numar_ratari; j++) {
                    send(socket_cli, &subscribers[i].ratari_sf, sizeof(udp_messagge_t), 0);
                    subscribers[i].numar_ratari = 0;
                }
                return;
            }
        }
    }

    subscribers[*clients_number].fd = socket_cli;
    subscribers[*clients_number].online = 1;
    subscribers[*clients_number].numar_sf = 0;
    subscribers[*clients_number].numar_ratari = 0;
    memcpy(&subscribers[*clients_number].client_id, id_buffer, 11);
    *clients_number = *clients_number + 1;

    printf("New client %s connected from %s:%d.\n", id_buffer, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
}

void handle_client(int sockfd_tcp, int epoll_fd, struct epoll_event *events, int *events_number, subscription_t *subscribers, int *clients_number, topic_subscribers_t *topics, int *topic_number) {
    char buffer_comanda[BUFFER_SIZE];
    int n = read(sockfd_tcp, buffer_comanda, BUFFER_SIZE);
    if (n < 0) {
        error("Nu am putut citi mesajul");
    }

    if (n == 0) {
        for (int i = 0; i < *events_number; i++) {
            if (events[3 + i].data.fd == sockfd_tcp) {
                char id[BUFFER_SIZE];
                bzero(id, BUFFER_SIZE);
                for (int j = 0; j < *clients_number; j++) {
                    if (subscribers[j].fd == sockfd_tcp) {
                        subscribers[j].online = 0;
                        strcpy(id, subscribers[j].client_id);
                        break;
                    }
                }

                close(events[3+i].data.fd);
                for (int j = 3 + i; j < *events_number - 1; j++) {
                    events[j] = events[j + 1];
                }
                *events_number = *events_number - 1;

                printf("Client %s disconnected.\n", id);
                return;
            }
        }
    }

    char *token = strtok(buffer_comanda, " ");
    if (strcmp(token, "subscribe") == 0) {
        token = strtok(NULL, " ");
        for (int i = 0; i < *topic_number; i++) {
            if (strcmp(topics[i].topic, token) == 0) {
                for (int j = 0; j < *clients_number; j++){
                    if (subscribers[j].fd == sockfd_tcp) {
                        token = strtok(NULL, " ");
                        int sf = atoi(token);

                        if (sf) {
                            strcpy(subscribers[j].topicuri_sf[subscribers[j].numar_sf], topics[i].topic);
                            subscribers[j].numar_sf = subscribers[j].numar_sf + 1;
                        }
                        
                        topics[i].subscriptions[topics[i].number_of_subscribers] = subscribers[j];
                        topics[i].number_of_subscribers++;

                        return;
                    }
                }
            }
        }

        strcpy(topics[*topic_number].topic, token);
        topics[*topic_number].number_of_subscribers = 1;
        for (int i = 0; i < *clients_number; i++) {
            if (subscribers[i].fd == sockfd_tcp) {
                topics[*topic_number].subscriptions[0] = subscribers[i];
                *topic_number = *topic_number + 1;
                return;
            }
        }
    }
}

void handle_udp(int sockfd_udp, subscription_t *subscribers, int client_number, topic_subscribers_t *topics, int topic_number) {
    char buffer_mesaj[1560];

    struct sockaddr_in cli_udp;
    bzero(&cli_udp, sizeof(cli_udp));

    int size_cli = sizeof(cli_udp);

    int n = recvfrom(sockfd_udp, buffer_mesaj, 1560, MSG_WAITALL, (struct sockaddr*)&cli_udp, &size_cli);

    char topic_buffer[MAX_TOPIC_LEN];
    memcpy(topic_buffer, buffer_mesaj, 50);

    for (int i = 0; i < topic_number; i++) {
        if (strcmp(topics[i].topic, topic_buffer) == 0) {
            for (int j = 0; j < topics[i].number_of_subscribers; j++) {
                udp_messagge_t mesaj;
                bzero(&mesaj, sizeof(mesaj));
                memcpy(&mesaj.ip, inet_ntoa(cli_udp.sin_addr), 16);
                mesaj.port = ntohs(cli_udp.sin_port);
                memcpy(&mesaj.mesaj, buffer_mesaj, n);
                if(topics[i].subscriptions[j].online) {
                    send(topics[i].subscriptions[j].fd, &mesaj, sizeof(udp_messagge_t), 0);
                } else {
                    for (int k = 0; k < topics[i].subscriptions[j].numar_sf; k++) {
                        if (strcmp(topics[i].topic, topics[i].subscriptions[j].topicuri_sf[k]) == 0) {
                            int pozitie = topics[i].subscriptions[j].numar_ratari;
                            topics[i].subscriptions[j].ratari_sf[pozitie] = mesaj;
                            topics[i].subscriptions[j].numar_ratari++;
                        }
                    }
                }
            }
        }
    }
}

           