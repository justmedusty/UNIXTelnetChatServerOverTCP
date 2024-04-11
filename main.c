//
// Created by dustyn on 4/3/24.
//

#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "ListenerFetch.h"

//define our port on which our server listener socket will be bound


//message buffer size on the server
#define BUFFER_SIZE 1024


//Main function where we will utilize select as our block handler. Remember if you don't use something like poll or select, sends and recvs will block indefinitely until they are ready! This is less than ideal
int main() {

    //We will define our two file descriptor sets to select() on, we will make 2 the select set and the master set since select will manipulate the set so we want a clean set that we can have and only select on the select set to keep everything clean
    fd_set select_set, master_set;

    //The max file descriptor value, this will help with the loop so we can stop at the highest file descriptor value and restart the loop
    int fd_max;

    //Our listener socket
    int listener;

    //What will be the length of our client socket address
    socklen_t socklen;

    //this will store our client address, using sockaddr_storage since it is big enough for anything and can be cast later
    struct sockaddr_storage client_address;

    //the buffer which will store the server message, note if a message is longer than the buffer it will be truncated
    char buffer[BUFFER_SIZE];

    memset(&buffer,0,sizeof buffer);

    char server_message[BUFFER_SIZE];

    //return value of select call
    int return_value;
    //This will hold the new client sockets file descriptor when we accept new connections

    int newFd;
    //Bytes received which will be used to tell if a client has disconnected

    size_t bytes_received;

    //This will hold the client IP addr, of size INET_6 because it is big enough to hold either
    char clientIP[INET6_ADDRSTRLEN];

    //We will get our listener and assign the file descriptor to our listener variable, using the function we wrote above
    listener = get_listener_socketFd();

    //This is out timeout value, it will time out the select() call and move on
    struct timeval tv;
    //half million microseconds (half second) and 5 seconds so 5 and a half seconds!
    tv.tv_sec = 5;
    tv.tv_usec = 500000;

    //fd_max is listeners file descriptor which just means that we don't want to iterate higher than the highest file descriptor
    fd_max = listener;

    //We will zero both sets to make sure they are ready to go
    FD_ZERO(&select_set);
    FD_ZERO(&master_set);

    //We will add our listener to the master set
    FD_SET(listener, &master_set);

    //Start our infinite loop
    for (;;) {

        //We want to be sure to zero both the buffer and the server message to prevent any abnormalities or undefined behaviours
        memset(&server_message,0,sizeof server_message);
        memset(&buffer,0,sizeof buffer);

        //Select set assign the values in our master set , remember since select() will modify the set it is smart to work off a "worker set" if you will and have a clean master set for storing your FDs untouched.
        select_set = master_set;

        //We will call select with our number of fds set at max + 1, we will pass select set as a reading set since this is a server not a client
        //handle errors accordingly
        if ((return_value = select(fd_max + 1, &select_set,NULL, NULL, &tv)) <= 0) {
            //This means timeout, we don't want to exit on failure in a timeout
            if(return_value == 0){
                continue;
            }
            //If it wasn't a timeout an error has occured ,print the error and close the program
            strerror(return_value);
            exit(EXIT_FAILURE);
        }

        //Start main select loop, loop through file descriptors and if we find one in the set...
        for (int i = 0; i <= fd_max + 1; i++) {

            if (FD_ISSET(i, &select_set)) {

                //Check if it is the listener, if it is this means there is a connection waiting, and we need to accept it. We will assign the return value of accept (the file descriptor if no error occurs) to newFd
                if (i == listener) {

                    socklen = sizeof client_address;
                    newFd = accept(listener, (struct sockaddr *) &client_address, &socklen);

                    //Standard error checking
                    if (newFd == -1) {

                        //Print the error using strerror
                        strerror(errno);

                    } else {

                        //Else we will add this new Fd to the master set, and if the file descriptor is larger than the max this is our new max
                        FD_SET(newFd, &master_set);
                        if (newFd > fd_max) {

                            fd_max = newFd;
                        }


                        //We will print the new connection and their ip address derived from the inet network to presentation call
                        printf("Server : New Connection from %s\n", inet_ntop(client_address.ss_family,
                                                                              get_internet_address(
                                                                                      (struct sockaddr *) &client_address),
                                                                              clientIP, INET6_ADDRSTRLEN));


                        //We'll go ahead and use sprintf which will let us also put the file descriptor into the server message
                        sprintf(server_message,"User %d joined\n",newFd);

                        //Here we will loop through all FDs in the set and as long as it is not the listener or the user that just joined, we will send the message that we just stored in the buffer
                        for (int j = 0; j <= fd_max; j++) {
                            if (FD_ISSET(j, &master_set)) {
                                if (j != listener && j != newFd) {
                                    if (send(j, server_message, sizeof server_message, 0) == -1) {
                                        perror("send");

                                    }

                                }
                            }
                        }

                    }

                    //Else this is not the listener and is a client
                } else {

                    //If this is an error or a 0, if it is 0 then it means a client disconnected
                    if ((bytes_received = recv(i, buffer, sizeof buffer, 0)) <= 0) {

                        //If it is a hangup message then print to console, and then send a message to each other connected user in the chat, obviously excluding the listener and the one hanging up
                        if (bytes_received == 0) {
                            printf("Client socket %d disconnected\n", i);
                            //Copy our server message into the server message, let users know that a user has left the chat
                            sprintf(server_message,"User %d left\n",i);

                            //Our server-wide broadcast to the other users connected, not the listener or the user that left
                            for (int j = 0; j <= fd_max; j++) {
                                if (FD_ISSET(j, &master_set)) {
                                    if (j != listener && j != i) {
                                        if (send(j, server_message, strlen(server_message), 0) == -1) {
                                            perror("send");

                                        }

                                    }
                                }
                            }
                            //Else an error occurred
                        } else {
                            perror("recv");
                        }
                        //Close the file descriptor and clear it from the master set since this client hung up
                        close(i);
                        FD_CLR(i, &master_set);

                        //Else this is a client with a message to send, so we will send it to all others , the message was written into the buffer so it will broadcast to the other users right out of the buffer
                    } else {

                        /*
                         * Here for our server-wide broadcast we will use sprintf to prepend the
                         * file descriptor of the user so that others may be able to identify who is speaking
                         * in the chat. Each user will be assigned the file descriptor of the socket they are communicating on
                         */
                        char message[BUFFER_SIZE];
                        strcpy(message,buffer);
                        char prepend_string[10];
                        sprintf(prepend_string,"User %d:",i);
                        sprintf(buffer,"%s %s",prepend_string,message);

                        for (int j = 0; j <= fd_max; j++) {
                            if (FD_ISSET(j, &master_set)) {
                                if (j != listener && j != i) {
                                    if (send(j, buffer, bytes_received + (sizeof prepend_string), 0) == -1) {
                                        perror("send");
                                    }

                                }
                            }
                        }

                    }
                }

            }
        }
    }

}