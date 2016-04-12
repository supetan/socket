/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_main.h
 *
 *  This file contains structure definitions and function 
 *  declarations used by both master/slave servers.
 *
 ***************************************************************/
#ifndef _SERVER_MAIN_H_
#define _SERVER_MAIN_H_

#include <stdio.h>
#include <stdint.h>
#include <sys/socket.h>  /* defines socket, connect, ... */
#include <netinet/in.h>  /* defines sockaddr_in */
#include <pthread.h>
#include <arpa/inet.h>   /* inet_pton, ... */
#include <sys/select.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/time.h>
#include <dirent.h>
#include <libgen.h>
#include <time.h>

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
struct server_main
{
  /* DB File pointer. Open once for everyone */
  FILE *db_fp;

  /* Mutex lock for DB */
  pthread_mutex_t db_mutex;

  /* UDP socket descriptor */
  int udp_sock_fd;

#ifdef NAME_SERVER_MASTER
  struct server_master_sync *master_sync;
#endif /* NAME_SERVER_MASTER */

  /* UDP thread */
  pthread_t t_read_udp;
#ifdef NAME_SERVER_MASTER
  pthread_t t_read_db;
  pthread_t t_tcp_listen;
#endif /* NAME_SERVER_MASTER */
  
  /* Log thread */
  pthread_t t_server_log;
#ifdef NAME_SERVER_SLAVE
  pthread_t t_sync_update;
#endif /* NAME_SERVER_SLAVE */
};

/* server file name structure */
typedef struct file_names
{
    char *file_name;
    struct file_names *next;
}file_names;

struct server_main *mainp;

void add_log_message_to_buffer(char *p_temp_buffer);
uint8_t server_main_init(void);
void server_main_deinit(void);
uint8_t server_query_responder_init(void);
int server_logging_init(void);
int server_logging_thread_create(void);
int server_delete_logs(char *p_directory_name); 
int server_select_master(char *p_master_ip_address, unsigned int *p_master_port_num); 
void timer_handler(int signum);
void server_logging_timer_init(void);
int8_t server_mutex_init(pthread_mutex_t *pMutexId);
int8_t server_mutex_lock(pthread_mutex_t *pMutexId);
int8_t server_mutex_unlock(pthread_mutex_t *pMutexId);
int8_t server_mutex_destroy(pthread_mutex_t *pMutexId);
void add_files_to_list(char *p_file_name);
void remove_files_from_list(char *file_name);
void server_empty_log_buffer(void);
void server_move_logs_to_file(void);
int server_delete_slave_database(void); 
void wait_for_threads_termination(void);
void *server_query_responder_handle_request(void *arg);

#endif /* _SERVER_MAIN_H_ */

/************************* END OF FILE *************************/
