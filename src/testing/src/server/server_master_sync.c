/********************************************************************
 *     ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_master_sync.c
 *
 *  This file has master server functions implemented
 *  for a database synchronization between servers.
 *
 *******************************************************************/
#include "../flash_main.h"
#include "server_main.h"
#include "server_sync.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
extern int g_enable_debug_messages;
extern pthread_mutex_t g_log_mutex;
extern int g_is_server_log_init_successful;
extern char g_db_file_name[FILENAME_MAX];

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
#ifdef NAME_SERVER_MASTER
struct server_master_sync *master_sync = NULL;

/********************************************************************
 * Init and de-init functions
 *******************************************************************/
uint8_t server_master_sync_init ()
{
  if (!mainp) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Something very wrong!");
    return F_FAILURE;
  }

  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Initializing server master. Calling global init,"
                            " DB init and TCP sock init to clients");
  server_master_sync_global_init ();

  /* DB file related changes */
  server_sync_master_db_init ();

  /* Create TCP socket */
  if (server_master_sync_sock_init () != F_SUCCESS) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "TCP socket creation failed for some reason");
    return F_FAILURE;
  }
  return F_SUCCESS;
}

void server_master_sync_deinit ()
{
  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "De-init everything. Shutting down server. Bye!");

  /* Release global structure */
  if (master_sync != NULL) {
    free (master_sync);
    master_sync = NULL;
  }
}

void server_master_sync_global_init ()
{
  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Initialize the global structre used across "
                              "master");

  /* Initialize the global structure */
  master_sync = malloc(sizeof (struct server_master_sync));
  memset (master_sync, 0, sizeof (struct server_master_sync));
  memset (&master_sync->slave_sock_fd_list, 0, SLAVE_COUNT_MAX);
  master_sync->slave_count = 0;

  mainp->master_sync = master_sync;
}

uint8_t server_sync_master_db_init ()
{
  pthread_t t_read_db = {0};

  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Initialize the DB related functions in Master");

  if (mainp == NULL) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Something very wrong here!");
    return F_FAILURE;
  }

  /* Set the file descriptor in the global structure */
  master_sync->db_fp = mainp->db_fp;

  /* Strart thread for timer */
  if (pthread_create (&t_read_db, NULL, &server_sync_master_handle_db_timer,
                      NULL) != 0) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Thread creation for reading DB failed."
                              "No more changes to DB please");
    return F_FAILURE;
  }

  mainp->t_read_db = t_read_db;

  return F_SUCCESS;
}

uint8_t server_master_sync_sock_init ()
{
  struct sockaddr_in6 server_addr = { 0 };
  pthread_t thread = { 0 };
  int sock_fd = 0;

  if (!mainp) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Something very wrong here!");
    return F_FAILURE;
  }

  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Attempting TCP socket creation for master.");

  if ((sock_fd = socket (AF_INET6, SOCK_STREAM, 0)) < 0) 
  {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Socket creation failed.");
    return F_FAILURE;
  }

  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin6_family = AF_INET6;
  server_addr.sin6_port = htons(SERVER_SYNC_PORT);
  server_addr.sin6_addr = in6addr_any;
 
  if (bind(sock_fd, (struct sockaddr*)&server_addr,
            sizeof (server_addr)) < 0) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Bind failed.");
    return F_FAILURE;
  }

  /* Set the global tcp socket in master_sync */
  master_sync->tcp_listen_fd = sock_fd;

  /* Create a thread to listen to slave connections.
   * Store the slave socket descriptors and exit.
   * During update, use the list of slave socket descriptors
   * and send update.
   */
   if (pthread_create (&thread, NULL, &server_master_sync_listen,
                       NULL) != 0) {
     LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Thread creation failed. All hope lost!");
     return F_FAILURE;
   }
   mainp->t_tcp_listen = thread;

   return F_SUCCESS;
}

/********************************************************************
 * Thread handler functions
 *******************************************************************/

