#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>

static long int MAX_QUEUE_LIMIT = 10000;
static long int MAX_BUFFER_SIZE = 999999;

struct client_request
{
    char hostname[100];
    char port_number[10];
};

// Terminates the proxy indicating error
void closeWithError(char *error);

// Function accepts client socket and exhchanges data with the client and the server
void exchangeDataWithClient(int client_socket);

// It is used to parse the client request and obtain the server hostname and port
int parseRequest(char *requestString, struct client_request *request) ;

//Trims the request
char * removeExtraSpaces(char* string);

int
main(int argc, char **argv)
{
  
  //Initializations
    int listenfd, connectionfd;
    struct sockaddr_in  cliaddr, servaddr;
    socklen_t client_length;

    // Open a socket
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // Setup Proxy address
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));

    //Bind the Socket
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        closeWithError("Bind Error for the request!");
    }

    // Mark the port as accepting connections
    if (listen(listenfd,MAX_QUEUE_LIMIT) == -1)
    {
        closeWithError("Listen Error for the request!");
    }

    //Accept connections infinitely
    for (;;)
    {
        client_length = sizeof(cliaddr);
        connectionfd = accept(listenfd, (struct sockaddr *)&cliaddr,&client_length);
        if (connectionfd == -1)
        {
            closeWithError("Accept failed.\n");

        }

        printf("Connection opened.\n");

        //Communicate with the client
        exchangeDataWithClient(connectionfd);

        printf("Connection closed.\n");
        //Close the connection
        close(connectionfd);

        //Reset the client address
        bzero(&cliaddr, sizeof(cliaddr));

    }
  }

void exchangeDataWithClient(int client_socket) {

    //Initialize the data
    char buffer[MAX_BUFFER_SIZE], server_buffer[MAX_BUFFER_SIZE], raw_buffer[MAX_BUFFER_SIZE];
    int receivedMessageSize, serverCommunicationSize;
    struct client_request request;
    struct sockaddr_in  web_server_socket, proxy_server_socket;
    int read_connection;
    struct hostent *host_entity;

    char *badMessage="HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\n<html><head>Error 400. Bad Request</head><body>Error 400. Bad Request</body></html>";
    // printf("Bad Message string: %s\n", badMessage);

    memset(buffer,0,MAX_BUFFER_SIZE);

    //Revceive the request from the client
    if ( (receivedMessageSize = recv(client_socket,&buffer,MAX_BUFFER_SIZE,0)) != 0)
    {
        memset(server_buffer,0,MAX_BUFFER_SIZE);
        memset(raw_buffer,0,MAX_BUFFER_SIZE);

        while (receivedMessageSize) {
            memcpy(&raw_buffer,&buffer,strlen(buffer));

            read_connection = socket(AF_INET, SOCK_STREAM, 0);
            bzero(&proxy_server_socket, sizeof(proxy_server_socket));
            bzero(&request, sizeof(request));

            //Parse the request
            if(parseRequest(raw_buffer,&request) == -1){
                printf("Return as the request is not parsed.\n");
                send(client_socket,&badMessage,strlen(badMessage),0);
                return;
            }
            printf("Request is: %s\n", buffer);

            //Processing address for server connection
            proxy_server_socket.sin_family = AF_INET;

            //Check if it is an IP address or a host name.
            if ((request.hostname[0] >= 'a' && request.hostname[0] <= 'z') ||  (request.hostname[0] >= 'A' && request.hostname[0] <= 'z'))
            {
                printf("Parsing for host name:%s\n", request.hostname);
                if ((host_entity = gethostbyname(request.hostname)) != NULL)
                {
                    memcpy(&proxy_server_socket.sin_addr, (struct in_addr*)host_entity->h_addr,host_entity->h_length);
                }
                else {
                    printf("Could not resolve host.\n");
                    send(client_socket,&badMessage,strlen(badMessage),0);
                    return;
                }

                printf("s_addr = %s\n", inet_ntoa((struct in_addr)proxy_server_socket.sin_addr));
            }
            else {
                printf("Parsing for IP:%s\n", request.hostname);
                 if(inet_aton(request.hostname,&proxy_server_socket.sin_addr) == 0){
                    printf("Could not resolve host.\n");
                    send(client_socket,&badMessage,strlen(badMessage),0);
                    return;
                 }
            }

            //Set the port
            proxy_server_socket.sin_port = htons(atoi(request.port_number));

            // Open a connection
            int connectionResult = connect(read_connection, (struct sockaddr *) &proxy_server_socket, sizeof(proxy_server_socket));

            if (connectionResult == -1)
            {
                    printf("Could not connect to web.");
                    send(client_socket,&badMessage,strlen(badMessage),0);
                    return;
            }

            printf("Connected to the web\n");
            printf("Request: %s\n", buffer);

            // Send data to server
            int sent_to_server_size, sent_to_client_size;
            if((sent_to_server_size = send(read_connection,&buffer,receivedMessageSize,0)) == receivedMessageSize) {

                printf("Sent request to web. Bytes = %d \n",sent_to_server_size);

                //CHeck the client socket capacity.
                int client_capacity = 0, buffer_capacity = 0,error =0;
                unsigned int length_of_cap = sizeof(client_capacity);
                unsigned int length_of_err = sizeof(error);
                if(getsockopt(client_socket,SOL_SOCKET,SO_RCVBUF,(void *)&client_capacity, &length_of_cap) != 0) {
                    return;
                }

                printf("Max buffer capacity of client:%d\n", client_capacity);

                if (client_capacity < MAX_BUFFER_SIZE)
                {
                    buffer_capacity = client_capacity;
                }
                else {
                    buffer_capacity = MAX_BUFFER_SIZE;
                }
                
                // Receive response from server
                serverCommunicationSize = recv(read_connection,&server_buffer,buffer_capacity,0);
                do 
                {
                    printf("Sending response to client  Bytes: %d\n",serverCommunicationSize);

                     //Check any error in client socket
                    if(getsockopt(client_socket,SOL_SOCKET,SO_ERROR,(void *)&error, &length_of_err) != 0) {
                        printf("Could not check if socket is connected.\n");
                        return;
                    }

                    if (error != 0)
                    {
                        printf("Error in client socket:%s\n", strerror(error));
                        return;
                    }

                    // Send response back to client
                    if ((sent_to_client_size = send(client_socket,&server_buffer,serverCommunicationSize,0)) != serverCommunicationSize) {
                        printf("Could not write to client socket.");
                        return;
                    }
                     // printf("%s\n",server_buffer);
                    memset(server_buffer,0,MAX_BUFFER_SIZE);
                    printf("Sent response to client, Bytes: %d \n",sent_to_client_size);

                    int client_capacity_in = 0, buffer_capacity_in = 0;
                    unsigned int length_of_cap_in = sizeof(client_capacity_in);
                    if (getsockopt(client_socket,SOL_SOCKET,SO_RCVBUF,(void *)&client_capacity_in, &length_of_cap_in) != 0) {
                        return;
                    }

                    printf("Max buffer capacity of client:%d\n", client_capacity_in);


                    if (client_capacity_in < MAX_BUFFER_SIZE)
                    {
                        buffer_capacity_in = client_capacity_in;
                    }
                    else {
                        buffer_capacity_in = MAX_BUFFER_SIZE;
                    }

                    // Receive response from server and check if all the response is received.
                    serverCommunicationSize = recv(read_connection,&server_buffer,buffer_capacity_in,0);
                } while(serverCommunicationSize > 0);
            }
            else {
                printf("Error while receiving from server\n");
                send(client_socket,&badMessage,strlen(badMessage),0);
            }
            printf("Out of server loop\n");

            // Close connection with the server
            close(read_connection);
            memset(buffer,0,MAX_BUFFER_SIZE);

            // Receive message from client
            receivedMessageSize = recv(client_socket,&buffer,MAX_BUFFER_SIZE,0);
            printf("new buffer size while receiving from client: %d\n",receivedMessageSize);
        }
    }
    else {
        printf("Error while receiving from client\n");
    }


}

