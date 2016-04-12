/********************************************************************
 *     ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_query_responder.c
 *
 *  This file has functions used to respond to UDP queries.
 *
 *******************************************************************/
#include "../flash_main.h"

#include "server_main.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
extern int g_enable_debug_messages;
extern pthread_mutex_t g_log_mutex;
extern int g_is_server_log_init_successful;

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
uint8_t server_query_responder_init ()
{
  struct sockaddr_in6 servaddr;
  pthread_t t_read_udp;
  int sockudp;

  LOG_SERVER_MESSAGE_TO_FILE("ENTRY", "Initializing server query responder..")

  if (mainp == NULL) {
    LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Main pointer is NULL. Something very wrong!");
    return F_FAILURE;
  }

  /* Create socket */
  if ((sockudp = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
      LOG_SERVER_MESSAGE_TO_FILE("ERROR", "UDP socket creation failed in server.")
      return F_FAILURE;
  }

  /* Pick a port and bind socket to it. */
  /* Accept connections from any address. */
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin6_family = AF_INET6;
  servaddr.sin6_addr = in6addr_any;
  servaddr.sin6_port = htons(UDP_PORT_NUMBER);

  if (bind(sockudp, (struct sockaddr *) &servaddr,
      sizeof(servaddr)) < 0) {
      LOG_SERVER_MESSAGE_TO_FILE("ERROR", "UDP bind failed in server.")
      return F_FAILURE;
  }

  mainp->udp_sock_fd = sockudp;

  /* New thread for receiving */
  LOG_SERVER_MESSAGE_TO_FILE("INFO", "Creating thread to listen on socket and handle "
                             "clients.");
  if (pthread_create (&t_read_udp, NULL, &server_query_responder_handle_request,
                      NULL) != 0) {
    LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Thread creation failed.");
    return F_FAILURE;
  }
  mainp->t_read_udp = t_read_udp;
  
  return F_SUCCESS;
}

void *server_query_responder_handle_request(void *arg)
{
  struct sockaddr_storage cliaddr;
  const char *resp_not_found = "No entries found";
  char buff[MAXLINE];
  char line[MAXLINE]; 
  char *ip = NULL;
  char *domain_name = NULL; 
  char *key = NULL; 
  char *user_query = NULL;
  socklen_t len;
  int found = FALSE; 
  int nBytes = 0;

  if ((mainp == NULL) || (mainp->db_fp == NULL) || (mainp->udp_sock_fd == -1)) {
    LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Main pointers are NULL. Something very wrong."
                 " Exit thread");
    pthread_exit (NULL);
  }

  /* Register for signal handling */
  signal_registration ();

  len = sizeof(cliaddr);
  while(1) {
    memset(buff, '\0', MAXLINE);
    nBytes = recvfrom (mainp->udp_sock_fd, buff, MAXLINE, 0,
      (struct sockaddr *)&cliaddr,&len);
    key = strtok (buff," ");
    user_query = strtok (NULL,"\n");
    if (strncmp (key, SECURITY_KEY, strlen(key)) == 0)
    {
       LOG_SERVER_MESSAGE_TO_FILE("INFO", "Security key is accepted");
  	   LOG_SERVER_MESSAGE_TO_FILE("INFO", "Server received query: %s",user_query);

       server_mutex_lock(&mainp->db_mutex); /* lock the DB file for reading */

       /* move the file pointer to the beginning of the DB file */
       fseek(mainp->db_fp, 0, SEEK_SET);

       found = FALSE;
       while (fgets (line, MAXLINE, mainp->db_fp))
       {
         /* domain name */
         domain_name = strtok (line," "); 
         /* version */
         ip = strtok (NULL," "); 
         /* ip address */
         ip = strtok (NULL," "); 
         /* strlen of prefix "flash " is 6 and 1 is for \n at the end */
         if (strlen (domain_name) == (nBytes - 7)) 
         {
           if (strncmp(domain_name, user_query, strlen (domain_name)) == 0)
           {
             LOG_SERVER_MESSAGE_TO_FILE("INFO", "Sending response - IP %s, "
                      "Domain Name %s", ip, domain_name);
      	     sendto(mainp->udp_sock_fd, ip, MAXLINE, 0,
                      (struct sockaddr *)&cliaddr, len);
             found = TRUE;
             break;
           }
         }
       }
       server_mutex_unlock(&mainp->db_mutex); /* unlock the DB file after reading */
       if (found == FALSE)
       {
         LOG_SERVER_MESSAGE_TO_FILE("INFO", "Entry not found. "
                   "Sending 'not found' to client");
         sendto (mainp->udp_sock_fd, resp_not_found, MAXLINE, 0,
                (struct sockaddr *)&cliaddr,len);
       }
    }
    else
    {
        LOG_SERVER_MESSAGE_TO_FILE("INFO","Client sends wrong security key: %s", key)
    }

  }
  pthread_exit (NULL);
}

/************************* END OF FILE *************************/
