/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_sync.h
 *
 *  This file contains structure definitions and function 
 *  declarations used by both master/slave servers for 
 *  synchronization.
 *
 ***************************************************************/
#ifndef _SERVER_SYNC_H_
#define _SERVER_SYNC_H_

/********************************************************************
 *                    MACRO DEFINITIONS
 *******************************************************************/
#define VERSION_IPV4                    4
#define VERSION_IPV6                    6
/* Domain name length can be upto 255 with reference to
   http://tools.ietf.org/html/rfc3986 */
#define DOMAIN_NAME_MAX_LEN             64
#define IPV4_ADDRESS_MAX_LEN            16
#define IPV6_ADDRESS_MAX_LEN            50
#define IPV4_ADDRESS_SIZE_IN_BITS       64
#define IPV6_ADDRESS_SIZE_IN_BITS       128

#define SERVER_SYNC_NO_MODIFY_DATA      0
#define SERVER_SYNC_MODIFY_DATA         1
#define SERVER_SYNC_NEW_DATA            2
#define SERVER_SYNC_DELETE_DATA         3

/* Server TCP connection Macros */
#define SERVER_SYNC_PORT                5000
#define SERVER_MASTER_LISTEN_Q_LEN      5
#define SERVER_MASTER_DB_QUERY_TIME     5 /* Seconds */
#define SLAVE_COUNT_MAX                 30

/********************************************************************
 *                  STRUCTURE DEFINITIONS
 *******************************************************************/
#ifdef NAME_SERVER_MASTER
struct server_master_sync
{
  /* TCP server socket_fd */
  int tcp_listen_fd;

  /* List of client socket descriptors */
  int slave_sock_fd_list[SLAVE_COUNT_MAX];
  uint8_t slave_count;

  /* File descriptor for DB */
  FILE *db_fp;
};
#endif /* NAME_SERVER_MASTER */

/********************************************************************
 *          FUNCTIONS DECLARATIONS
 *******************************************************************/
#ifdef NAME_SERVER_MASTER

uint8_t server_master_sync_init ();
void server_master_sync_deinit ();
void server_master_sync_global_init ();
uint8_t server_sync_master_db_init ();
uint8_t server_master_sync_sock_init ();
void *server_master_sync_listen (void *arg);
void *server_sync_master_handle_db_timer (void *arg);
uint8_t server_sync_master_query_db ();
#endif /* NAME_SERVER_MASTER */

#ifdef NAME_SERVER_SLAVE
int server_slave_sync_init(void);
void server_sync_slave_recv_db_update(void);
int server_sync_slave_handle_db_update(void);
int server_connect_to_master(char *p_master_ip_address, unsigned int master_server_port_num);
#endif /* NAME_SERVER_SLAVE */

#endif /* _SERVER_SYNC_H_ */

/************************* END OF FILE *************************/
