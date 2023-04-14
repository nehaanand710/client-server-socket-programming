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
#include <ctype.h>
#include <stdbool.h>
#include <libtar.h>


#define SERVER_IP "127.0.0.1" // Change this to the IP address of your server
#define MIRROR_IP "127.0.0.1" // Change this to the IP address of your server
#define SERVER_PORT 8080     // Change this to the port number your server is listening on
#define MIRROR_PORT 8082     // Change this to the port number your server is listening on
#define BUFFER_SIZE 16384 // size of the buffer to read from the socket
#define MAX_WORDS_IN_COMMAND 8

struct result_process_message {
    int isvalidmsg ;
    int containsuFlag;
}result;

int is_numeric(const char *str) {
    while (*str) {
        if (!isdigit(*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int receive_text(int sockfd, char *buffer) {
    memset(buffer, '\0', BUFFER_SIZE);
    int n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive text message");
        return -1;
    }
    buffer[n] = '\0';
    printf("\nResponse from the server:\n %s\n\n", buffer);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

    return 0;
}

int receive_file(int sockfd, char* buffer, int is_user_flag) {
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
    // printf("Received filename: %s\n", filename);

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
    printf("\nReceived filesize: %ld\n", filesize);

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
    printf("Received file: %s\n\n", filename);

    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }

     if (is_user_flag) {
        printf("unzip \n");
        TAR *tar;
        int ret;

        tar = tar_open("temp.tar.gz", NULL, O_RDONLY, 0, TAR_GNU);
        if (tar == NULL) {
            fprintf(stderr, "Failed to open tar temp.tar.gz\n");
            return 1;
        }

        ret = tar_extract_all(tar, ".");
        if (ret != 0) {
            fprintf(stderr, "Failed to extract tar temp.tar.gz\n");
            tar_close(tar);
            return 1;
        }

        tar_close(tar);
        printf("Tar archive extracted successfully\n");
        return 0;
        }
     
    return 0;
}

int get_message(int sockfd, char* buffer,int  is_user_flag) {
    // char buffer[BUFFER_SIZE];
    //receive response type
    int n = read(sockfd, buffer, BUFFER_SIZE);
    if (n < 0) {
        perror("ERROR: Failed to receive message type");
        return -1;
    }
    buffer[n] = '\0';
    // printf("Received message type: %s\n", buffer);
    
    //send ack
    n = write(sockfd, "ack", 3);
    if (n < 0) {
        perror("ERROR: Failed to ack");
        return -1;
    }
    
    if (strcmp(buffer, "text") == 0) {
        return receive_text(sockfd, buffer);
    } else if (strcmp(buffer, "file") == 0) {
        return receive_file(sockfd, buffer, is_user_flag);
    } else {
        printf("ERROR: Unknown message type: %s\n", buffer);
        return -1;
    }
}

int incoming_msg_tokens(const char *str, char **tokens) {
    char *token;
    int token_count = 0;

    // Make a copy of the input string, since strtok modifies the original string.
    char *str_copy = strdup(str);

    // Tokenize the string by space since commands are expected to be separated by a space
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

bool isLeapYear(int year) {
    return ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0);
}

bool isValidDate(int year, int month, int day) {
    if (year < 1 || month < 1 || month > 12 || day < 1 || day > 31) {
        return false;
    }

    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && isLeapYear(year)) {
        daysInMonth[1] = 29;
    }

    return day <= daysInMonth[month - 1];
}


bool isValidDateFormat(char *date) {
 int year, month, day;
         if (sscanf(date, "%d-%d-%d", &year, &month, &day) == 3) {
            
            //printf("date given is %s\n",tokens[1]);
            //        fflush(stdout);
            if (!isValidDate(year, month, day)) {
                 printf("Invalid date %s.\n",date);
                 fflush(stdout);
                 return 0;
            }
            
        } else {
            printf("Invalid input format of date %s.Kindly enter in the form of yyyy-mm-dd\n",date);
            return 0;
        }
    return 1;
}

bool isdate1lessThanDate2(char *date1,char * date2) {
    int year1,year2,month1,month2,day1,day2;
    sscanf(date1, "%d-%d-%d", &year1, &month1, &day1);
    sscanf(date2, "%d-%d-%d", &year2, &month2, &day2);

    if(year1 <= year2) {
        return true;
    }
    else if (year1> year2) {
        return false;
    }
    else  {
        if (month1<= month2) {
            return true;
        }
        else if (month1>month2) {
            return false;
        }
        else {
            if( day1<= day2) {
                return true;
            }
            else {
                return false;
            }
        }
    }
}

struct result_process_message process_message (char* message) {
    struct result_process_message result;
    // return 1;
    if (strlen(message) == 0) {
        result.isvalidmsg =0;
        result.containsuFlag=0;
        return result;
    }

       //checking if the user has entered quit
    if (strcmp(message, "quit") == 0) {
                // Close connection to client and exit child process
                //close(sock);
                //break;
                result.isvalidmsg =1;
                result.containsuFlag=0;
                return result;
    }
    char* cmd = malloc(200);
    char** tokens = malloc(strlen(message)*sizeof(char));
    int num_commands = incoming_msg_tokens(message, tokens);
    int returnval=1;
    int is_user_flag=0;
    //printf("total tokens are %d \n",num_commands);
    int  message_len= strlen(message);
        if(strcmp(tokens[num_commands-1],"-u") == 0) {
                 //printf("String contains -u %s\n",message);
                
                if(message_len>2) {
                    message[message_len-2] = '\0';
                }
                // updating variables
                num_commands= num_commands -1;
                is_user_flag =1;
                // printf("String contains -u after %s \n",message);
         }  
    // printf("Number of wrods eneterd by user %d \n",num_commands);
     if (strcmp(tokens[0],"findfile")  == 0) {
         //printf("Inside first condition %s \n",tokens[0]);
    //fflush(stdout);
        if(num_commands !=2) {
                    printf("[Correct Command Usage]  findfile  filename \n");
                    fflush(stdout);
                    returnval =0;
                 }
     }
     else  if (strcmp(tokens[0],"sgetfiles")  == 0) {
        if(num_commands !=3 ) {
                    printf("[Correct Command Usage]  sgetfiles size1 size2 <-u>\n");
                    fflush(stdout);
                    returnval =0;
                 }
        else {
               //printf("the size are %s %s \n",tokens[2],tokens[3]);
               // fflush(stdout);
               //SEG FAULT handling
                if (is_numeric(tokens[1]) && is_numeric(tokens[2])) {
                    long size1 = strtol(tokens[1], NULL, 10);
                    long size2 = strtol(tokens[2], NULL, 10);
                    // int size1= atoi(tokens[1]);
                    // int size2 =atoi(tokens[2]);
                    if(!(size1>=0 && size2>=0 && (size1<=size2))) {
                        printf("[Correct Command Usage]  sgetfiles size1 size2 <-u>\n");
                        printf("size1>=0 size>=0 and size1<=size2 ,current values are %d %d\n",size1,size2);
                        fflush(stdout);
                        returnval =0;
                    }
                } else {
                    returnval = 0;
                }
            }
        }
    
     else  if (strcmp(tokens[0],"dgetfiles")  == 0) {
        if(num_commands !=3 ) {
                    printf("[Correct Command Usage]  dgetfiles  date1 date2 <-u>\n");
                    fflush(stdout);
                    returnval =0;
                 }
        else {
                
                if(!(isValidDateFormat(tokens[1]) &&  isValidDateFormat(tokens[2]))) {
                    printf("[Correct Command Usage]  dgetfiles  date1 date2 <-u>\n");
                    printf("date1 date2 should follow the format yyyy-mm-dd and date1> date2\n");
                    fflush(stdout);
                    returnval =0;
                }
                else {
                   
                    if(!isdate1lessThanDate2(tokens[1],tokens[2])) {
                    printf("[Correct Command Usage]  dgetfiles  date1 date2 <-u>\n");
                    printf("date1 one must be less than equal to date2 \n");
                    fflush(stdout);
                    returnval =0;
                    }
                }


            }
        }

     
      else  if (strcmp(tokens[0],"getfiles")  == 0) {
        printf("the value of num_cmnd is %d\n",num_commands);
        if(!(num_commands >=2 && num_commands <7)) {
                    printf("[Correct Command Usage]  sgetfiles  file1 file2 file3 file4 file5 file6<-u>\n");
                    printf("Maximum six file names can be entered \n");
                    fflush(stdout);
                    returnval =0;
                 }
            }
     else  if (strcmp(tokens[0],"gettargz")  == 0) {
        if(!(num_commands >=2 && num_commands<7)) {
                    printf("[Correct Command Usage]  gettargz <extensionlist> <-u>\n");
                    printf("Minimum 1 and Maximum six extensions can be entered \n");
                    fflush(stdout);
                    returnval =0;
                 }
        
     }
     else {
        printf("Invalid Command Type !!! \n ");
        printf("Please choose one of the following commands \n");
        printf("1.    findfile filename \n");
        printf("2.    sgetfiles size1 size2 <-u> \n");
        printf("3.    dgetfiles date1 date2 <-u> \n");
        printf("4.    getfiles file1 file2 file3 file4 file5 file6 <-u>\n");
        printf("5.    gettargz <extensionlist> <-u> \n");
        printf("6.    quit \n");

        returnval=0;
     }

 
    result.isvalidmsg =returnval;
    result.containsuFlag=is_user_flag;
    return result;
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
        printf("Connected to mirror at %s:%d \n", MIRROR_IP, MIRROR_PORT);
    }

    while (1) {
        // scanf("%s", message);
        int is_valid;
        
            printf("Enter message to be sent to the server:\n");
            
            // fgets(buffer, sizeof(buffer),message);
            gets(message);
        // printf("Entered message is: %s", message);
        //process the message
        struct result_process_message values = process_message(message);
        is_valid = values.isvalidmsg;
         
    
        // strcat(message, "\n");
        //printf("Entered message is after porcessing : %s", message);
        if (is_valid) {
            
            send(sock, message, strlen(message), 0);

            if (strcmp(message, "quit") == 0) {
                exit(0);
            }
           
        } else {
            //printf("Message is not correct. try again\n");
            //fflush(stdout);
            continue;
        }

        // printf("Message sent to server\n");

        // Receive response from server
        // valread = read(sock, buffer, BUFFER_SIZE);

        // TODO: Send is_user_flag
        get_message(sock, buffer,values.containsuFlag);

        // printf("Response from the server: %s\n", buffer);
        // close(sock); // Close connection to server
    }

    return 0;
}