void *server_master_sync_listen (void *arg)
{
  struct sockaddr_in slave_addr = { 0 };
  socklen_t slave_sock_len      = 0;
  int listen_fd                 = 0;
  int slave_sock_fd             = 0;
  char read_write_buffer[MAXLINE];

  if (master_sync == NULL) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Argument to thread is no good");
    pthread_exit (NULL);
  }

  /* Register for the signals */
  signal_registration();

  /* Get socket descriptor from global structure */
  listen_fd = master_sync->tcp_listen_fd;

  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Attempting listen on a new thread, on socket %d",
              listen_fd);

  if (master_sync->slave_count >= SLAVE_COUNT_MAX) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Cannot accept any more clients. Exiting "
                                "thread. Maybe change the macro?!");
    pthread_exit (NULL);
  }

  /* Listen for new connections */
  if (listen (listen_fd, SERVER_MASTER_LISTEN_Q_LEN) < 0) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Listen on TCP master socket failed");
    pthread_exit (NULL);
  }

  while (1) {
    /* Not creating new threads to handle multiple slave servers.
    * Not that many slaves needed */
    slave_sock_len = sizeof(slave_addr);
    if ((slave_sock_fd = accept (listen_fd, (struct sockaddr *)&slave_addr,
                                &slave_sock_len)) < 0) 
    {
        LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Accept failed");
        continue;
    } 

    if (0 > (read(slave_sock_fd, read_write_buffer, MAXLINE)))
    {
        LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "read error");
        close(slave_sock_fd); 
        continue;
    }
    if (0 != strncmp(read_write_buffer, SECURITY_KEY, strlen(SECURITY_KEY)))
    {
        close(slave_sock_fd); 
        LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Security breach"); 
    }
    else
    {  
        /* Add to master_sync list of slave connections */
        master_sync->slave_sock_fd_list[master_sync->slave_count] = \
                                                    slave_sock_fd;
        master_sync->slave_count++;
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Slave connected. Slave count is %d",
                                    master_sync->slave_count);   
        server_sync_master_query_db();
    }  
  }

  pthread_exit (NULL);
}

void *server_sync_master_handle_db_timer (void *arg)
{
  long timeout_val = 0;

  if (!mainp || mainp->db_fp <= 0) {
    LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Something very wrong here. Exiting thread.");
    pthread_exit (NULL);
  }

  /* Register for the signals */
  //signal_registration();

  LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Thread to query DB every %d seconds.",
                              SERVER_MASTER_DB_QUERY_TIME);

  while (1) {
    timeout_val = time(0) + SERVER_MASTER_DB_QUERY_TIME;
    while (time (0) < timeout_val);
    if (server_sync_master_query_db() != F_SUCCESS){
      LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Stopping synchronization.");
    }
  }

  pthread_exit (NULL);
}

/********************************************************************
 * DB related functions
 *******************************************************************/

uint8_t server_sync_master_query_db()
{
    char send_buf[MAXLINE] = { 0 };
    FILE *db_fp = NULL;
    struct server_master_sync *master_syncp = NULL;
    int8_t slave_count = 0;

    if (NULL == mainp->db_fp) {
        LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "DB file pointer is NULL");
        return F_FAILURE;
    }

    db_fp = mainp->db_fp;

    LOG_SERVER_MESSAGE_TO_FILE ("INFO", "DB timer fired, quering DB");
    master_syncp = mainp->master_sync;

    /* Send the message to all slaves */
    slave_count = master_syncp->slave_count - 1;

    if (0 > slave_count) {
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "No slave servers.");
        return F_SUCCESS;
    }
  
    while (0 <= slave_count) {
        if (0 >= master_syncp->slave_sock_fd_list[slave_count]) {
            continue;
        }

        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Sending DB to client %d", slave_count);

        memset (send_buf, 0, MAXLINE);

        /* Read the file and look for SERVER_SYNC_NEW_DATA OR
        SERVER_SYNC_MODIFY_DATA. If found, send to slaves, one
        entry at a time for now. Could be changed later! */
        server_mutex_lock(&mainp->db_mutex);
        /* Read a line from the file. Lock first. */
        while (NULL != fgets(send_buf, MAXLINE, db_fp)) 
        {
            printf("%d:%s", master_syncp->slave_sock_fd_list[slave_count], send_buf);   
            if (0 > (write (master_syncp->slave_sock_fd_list[slave_count], send_buf,
                            MAXLINE)))
            {
                LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Write to client number %d failed.",
                                            slave_count);
                /* Rewind to the beginning of the file for the next iteration
                   and other operations */
                rewind(db_fp);
                master_syncp->slave_sock_fd_list[slave_count] = 0;

                break;
                 
            }
            memset (send_buf, 0, MAXLINE);
        } /* end of while */
        rewind(db_fp);
        server_mutex_unlock(&mainp->db_mutex);
        slave_count--;
    }
    return F_SUCCESS;
}
#endif /* NAME_SERVER_MASTER */

/************************* END OF FILE *************************/
