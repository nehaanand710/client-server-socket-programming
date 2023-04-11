/*
* Chaitanya and Neha Team
* ASP Project 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

#define SERVER_IP "127.0.0.1" // Change this to the IP address of your server
#define MIRROR_IP "127.0.0.1" // Change this to the IP address of your server
#define SERVER_PORT 8080     // Change this to the port number your server is listening on
#define MIRROR_PORT 8082     // Change this to the port number your server is listening on
#define BUFFER_SIZE 16384 // size of the buffer to read from the socket


int receive_text(int sockfd, char *buffer) {
    int n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive text message");
        return -1;
    }
    buffer[n] = '\0';
    printf("Received text message: %s\n", buffer);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

    return 0;
}

int receive_file(int sockfd, char* buffer) {
    // char buffer[BUFFER_SIZE];
    int n;
    FILE *fp;
    long filesize;
    char filename[256];
    
    // Receive filename
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive filename");
        return -1;
    }
    buffer[n] = '\0';
    strcpy(filename, buffer);
    printf("Received filename: %s\n", filename);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

    // Receive filesize
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive filesize");
        return -1;
    }
    buffer[n] = '\0';
    filesize = atol(buffer);
    printf("Received filesize: %ld\n", filesize);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

    // Open file for writing
    fp = fopen(filename, "wb");
    if (fp == NULL) {
        perror("ERROR: Failed to open file for writing");
        return -1;
    }

    // Receive file data
    while (filesize > 0) {
        n = read(sockfd, buffer, BUFFER_SIZE);
        if (n < 0) {
            perror("ERROR: Failed to receive file data");
            fclose(fp);
            return -1;
        }
        fwrite(buffer, 1, n, fp);
        filesize -= n;
    }
    fclose(fp);
    printf("Received file: %s\n", filename);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

    return 0;
}

// int receive_end(int sockfd) {
//     char buffer[BUFFER_SIZE];
//     int n = read(sockfd, buffer, BUFFER_SIZE);
//     if (n < 0) {
//         perror("ERROR: Failed to receive end message");
//         return -1;
//     }
//     buffer[n] = '\0';
//     if (strcmp(buffer, "end") != 0) {
//         printf("ERROR: Expected end message but received %s\n", buffer);
//         return -1;
//     }
//     printf("Received end message\n");
//     return 0;
// }

int get_message(int sockfd, char* buffer) {
    // char buffer[BUFFER_SIZE];
    int n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive message type");
        return -1;
    }
    buffer[n] = '\0';
    printf("Received message type: %s\n", buffer);
    
    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }
    
    if (strcmp(buffer, "text") == 0) {
        return receive_text(sockfd, buffer);
    } else if (strcmp(buffer, "file") == 0) {
        return receive_file(sockfd, buffer);
    } else {
        printf("ERROR: Unknown message type: %s\n", buffer);
        return -1;
    }
}

int process_message (char* message) {
    printf("In process message: %s %d\n", message, strlen(message));

    if (strlen(message) == 0) {
        return 0;
    }
    return 1;
}

int handle_ack(int* sock, char* message) {
    printf("ack: %s\n", message);
    if (strcmp(message,"Error: Server Full") == 0) {
        close(*sock);
        return -1;
        //request mirror
    }
    return 0;
}

int connect_socket (int* sock, char* ip_address, int port) {
    struct sockaddr_in serv_addr;

    // Create socket
    if ((*sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    // Set up server address
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip_address, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(*sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(*sock);
        return -1;
        // reconnect = 1;
        // sleep(1); // Wait for 1 second before attempting to reconnect
        // continue; // Try to reconnect
    }

    return 0;
}

int main() {
    int sock = 0, valread;
    char buffer[BUFFER_SIZE] = {0};

    int reconnect = 0;
    char* message=malloc(1024);

    // if (connect_socket(&sock, SERVER_IP, SERVER_PORT) < 0) {
    //     if (connect_socket(&sock, MIRROR_IP, MIRROR_PORT) < 0) {
    //         printf("Failed to connect with server and mirror. Exiting...\n");
    //         exit(0);
    //     }
    // }
    int server_connected = 0;
    if (connect_socket(&sock, SERVER_IP, SERVER_PORT) == 0) {
        read(sock, buffer, BUFFER_SIZE);
        if (handle_ack(&sock, buffer) == 0) {
            printf("Connected to server at %s:%d\n", SERVER_IP, SERVER_PORT);
            server_connected = 1;
        } else {
            printf("Server redirected to mirror. Connecting to mirror...\n");
        }
    } else {
        printf("Server Down. Connecting to Mirror\n");
    }

    if (!server_connected) {
        if (connect_socket(&sock, MIRROR_IP, MIRROR_PORT) < 0) {    
            printf("Mirror Down. Exiting...\n");
            exit(1);
        }
        printf("Connected to mirror at %s:%d", MIRROR_IP, MIRROR_PORT);
    }

    // //read connection acknowledgement
    // read(sock, buffer, BUFFER_SIZE);

    // if (handle_ack(&sock, buffer) < 0) {
    //     printf("Server rejected. Redirecting to mirror...\n");
    //     if (connect_socket(&sock, MIRROR_IP, MIRROR_PORT) < 0) {
    //         printf("Failed to connect with server and mirror. Exiting...\n");
    //         exit(0);
    //     } else {
    //         if (handle_ack(&sock, buffer) < 0) {
    //             printf("Failed to connect with server and mirror. Exiting...\n");
    //             exit(0);
    //         } else {
    //             printf("Connected to ");
    //         }

    //     }
    // }

    

    while (1) {
        // scanf("%s", message);
        int is_valid;
        do {
            printf("Enter message to be sent to the server:\n");
            gets(message);
        } while ((is_valid = process_message(message)) == 0);

        // printf("Entered message is: %s", message);
        //process the message
        // int is_valid = process_message(message);

        // strcat(message, "\n");

        if (is_valid) {
            send(sock, message, strlen(message), 0);
            if (strcmp(message, "quit") == 0) {
                // Close connection to client and exit child process
                close(sock);
                break;
            }
        } else {
            printf("Message is not correct. try again\n");
            continue;
        }

        printf("Message sent to server\n");

        // Receive response from server
        // valread = read(sock, buffer, BUFFER_SIZE);
        get_message(sock, buffer);

        printf("Response from the server: %s\n", buffer);
        // close(sock); // Close connection to server
    }

    return 0;
}
