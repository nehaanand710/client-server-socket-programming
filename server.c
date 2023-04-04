#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <signal.h>
#include <errno.h>

#define SERVER_PORT 8080
#define BUFFER_SIZE 16384

void sigchld_handler(int signum) {}

// /* Read characters from 'fd' until a newline is encountered. If a newline
//   character is not encountered in the first (n - 1) bytes, then the excess
//   characters are discarded. The returned string placed in 'buf' is
//   null-terminated and includes the newline character if it was read in the
//   first (n - 1) bytes. The function return value is the number of bytes
//   placed in buffer (which includes the newline character if encountered,
//   but excludes the terminating null byte). */
// ssize_t readLine(int fd, void *buffer, size_t n)
// {
//     ssize_t numRead;                    /* # of bytes fetched by last read() */
//     size_t totRead;                     /* Total bytes read so far */
//     char *buf;
//     char ch;

//     if (n <= 0 || buffer == NULL) {
//         errno = EINVAL;
//         return -1;
//     }

//     buf = buffer;                       /* No pointer arithmetic on "void *" */

//     totRead = 0;
//     for (;;) {
//         numRead = read(fd, &ch, 1);

//         if (numRead == -1) {
//             if (errno == EINTR)         /* Interrupted --> restart read() */
//                 continue;
//             else
//                 return -1;              /* Some other error */

//         } else if (numRead == 0) {      /* EOF */
//             if (totRead == 0)           /* No bytes read; return 0 */
//                 return 0;
//             else                        /* Some bytes read; add '\0' */
//                 break;

//         } else {                        /* 'numRead' must be 1 if we get here */
//             if (totRead < n - 1) {      /* Discard > (n - 1) bytes */
//                 totRead++;
//                 *buf++ = ch;
//             }

//             if (ch == '\n')
//                 break;
//         }
//     }

//     *buf = '\0';
//     return totRead;
// }

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

        char* response = process_message(buffer);

        // Send response to client
        send(new_socket, response, strlen(response), 0);
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char *message = "Hello, client!";
    int opt = 1;

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
    address.sin_port = htons(SERVER_PORT);

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
            if (count < 4 || count > 7) {
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
