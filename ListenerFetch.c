//
// Created by dustyn on 4/11/24.
//

#include "ListenerFetch.h"
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>

#define backlog 15
#define PORT "6969"

//This just takes a sockaddr struct and casts it to either a sockaddr_in struct (ipv4) or sockaddr_in6 struct (ipv6)
void *get_internet_address(struct sockaddr *addr) {
    //If AF_INET then this is IPV4 and cast accordingly
    if (addr->sa_family == AF_INET) {
        return &(((struct sockaddr_in *) addr)->sin_addr);
        //Else if it is AF_INET6 it is IPV6 and cast accordingly
    } else if (addr->sa_family == AF_INET6) {
        return &(((struct sockaddr_in6 *) addr)->sin6_addr);
    } else {
        fprintf(stderr, "Sockaddr was neither ipv4 or ipv6\n");
        exit(EXIT_FAILURE);
    }
}

//This is a function creating a listener socket for the server to listen for connections on
int get_listener_socketFd() {

    //The new listeners FD
    int listenerFd;

    //The 2 structs to pass to getaddr info and our pointer for iteration later
    struct addrinfo hints, *serverInfo, *pointer;

    //Set hints to 0s across the board to make sure it is fully empty
    memset(&hints, 0, sizeof hints);

    //UNSPEC since we will accept either ipv4 or ipv6
    hints.ai_family = AF_UNSPEC;

    //Stream socket type since this will be a TCP server
    hints.ai_socktype = SOCK_STREAM;

    //Set flag to PASSIVE to let the kernel provide addresses for us
    hints.ai_flags = AI_PASSIVE;

    //will help the return val of getaddr info on which gai_strerror can be called on if there is an error
    int return_value;

    //Will hold the length of connected client socket address
    socklen_t socklen;

    //Holds the yes value for the setsockoptions call
    int yes = 1;

    //We will attempt to fill our serverInfo struct pointed address with the results of getaddrinfo, error handle accordingly
    if ((return_value = getaddrinfo(NULL, PORT, &hints, &serverInfo)) != 0) {
        fprintf(stderr, "Error occurred on getaddrinfo() call .. %s\n", gai_strerror(return_value));
        exit(EXIT_FAILURE);
    }

    //Ensure that the set was filled properly
    if (serverInfo == NULL) {
        strerror(errno);
        exit(EXIT_FAILURE);
    }

    //Assign to pointer and iterate through the linked list to find a good address that we can use
    for (pointer = serverInfo; pointer != NULL; pointer = pointer->ai_next) {

        //Attempt to create a socket with the address in this entry in the list, continue to the next iteration on error code return value
        if ((listenerFd = socket(pointer->ai_family, pointer->ai_socktype, pointer->ai_protocol)) < 0) {
            continue;
        }

        //Here we will set the socket option REUSEADDR which will stop the program from yelling at us over address in use violations
        //error handle accordingly
        if (setsockopt(listenerFd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) != 0) {
            strerror(errno);
            exit(EXIT_FAILURE);


        }
        //Attempt to bind our listener to the given port, this is important as otherwise the kernel will bind a random port when we try to connect. This would make it
        //hard for clients to connect to us if the port is randomized.
        //Error handle accordingly
        if (bind(listenerFd, pointer->ai_addr, pointer->ai_addrlen) < 0) {
            close(listenerFd);
            continue;
        }
        //If we got here all was successful so break the loop
        break;

    }
    //Ensure that the bind was successful
    if (pointer == NULL) {
        perror("Error on bind\n");
        exit(EXIT_FAILURE);
    }

    //Free the result struct since we have our bound socket
    freeaddrinfo(serverInfo);

    //Listen on listener fd, error handle accordingly
    if (listen(listenerFd, backlog) != 0) {
        strerror(errno);
        exit(EXIT_FAILURE);
    }
    //Return our fd !
    return listenerFd;


}
