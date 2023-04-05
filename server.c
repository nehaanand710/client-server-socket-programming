#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>

#define SERVER_PORT 8080
#define MIRROR_PORT 8082
#define BUFFER_SIZE 16384
#define HEARTBEAT_INTERVAL_SECONDS 10

int is_mirror = 0;
int* is_other_down;

void* create_shared_memory(size_t size) {
  // Our memory buffer will be readable and writable:
  int protection = PROT_READ | PROT_WRITE;

  // The buffer will be shared (meaning other processes can access it), but
  // anonymous (meaning third-party processes cannot obtain an address for it),
  // so only this process and its children will be able to use it:
  int visibility = MAP_SHARED | MAP_ANONYMOUS;

  // The remaining parameters to `mmap()` are not important for this use case,
  // but the manpage for `mmap` explains their purpose.
  return mmap(NULL, size, protection, visibility, -1, 0);
}

void set_other_status (int is_down) {
    int* val = &is_down;
    memcpy(is_other_down, val, sizeof(int));
}

void *heartbeat(void *arg) {
    int connfd = *(int *)arg;
    while(1) {
        // printf("sending heartbeat\n");
        sleep(5); // wait for 5 seconds
        if(send(connfd, "heartbeat", 9, 0) == -1) {
            perror("send");
            exit(EXIT_FAILURE);
        }
    }
}

void *server_thread(void *arg) {
    int server_fd, new_socket, valread;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( SERVER_PORT + 1 );

    // Binding socket to address
    if (bind(server_fd, (struct sockaddr *)&address,
                                sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    while (1) {
        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        // Accept a new connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address,
                        (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        set_other_status(0);
        printf("Server is now UP\n");

        // Start a new thread to handle heartbeat messages
        // pthread_t tid;
        // if(pthread_create(&tid, NULL, heartbeat, &new_socket) != 0) {
        //     perror("pthread_create");
        //     exit(EXIT_FAILURE);
        // }

        // Receive data from client
        char buffer[1024] = {0};
        while ((valread = read(new_socket , buffer, 1024)) > 0) {
            // printf("%s\n",buffer);
        }
        // if (valread == 0) {
        //     printf("Client disconnected\n");
        // } else {
        //     perror("read");
        // }
        set_other_status(1);
        printf("Server is now DOWN\n");
        // pthread_cancel(tid);
    }
    return NULL;
}

void *mirror_thread(void *arg) {
    while(1) {    
        struct sockaddr_in serv_addr;
        int sockfd = 0, valread;
        char buffer[1024] = {0};

        // Create socket
        if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            perror("socket creation failed");
            exit(EXIT_FAILURE);
        }

        // Set server details
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(SERVER_PORT + 1);

        // Convert IPv4 and IPv6 addresses from text to binary form
        if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0) {
            perror("Invalid address");
            exit(EXIT_FAILURE);
        }

        // Connect to server
        while (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            // perror("connection failed");
            sleep(5);
        }

        set_other_status(0);
        printf("Server is now UP\n");
        
        // Start a new thread to handle heartbeat messages
        // pthread_t tid;
        // if(pthread_create(&tid, NULL, heartbeat, &sockfd) != 0) {
        //     perror("pthread_create");
        //     exit(EXIT_FAILURE);
        // }

        while ((valread = read(sockfd , buffer, 1024)) > 0) {
            // printf("%s\n",buffer);
        }
        // if (valread == 0) {
        //     printf("Client disconnected\n");
        // } else {
        //     perror("read");
        // }

        set_other_status(1);
        printf("Server is now DOWN\n");
        // pthread_cancel(tid);
    }

    return NULL;
}

char* process_message (char* message) {
    // quit request from client
    if (strcmp(message, "quit") == 0) {
        printf("client exit request. Exiting from the process handler\n");
        exit(0);
    }
    char* response = "Processed message";
    return response;
}

void process_client(int new_socket, char* message) {

    char buffer[BUFFER_SIZE] = {0};
    // while(1) {
    //     int valread = read(new_socket, buffer, BUFFER_SIZE);
    //     printf("read: %s, %d\n", buffer, valread);
    // }
    
    while (1) {
        // clear buffer before each read
        memset(buffer, 0, BUFFER_SIZE);

        // Receive message from client
        int valread = read(new_socket, buffer, BUFFER_SIZE);


        // get_input(new_socket, buffer);
        // printf("valread: %d\n", valread);
        
        // client disconnected
        if (valread <= 0) {
            printf("Client Disconnected or some error occured. Exiting the client handler\n");
            exit(0);
        }

        printf("Message received from client: %s\n", buffer);

        fflush(stdout);

        char* response = process_message(buffer);

        // Send response to client
        send(new_socket, response, strlen(response), 0);
    }
}

int main(int argc, char* argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    char *message = "Hello, client!";
    int opt = 1;

    if (argc > 1) {
        is_mirror = 1;
    } else {
        is_mirror = 0;
    }

    int port = is_mirror ? MIRROR_PORT : SERVER_PORT;

    is_other_down = (int*) create_shared_memory(sizeof(int));

    pthread_t heartbeat_tid;
    pthread_create(&heartbeat_tid, NULL, is_mirror ? mirror_thread : server_thread , NULL);
    
    // Create server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow address reuse
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    signal(SIGCHLD, sigchld_handler);

    // Set server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    // Bind socket to address and listen for connections
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept and process incoming connections indefinitely
    int count = 0;
    while (1) {
        printf("Waiting for client connections...\n");

        // Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue; // Try again
        }

        printf("New client connection accepted. Forking to handle\n");
        
        // Fork new process to handle client request
        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(server_fd); // Close listening socket in child process
            if (*is_other_down || count < 4 || count > 7) {
                char* accept_message = "Success: Accepted";
                send(new_socket, accept_message, strlen(accept_message), 0);
                process_client(new_socket, message);
            } else {
                char* reject_message = "Error: Server Full";
                send(new_socket, reject_message, strlen(reject_message), 0);
                exit(0);
            }

        } else if (pid < 0) { // Error forking process
            perror("Fork failed");
        } else { // Parent process
            close(new_socket); // Close client socket in parent process
            count == 8 ? count-- : count++;
        }
    }

    return 0;
}
