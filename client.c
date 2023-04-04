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
#define SERVER_PORT 8080     // Change this to the port number your server is listening on
#define BUFFER_SIZE 16384 // size of the buffer to read from the socket

int process_message (char* message) {
    printf("In process message: %s %d\n", message, strlen(message));

    if (strlen(message) == 0) {
        return 0;
    }
    return 1;
}

void handle_ack(int sock, char* message) {
    printf("ack: %s\n", message);
    if (strcmp(message,"Error: Server Full") == 0) {
        close(sock);
        exit(0);
        //request mirror
    }
}

int main() {
    int sock = 0, valread;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};

    int reconnect = 0;
    char* message=malloc(1024);
    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }

    // Set up server address
    memset(&serv_addr, '0', sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return -1;
    }

    // Connect to server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        close(sock);
        exit(1);
        // reconnect = 1;
        // sleep(1); // Wait for 1 second before attempting to reconnect
        // continue; // Try to reconnect
    }

    //read connection acknowledgement
    read(sock, buffer, BUFFER_SIZE);

    handle_ack(sock, buffer);

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
        valread = read(sock, buffer, BUFFER_SIZE);

        printf("Response from the server: %s\n", buffer);
        // close(sock); // Close connection to server
    }

    return 0;
}
