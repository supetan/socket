#include "client.h"
#include "../flash_main.h"
#include <sys/time.h>

static uint8_t g_EACH_SERVER_RESPONSE_WAIT_TIME =1;
extern int g_enable_debug_messages;

/*This function sets the read socket to non-blocking mode*/
uint8_t set_nonblock(uint8_t sockfd)
{
    uint8_t val;
	
	val = fcntl(sockfd, F_GETFL, 0);
	if (fcntl(sockfd, F_SETFL, val | O_NONBLOCK) < 0) {
		LOG_PERROR_MESSAGE("fcntl(sockfd)");
		return FALSE;
	}

	return TRUE;
}

/* This function reads the strings from server and stores in an array*/
uint8_t read_from_socket(uint8_t udpfd,char*recvline)
{
  time_t start_time = time(NULL);
  time_t current_time = time(NULL);

  while (current_time < start_time + g_EACH_SERVER_RESPONSE_WAIT_TIME)
  {
	 if((recvfrom(udpfd,recvline,MAXLINE,FALSE,NULL, NULL)) > 0)//If read is successfull, return else continue until timer expires
	 {
		return TRUE;
	 }
  	current_time = time(NULL);
  }
  return FALSE;
}
		
uint8_t select_random_server()//generates random number to select the server from the list 
{
   time_t t = 0;
   uint8_t rand_server;
   /* Intializes random number generator */
   srand((unsigned) time(&t));
   rand_server = rand();
 
   return rand_server;
}

uint8_t read_server_info(char* input_server_addr,uint32_t* portNum, uint8_t serverno) //reads the server info from the file
{
	FILE *fp;
	uint8_t servcount = FALSE; //Initialize to 0 
	uint32_t server_priority;
	fp = fopen(SERVER_LIST_PATH, "r");
	if (fp == NULL)
	{
	  LOG_PERROR_MESSAGE("Can't open file server_list!\n");
	  return FALSE;
	}

	while ( servcount <= serverno) 	
	{
		if(fscanf(fp, "%s %d %d", input_server_addr, portNum, &server_priority) != PARAMETERS_TO_READ) //read three variable from the file ip-address and port no.
		{
			LOG_MESSAGE(INFO, stdout,"Reading line in file failed\n");
			LOG_MESSAGE(INFO, stdout,"Create a file in <format Ip-address> <portnum> <server_priority>\n");
			return FALSE;
		}
		servcount++;
	}
	fclose (fp);
	return TRUE;
}

/* This function removes the newline character from the input string*/
void remove_newline(char *sendline)
{
	uint8_t n=0;
	while(sendline[n] != '\n')
	{
		  n++;
	}
	sendline[n] = '\0';
}

uint8_t create_socket_descriptor(uint8_t server_len, uint8_t* isIPV6) //Creates socket descriptor
{
	uint8_t udpfd;
	uint8_t server_family;
#ifdef TCP_SUPPORT
	uint8_t socket_type = SOCK_STREAM;
#else
	uint8_t socket_type = SOCK_DGRAM;
#endif

	if(server_len <= IPV4_STRING_SIZE)
	{
		server_family = AF_INET;
		*isIPV6 = FALSE;
	}
	else
	{
		server_family = AF_INET6;
		*isIPV6 = TRUE;
	}

	if ( (udpfd = socket(server_family, socket_type, FALSE)) < 0) 
	{
		LOG_PERROR_MESSAGE("socket error");
		return FALSE;
	}

	return udpfd;

}

/* This function writes the user query to socket*/
uint8_t handle_client_Request(uint8_t udpfd,char* server_address, uint32_t portNum,uint8_t isIPV6,char*sendline )
{
	struct sockaddr_in6 servaddr6;
	struct sockaddr_in servaddr;
	uint8_t nBytes = strlen(sendline)+1;

	uint8_t len;
	
	switch(isIPV6)
	{
		case TRUE:
			{		
			   memset(&servaddr6, 0, sizeof(servaddr6));
			   servaddr6.sin6_family = AF_INET6;
			   servaddr6.sin6_port = htons(portNum);
			   servaddr6.sin6_addr = in6addr_any;	
			   len = sizeof(servaddr6);
			   
				if (inet_pton(AF_INET6, server_address, &servaddr6.sin6_addr) <= 0) 
				{
						LOG_PERROR_MESSAGE("inet_pton error for\n");
						return FALSE;
				}
#ifdef TCP_SUPPORT
			    // Connect to IP address and port indicated by servaddr
			    // Check if it was succesful
			    if (connect(udpfd,
			                (struct sockaddr *) &servaddr6,
			                sizeof(servaddr6)) < 0) {
			        LOG_PERROR_MESSAGE("connect error");
			        return FALSE;
			    }	
#endif

				/*Send message to server*/
				if((sendto(udpfd,sendline,nBytes,0,(struct sockaddr *)&servaddr6,len)) < 0)
				{
					  LOG_PERROR_MESSAGE("Send Error from the socket\n");
					  return FALSE;

				}

				break;
			}

		case FALSE:
			{		
				memset(&servaddr, 0, sizeof(servaddr));
				servaddr.sin_family = AF_INET;
				servaddr.sin_port	= htons(portNum); 
				len = sizeof(servaddr);

				if (inet_pton(AF_INET, server_address, &servaddr.sin_addr) <= 0) 
				{
						LOG_PERROR_MESSAGE("inet_pton error for %s\n");
						return FALSE;
				}

#ifdef TCP_SUPPORT
				// Connect to IP address and port indicated by servaddr
				// Check if it was succesful
				LOG_MESSAGE(INFO, stdout, "Trying TCP Connection\n");
				if (connect(udpfd,
							(struct sockaddr *) &servaddr,
							sizeof(servaddr)) < 0) {
					LOG_PERROR_MESSAGE("connect error");
					return FALSE;
				}	
#endif
				
				/*Send message to server*/
				if((sendto(udpfd,sendline,nBytes,0,(struct sockaddr *)&servaddr,len)) < 0)
				{
					  LOG_PERROR_MESSAGE("Send Error from the socket\n");
					  return FALSE;
				}

				break;
			}
		default:
			{
				//Should not come here;
				return FALSE;
				break;
			}
		}


return TRUE;

}

