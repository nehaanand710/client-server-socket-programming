/*
* Group 7 
* 
* ASP Project :COMP 8567 :Section 1
* 110091353 : 	Chaitanya Jariwala
* 110090513 :   Neha Anand
*/


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
#include <sys/time.h>

#define SERVER_PORT 8080
#define MIRROR_PORT 8082
#define BUFFER_SIZE 16384
#define HEARTBEAT_INTERVAL_SECONDS 10
#define IP "127.0.0.1"

int is_mirror = 0;
int* is_other_down;

/* Generates a timestamp which is used to have different temprorary name for the tar file*/
long long get_time() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000LL + tv.tv_usec / 1000;
}

// Tokenizes the given string `str` by space and stores the tokens in `tokens`.
// Returns the number of tokens found.
int tokenize_string(const char *str, char **tokens) {
    char *token;
    int token_count = 0;

    // Make a copy of the input string, since strtok modifies the original string.
    char *str_copy = strdup(str);

    // Tokenize the string by space.
    token = strtok(str_copy, " ");
    while (token != NULL) {
        // Copy the token into the output array.
        tokens[token_count] = strdup(token);
        token_count++;

        // Get the next token.
        token = strtok(NULL, " ");
    }

    // Free the temporary string copy.
    // free(str_copy);

    // Return the number of tokens found.
    return token_count;
}

char* execute_command(char* cmd) {
    // return "sample_text";
    char buffer[BUFFER_SIZE-1];
    memset(buffer, '\0', sizeof(buffer));
    FILE* fp;
    char* result;

    fp = popen(cmd, "r");
    if (fp == NULL) {
        printf("Failed to run command\n");
        return "";
    }

    // Allocate memory for result string
    result = (char*) malloc(BUFFER_SIZE);
    memset(result, '\0', sizeof(result));

    while (fgets(buffer, sizeof(buffer), fp) != NULL) {
        strcat(result, buffer);
        memset(buffer, '\0', sizeof(buffer));
    }
    strcat(result, "\0");

    pclose(fp);
    return result;
}

int send_text(int sockfd, char *text) {
    char buffer[BUFFER_SIZE];

   
    //send response type
    int n = write(sockfd, "text", 4);
    if (n < 0) {
        perror("ERROR: Failed to send message type");
        return -1;
    }

    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }
    
    //send text data
    n = write(sockfd, text, strlen(text));
    if (n < 0) {
        perror("ERROR: Failed to send text message");
        return -1;
    }

    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }
    
    return 0;
}

int send_file(int sockfd, char *filename) {
    char buffer[BUFFER_SIZE];
    int n;
    FILE *fp;
    long filesize;
    
    // Open file for reading
    fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("ERROR: Failed to open file for reading");
        return -1;
    }

    // Get file size
    fseek(fp, 0L, SEEK_END);
    filesize = ftell(fp);
    rewind(fp);

    //send response type
    n = write(sockfd, "file", 4);
    if (n < 0) {
        perror("ERROR: Failed to send message type");
        return -1;
    }

    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }

    // Send filename
    n = write(sockfd, "temp.tar.gz", strlen("temp.tar.gz"));
    sprintf(buffer, "%ld\n", filesize);
    if (n < 0) {
        perror("ERROR: Failed to send filename");
        fclose(fp);
        return -1;
    }

    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }

    // Send filesize
    sprintf(buffer, "%ld\n", filesize);
    n = write(sockfd, buffer, strlen(buffer));
    if (n < 0) {
        perror("ERROR: Failed to send filesize");
        fclose(fp);
        return -1;
    }

    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }

    // Send file data
    while (filesize > 0) {
        n = fread(buffer, 1, BUFFER_SIZE, fp);
        if (n < 0) {
            perror("ERROR: Failed to read file data");
            fclose(fp);
            return -1;
        }
        n = write(sockfd, buffer, n);
        if (n < 0) {
            perror("ERROR: Failed to send file data");
            fclose(fp);
            return -1;
        }
        filesize -= n;
    }
    fclose(fp);
    //receive ack
    n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive ack for filesize");
        return -1;
    }
    return 0;
}

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
        printf("[*] SERVER: Mirror is now UP\n\n");

        // Receive data from client
        char buffer[1024] = {0};
        while ((valread = read(new_socket , buffer, 1024)) > 0) {
            // printf("%s\n",buffer);
        }

        set_other_status(1);
        printf("[*] SERVER: Mirror is now DOWN\n\n");
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
        if(inet_pton(AF_INET, IP, &serv_addr.sin_addr)<=0) {
            perror("Invalid address");
            exit(EXIT_FAILURE);
        }

        // Connect to server
        while (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            // perror("connection failed");
            sleep(5);
        }

        set_other_status(0);
        printf("[*] SERVER: Server is now UP\n\n");

        while ((valread = read(sockfd , buffer, 1024)) > 0) {
            // printf("%s\n",buffer);
        }

        set_other_status(1);
        printf("[*] SERVER: Server is now DOWN\n\n");
        // pthread_cancel(tid);
    }

    return NULL;
}

