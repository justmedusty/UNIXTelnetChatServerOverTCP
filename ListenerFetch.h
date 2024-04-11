
//
// Created by dustyn on 4/11/24.
//
#include <sys/socket.h>
#ifndef UNIXTELNETSERVEROVERTCP_LISTENERFETCH_H
#define UNIXTELNETSERVEROVERTCP_LISTENERFETCH_H
int get_listener_socketFd();
void *get_internet_address(struct sockaddr*);
#endif //UNIXTELNETSERVEROVERTCP_LISTENERFETCH_H