int parseRequest(char *requestString, struct client_request *request) {

    
    //Check if the request is GET else return to close the connection
    if (strstr(requestString,"GET") != NULL)
    {
        printf("It is a get request\n");

        if (strstr(requestString,"HTTP/1.1") == NULL)
        {
            return -1;
        }

        printf("It is a HTTP/1.1 request\n");

        char *hostStart;
        
        if ((hostStart = strstr(requestString,"Host:")) == NULL)
        {
            printf("Could not retrieve host for HTTP/1.1\n");
            return -1;
        }

        if (strlen(hostStart) <= 5)
        {
            printf("Could not parse host from the request.\n");
            return -1;
        }

        printf("We have successfully parsed host.\n");

        
        //Find the Host
        hostStart += 5;
        hostStart = removeExtraSpaces(hostStart);
        
        int i = 0, didFindNewLineCharacters = 0;
        while(hostStart[i] != 13 && hostStart[i] != 10 && hostStart[i] != ' ') 
           {
            i++;
            didFindNewLineCharacters = 1;
           } 
        
        if (i<strlen(hostStart) && didFindNewLineCharacters)
        {
            hostStart[i] = 0;
        }

        if (strlen(hostStart) == 0)
        {
            return -1;
        }

        //Check if there is any host port specified.
        if (strstr(hostStart,":"))
        {
            printf("Reached for host with port,%s\n", hostStart);
            strncpy(request->hostname,hostStart,strstr(hostStart,":")-hostStart);
            char * port_number = strstr(hostStart,":") + 1;

            if (strlen(port_number) == 0) 
            {   
                return -1;
            }

            strncpy(request->port_number,port_number,strlen(port_number));
        }
        else {

            strncpy(request->hostname,hostStart,strlen(hostStart));
            strncpy(request->port_number, "80",2);
        }

        printf("Host is:%s\n", request->hostname);
        printf("Port is:%s\n", request->port_number);

    }
    else {
        return -1;
    }

    return 0;

}

char * removeExtraSpaces(char* string) {

    //trim from front
    int count = strlen(string);
    int i;
    for (i = 0; i < count; ++i)
    {
        /* code */
        if (string [i] == ' ')
        {
            continue;
        }
        else {
            break;
        }
    }

    string = string + i;

    //trim from back
    while(string[strlen(string) -1 ] == ' ' || string[strlen(string) -1 ] == '\n') {
        string[strlen(string) -1 ] = '\0';
    }


    int length = strlen(string);
    char * outputString = string;

    int j=0;
    for (i = 0; i < length; ++i)
    {
        if (string[i] == ' ' && string[i-1] == ' ' )
        {
            continue;
        }
        else {
            outputString[j] = string[i];
            j++;
        }
    }

    return outputString;

}


void closeWithError(char *error) {

    printf("%s\n",error);
    exit(1);
}