uint8_t client_init(void)
{
	uint8_t udpfd;
	uint32_t portNum;
	char recvline[MAXLINE + 1],input_server_addr[MAXLINE + 1] ;
	char sendline[MAXLINE + 1],buffer[MAXLINE + 1];
	uint8_t select_server;
	uint8_t server_len,isIPV6;
	uint8_t retry_query = FALSE; //This vairable is set if the response is not received fromt the server in first attempt
							 // If this variable is set, the query timer is not updated that is used to calculate performance metric

	time_t current_time = time(NULL);
	time_t start_connection_time = time(NULL);

	struct timeval query_sent_t, response_received_t, round_trip_time_t;	// To calculate performance		 

	uint8_t rsp_received = FALSE; 							 
	memset(recvline,'\0',MAXLINE + 1);
	memset(sendline,'\0',MAXLINE + 1);
	memset(input_server_addr,'\0',MAXLINE + 1);
	memset(buffer,'\0',MAXLINE + 1);


	LOG_MESSAGE(INFO, stdout,"Enter your query:\n");
	LOG_MESSAGE(INFO, stdout,"Format : home.flash.com\n");

	fgets(buffer,MAXLINE,stdin);
	remove_newline(buffer);

	sprintf(sendline, "%s %s",SECURITY_KEY,buffer); //prefix actual query with code flash  to avoid malicious client in server

	select_server = select_random_server(); 
	retry_query = FALSE; //Make sure this is set to false when the query is made first time

	for(start_connection_time = time(NULL);current_time < start_connection_time + CONNECTION_TIME;current_time = time(NULL))
	{
		select_server = select_server % NUM_OF_SERVERS; //select random server at first from first server. 
		if(FALSE == read_server_info(input_server_addr,&portNum,select_server))
		{
			LOG_MESSAGE(INFO, stdout,"Read Server info from file failed\n");
			continue;
		}

		LOG_MESSAGE(INFO, stdout,"Connecting to %s %d...\n",input_server_addr,portNum);
		
		server_len = strlen(input_server_addr);
		
		if((udpfd = create_socket_descriptor(server_len,&isIPV6)) == FALSE)
		{
			LOG_PERROR_MESSAGE("socket descriptor creation failed\n");
			continue;

		}
#ifndef TCP_SUPPORT		
		if (set_nonblock(udpfd) == 0) 
		{  
			LOG_PERROR_MESSAGE("Sockfd noblock set failed\n");	
			continue;

		}
#endif
		if(FALSE == handle_client_Request(udpfd,input_server_addr,portNum,isIPV6,sendline ))
		{
			LOG_PERROR_MESSAGE("write to socket failed\n");
			select_server++; //increment to select next server from list. 
			continue;
		}

		if(!retry_query)
		{	
			gettimeofday(&query_sent_t, NULL); // stores the time at which the query was sent for the first time 
			
		}
		/*Receive message from server*/
		if(FALSE == read_from_socket(udpfd,recvline))
		{
			select_server++; //increment to select next server from list. 
			close(udpfd);
			retry_query = TRUE;
			g_EACH_SERVER_RESPONSE_WAIT_TIME+=1; //In case the response is not received within specified time, increase the wait timer by 1 s
			continue;

		}
		
		gettimeofday(&response_received_t, NULL);		
		round_trip_time_t.tv_usec = ((response_received_t.tv_sec - query_sent_t.tv_sec)*CLOCKS_PER_USEC + response_received_t.tv_usec) - query_sent_t.tv_usec;

		LOG_MESSAGE(INFO, stdout, "Mapped address: %s\n",recvline);
		LOG_MESSAGE(INFO, stdout, "Total time taken by CPU: %ld us\n", round_trip_time_t.tv_usec);
		rsp_received = TRUE;
		break;

	}

	if(!rsp_received)
	{
		LOG_MESSAGE(INFO, stdout, "Connection Timeout\nPlease retry after some time\n");
	}

	return FALSE;
}


