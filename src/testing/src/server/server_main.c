/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_main.c
 *
 *  This file contains init functions used by server.
 *
 ***************************************************************/
#include "server_main.h"
#include "server_sync.h"
#include "../flash_main.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
extern pthread_mutex_t g_log_mutex;
int g_is_server_log_init_successful = FALSE;
char g_db_file_name[FILENAME_MAX];
extern int g_enable_debug_messages;
FILE *p_log_file_ptr;
unsigned int random_value;

/****************************************************************
 *               STATIC FUNCTION DECLARATIONS
 ***************************************************************/
#ifdef NAME_SERVER_SLAVE
static int s_file_exist(void);
#endif /* NAME_SERVER_SLAVE */

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
uint8_t server_main_init(void)
{
    mainp = malloc(sizeof(struct server_main));
    memset(mainp, 0, sizeof(struct server_main));

    memset(g_db_file_name, 0, FILENAME_MAX); 
    /* Open DB in rw mode */
#ifdef NAME_SERVER_MASTER
    memcpy(g_db_file_name, "../server/DB", strlen("../server/DB"));
    mainp->db_fp = fopen(g_db_file_name, "rw");
#elif NAME_SERVER_SLAVE
    do
    {
        random_value = rand();  
        snprintf(g_db_file_name, FILENAME_MAX, "../server/slave_DB_%u", random_value);
    } while(s_file_exist()); 
    mainp->db_fp = fopen(g_db_file_name, "a+");
    //printf("File descriptor value: %d\n", mainp->db_fp);
#endif
    if (mainp->db_fp == NULL)
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Error opening DB")
        return F_FAILURE;
    }

    if (F_SUCCESS != server_mutex_init(&mainp->db_mutex))
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Server lock failed")
        return F_FAILURE;
    }
    return F_SUCCESS;
}

void server_main_deinit(void)
{
    LOG_SERVER_MESSAGE_TO_FILE("ENTRY", "De-initializing server...");
    if (mainp != NULL)
    {
        fclose (mainp->db_fp);

#ifdef NAME_SERVER_MASTER
        if (mainp->master_sync != NULL)
        {
            server_master_sync_deinit();
        }
#endif /* NAME_SERVER_MASTER */
        free (mainp);
        mainp = NULL;
    }
}

uint8_t server_init(void)
{
    if (F_SUCCESS != server_main_init())
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Server main init failed");
        return F_FAILURE;
    }

    /* Server logging initialization must be done first */
    if (F_SUCCESS != server_logging_init())
    {
        return F_FAILURE;
    }
    g_is_server_log_init_successful = TRUE;
    
    LOG_SERVER_MESSAGE_TO_FILE("ENTRY", "Initializing the Server...");

    if (F_SUCCESS != server_query_responder_init())
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Server query responder init failed");
        return F_FAILURE;
    }

#ifdef NAME_SERVER_MASTER
    if (F_SUCCESS != server_master_sync_init())
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Server master sync init failed");
        return F_FAILURE;
    }
#endif /* NAME_SERVER_MASTER */

#ifdef NAME_SERVER_SLAVE
    /* Connect to master */
    if (F_SUCCESS != server_slave_sync_init())
    {
        LOG_SERVER_MESSAGE_TO_FILE ("ERROR", "Server slave sync init failed");
        return F_FAILURE;
    }
#endif /* NAME_SERVER_SLAVE */
     
    /* Wait for threads to join back */
    wait_for_threads_termination();
    server_main_deinit();    
  
    return F_SUCCESS;
}

int8_t server_mutex_init(pthread_mutex_t *p_mutex_id)
{
    int return_value;

    LOG_SERVER_MESSAGE_TO_FILE("ENTRY", "Initializing mutex lock .. ");

    return_value = pthread_mutex_init(p_mutex_id, NULL);
    return return_value;
}

int8_t server_mutex_lock(pthread_mutex_t *p_mutex_id)
{
    int return_value;

    return_value = pthread_mutex_lock(p_mutex_id);
    return return_value;
}

int8_t server_mutex_unlock(pthread_mutex_t *p_mutex_id)
{
    int return_value;

    return_value = pthread_mutex_unlock(p_mutex_id);
    return return_value;
}

int8_t server_mutex_destroy(pthread_mutex_t *p_mutex_id)
{
    int return_value;

    return_value = pthread_mutex_destroy(p_mutex_id);
    return return_value;
}

void signal_registration(void)
{
    /* Register signals that we expect to recieve */
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);
    signal(SIGHUP, signal_handler); 
    signal(SIGPIPE, signal_handler); 
    signal(SIGINT, signal_handler); 
    signal(SIGALRM, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);   
    signal(SIGTSTP, SIG_IGN);   
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, SIG_IGN);    
    signal(SIGTERM, SIG_DFL);   
}

void signal_handler(int sig_num)
{
    FILE *fp = fopen("Signal.txt", "a+");
    (void) sig_num;
    (void) fp;

    fprintf (fp, "Caught Signal %d", sig_num);

    server_empty_log_buffer();
    fclose (p_log_file_ptr);

    server_main_deinit(); 
    exit(EXIT_SUCCESS);
}

#ifdef NAME_SERVER_SLAVE
int server_delete_slave_database(void) 
{
    remove(g_db_file_name);
    return F_SUCCESS;
}
#endif /* NAME_SERVER_SLAVE */

void wait_for_threads_termination(void)
{
    if (0 != mainp->t_read_udp) 
    {
        pthread_join(mainp->t_read_udp, NULL);
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "UDP thread joined");
        server_empty_log_buffer();
        mainp->t_read_udp = 0;
    }

#ifdef NAME_SERVER_MASTER
    if (0 != mainp->t_read_db) 
    {
        pthread_join(mainp->t_read_db, NULL);
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "DB thread joined");
        server_empty_log_buffer();
        mainp->t_read_db = 0;
    }

    if (0 != mainp->t_tcp_listen) 
    {
        pthread_join(mainp->t_tcp_listen, NULL);
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "TCP Listen thread joined");
        server_empty_log_buffer();
        mainp->t_tcp_listen = 0;
    }
#endif /* NAME_SERVER_MASTER */

#ifdef NAME_SERVER_SLAVE
    if (0 != mainp->t_sync_update) 
    {
        pthread_join(mainp->t_sync_update, NULL);
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "TCP Sync update thread joined");
        server_empty_log_buffer();
        mainp->t_sync_update = 0;
    }
#endif /* NAME_SERVER_SLAVE */
    if (0 != mainp->t_server_log) 
    {
        pthread_join(mainp->t_server_log, NULL);
        LOG_SERVER_MESSAGE_TO_FILE ("INFO", "Logs thread joined");
        server_empty_log_buffer();
        mainp->t_server_log = 0;
    }
}

/****************************************************************
 *               STATIC FUNCTION DEFINITIONS
 ***************************************************************/
#ifdef NAME_SERVER_SLAVE
static int s_file_exist(void)
{
  struct stat buffer;
  return (0 == stat(g_db_file_name, &buffer));
}
#endif /* NAME_SERVER_SLAVE */

/************************* END OF FILE *************************/