//cmd is the find criteria. The files that are returned from this command will be included in the tar
void find_and_send_tar (int new_socket, char* cmd, char* filename) {
    char* result = execute_command(cmd);

    if (strlen(result) == 0) {
        send_text(new_socket, "No file found");
    } else {
        //create tar
        sprintf(cmd, "%s | tar -czvf %s --null -T - > /dev/null 2>&1", cmd, filename);
        system(cmd);

        //send file
        send_file(new_socket, filename);

        // remove the file from server
        sprintf(cmd, "rm -f %s", filename);
        system(cmd);
    }
    // free(result);
}

void process_message (int new_socket, char* message) {

    if (strcmp(message, "quit") == 0) {
        printf("[*] SERVER: Client closed connection\n\n");
        exit(0);
    }

    char* cmd = malloc(200);
    char** tokens = malloc(strlen(message)*sizeof(char));
    int n = tokenize_string(message, tokens);
    
    if (strncmp(message, "findfile", 8) == 0) {
        sprintf(cmd, "find ~ -name %s -printf \"%%f, size: %%s bytes, created on: %%t\n\" -quit", tokens[1]);
        // printf("cmd value: %s\n", cmd);
        char* result = execute_command(cmd);
        if (strlen(result) == 0) {
            result = "File Not Found";
        }

        send_text(new_socket, result);
    }
    //generate unique filename
    char* filename = malloc(30);
    sprintf(filename, "temp_%lld.tar.gz", get_time());

    if (strncmp(message, "sgetfiles", 9) == 0) {
        //create find condition
        sprintf(cmd, "find ~ -type f -size +\"%s\"c -size -\"%s\"c -print0", tokens[1], tokens[2]);
        find_and_send_tar(new_socket, cmd, filename);
    }

    else if (strncmp(message, "dgetfiles", 9) == 0) {
        //create find condition
        sprintf(cmd, "find ~ -type f -newermt \"%s 00:00:00\" ! -newermt \"%s 23:59:59\" -print0", tokens[1], tokens[2]);
        find_and_send_tar(new_socket, cmd, filename);
    }

    else if (strncmp(message, "getfiles", 8) == 0) {
        //create find criteria
        sprintf(cmd, "find ~ \\( -name %s", tokens[1]);
        
        int i;
        for (i=2;i<n;i++) {
            sprintf(cmd, "%s -o -name %s", cmd, tokens[i]);
        }

        sprintf(cmd, "%s \\) -print0", cmd);

        find_and_send_tar(new_socket, cmd, filename);
    }

    else if (strncmp(message, "gettargz", 8) == 0) {
        //create find criteria
        sprintf(cmd, "find ~ -type f \\( -iname \"*.%s\"", tokens[1]);
        
        int i;
        for (i=2;i<n;i++) {
            sprintf(cmd, "%s -o -iname \"*.%s\"", cmd, tokens[i]);
        }

        sprintf(cmd, "%s \\) -print0", cmd);

        find_and_send_tar(new_socket, cmd, filename);
    }

    // free(cmd);
    // free(filename);
    // free(tokens);
    printf("[*] SERVER: Response sent to client\n\n");
    fflush(stdout);
}

void process_client(int new_socket) {
    
    char buffer[BUFFER_SIZE] = {0};
    
    while (1) {
        // clear buffer before each read
        memset(buffer, 0, BUFFER_SIZE);

        // Receive message from client
        int valread = read(new_socket, buffer, BUFFER_SIZE);

        // client disconnected
        if (valread <= 0) {
            printf("[*] SERVER: Client Disconnected Abruptly.\n\n");
            exit(0);
        }

        printf("[*] SERVER: Message received from client: %s\n\n", buffer);

        fflush(stdout);

        process_message(new_socket, buffer);
    }
}

int main(int argc, char* argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    int opt = 1;
    
    int port = SERVER_PORT;

    //shared variable being used to switch between server and mirror
    is_other_down = (int*) create_shared_memory(sizeof(int));

    pthread_t heartbeat_tid;
    pthread_create(&heartbeat_tid, NULL, is_mirror ? mirror_thread : server_thread , NULL);
    
    // Create server socket
    //1. socket(int domain, int type,int protocol)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed \n");
        exit(EXIT_FAILURE);
    }

    // Set socket options to allow address reuse
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Set server address
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    //3. Bind socket to address and listen for connections
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    //4. Listen for connections to socket
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    // Accept and process incoming connections indefinitely
    int count = 0;
    while (1) {

        //5.  Accept incoming connection
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue; // Try again
        }

        // Fork new process to handle client request
        pid_t pid = fork();
        if (pid == 0) { // Child process
            close(server_fd); // Close listening socket in child process
            if (is_mirror) {
                printf("[*] MIRROR: New client connection\n\n");
                fflush(stdout);
                char* accept_message = "Success: Accepted";
                send(new_socket, accept_message, strlen(accept_message), 0);
                process_client(new_socket);
            } else {
                if (*is_other_down || count < 4 || count > 7) {
                    printf("[*] SERVER: New client connection\n\n");
                    fflush(stdout);
                    char* accept_message = "Success: Accepted";
                    send(new_socket, accept_message, strlen(accept_message), 0);
                    process_client(new_socket);
                } else {
                    char* reject_message = "Error: Server Full";
                    send(new_socket, reject_message, strlen(reject_message), 0);
                    exit(0);
                }
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
