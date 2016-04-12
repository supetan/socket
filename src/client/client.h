#ifndef __CLIENT_H__
#define __CLIENT_H__

#ifndef _SYSSOCKET_H_
#define _SYSSOCKET_H_
#include <sys/socket.h>  /* defines socket, connect, ... */
#endif /* _SYSSOCKET_H_ */

#ifndef _NETINETIN_H_
#define _NETINETIN_H_
#include <netinet/in.h>  /* defines sockaddr_in */
#endif /* _NETINETIN_H_ */

#ifndef _ARPAINET_H_
#define _ARPAINET_H_
#include <arpa/inet.h>   /* inet_pton, ... */
#endif /* _ARPAINET_H_ */


#include <time.h>
#include <fcntl.h>
#include "../flash_main.h"

#define PARAMETERS_TO_READ 3
#define CONNECTION_TIME 10 //Total wait time before declaring no response is received from any server
#define CLOCKS_PER_USEC 1000000L


//#define TCP_SUPPORT 


uint8_t set_nonblock(uint8_t sockfd);
uint8_t read_from_socket(uint8_t udpfd,char*recvline);
uint8_t select_random_server();
uint8_t read_server_info(char* input_server_addr,uint32_t* portNum, uint8_t serverno);
void remove_newline(char *sendline);
uint8_t create_socket_descriptor(uint8_t server_len, uint8_t* isIPV6);
uint8_t handle_client_Request(uint8_t udpfd,char* server_address, uint32_t portNum,uint8_t isIPV6,char*sendline );

#endif /* __CLIENT_H__ */
