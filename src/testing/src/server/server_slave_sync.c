/********************************************************************
 *     ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_slave_sync.c
 *
 *  This file has slave server functions implemented
 *  for a database synchronization between servers.
 *
 *******************************************************************/
#include "../flash_main.h"

#include "server_main.h"

#include "server_sync.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
extern char g_db_file_name[FILENAME_MAX];
extern pthread_mutex_t g_log_mutex;
extern int g_is_server_log_init_successful;
extern int g_enable_debug_messages;
extern unsigned int random_value;
int server_sync_slave_sock_desc;

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
#ifdef NAME_SERVER_SLAVE

int server_slave_sync_init(void)
{
    pthread_t thread_id;
    int return_value;
    char master_server_ip_address[IPV6_ADDRESS_SIZE_IN_BITS];
    unsigned int master_server_port_num;
    
    if (!mainp) 
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Mainp is NULL.");
        return F_FAILURE;
    }

    memset(master_server_ip_address, 0, IPV6_ADDRESS_SIZE_IN_BITS);
    if (OK != server_select_master(master_server_ip_address, &master_server_port_num))
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Not able to select server");
        return F_FAILURE;
    }
    
    master_server_port_num = SERVER_SYNC_PORT; 
    if (OK != server_connect_to_master(master_server_ip_address, master_server_port_num))
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Not able to connect to master server");
        return F_FAILURE;
    }
     
    /* Create thread for sync update */
    return_value = pthread_create(&thread_id, NULL, (void*)&server_sync_slave_recv_db_update, NULL);
    if (OK != return_value)
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "pthread creation failed, error code:%d\n", return_value);
        return F_FAILURE;
    }
    mainp->t_sync_update = thread_id;

    LOG_SERVER_MESSAGE_TO_FILE("INFO", "Thread with id created successfully\n");

    return F_SUCCESS;
}

void server_sync_slave_recv_db_update(void)
{
    int return_value;

    /* Register for signal handling */
    signal_registration ();

    while (1)
    {
        return_value = server_sync_slave_handle_db_update();
        if (OK != return_value)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Slave server DB update failed...Out of sync with master server\n");
            goto exit;
        }
        else
        {
            LOG_SERVER_MESSAGE_TO_FILE("INFO", "Slave DB updated successfully\n");
        }
    }
exit:
    pthread_exit(NULL);
}

int server_sync_slave_handle_db_update(void)
{
    int num_of_bytes;
    FILE *p_copy_db_file_ptr = NULL;
    char copy_db_file_name[FILENAME_MAX];
    char read_write_buffer[MAXLINE + 1];
    int return_value;
    int is_lock_acquired = 0;

    memset(copy_db_file_name, '\0', FILENAME_MAX);
    snprintf(copy_db_file_name, FILENAME_MAX, "../server/copy_of_slave_DB_%u", random_value); 
    while(1)
    { 
        memset(read_write_buffer, 0, (MAXLINE + 1));
        num_of_bytes = read(server_sync_slave_sock_desc, read_write_buffer, MAXLINE);
        if (0 < num_of_bytes)
        {
            if (0 == strncmp(read_write_buffer, "END\n", strlen("END\n"))) 
            {
                LOG_SERVER_MESSAGE_TO_FILE("INFO", "I am receiving end");
                break;
            } 
            if (!is_lock_acquired)
            {
                /* Opening file for reading */
                p_copy_db_file_ptr = fopen(copy_db_file_name, "a+");
                if(NULL == p_copy_db_file_ptr)
                {
                    goto exit;
                }
                LOG_SERVER_MESSAGE_TO_FILE("INFO", "Input file %s opened successfully\n", copy_db_file_name);
                server_mutex_lock(&mainp->db_mutex);
                is_lock_acquired = 1;
            }
            printf("%s", read_write_buffer); 
            fputs(read_write_buffer, p_copy_db_file_ptr);
            continue;
        }
        if (0 > num_of_bytes)
        {
            if (is_lock_acquired)
            {
                server_mutex_unlock(&mainp->db_mutex);
                is_lock_acquired = 0;
            } 
            LOG_PERROR_MESSAGE("read error");
            goto exit;
        }
        if (0 == num_of_bytes)
        {
            continue;
        }
    }
    
    
    if ((0 == strncmp(read_write_buffer, "END\n", strlen("END\n"))) && 
       (NULL != mainp->db_fp) && 
       (NULL != p_copy_db_file_ptr))
    {
        return_value = fclose(mainp->db_fp);
        if (OK != return_value)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Closing file %s failed", g_db_file_name);
        }
        return_value = fclose(p_copy_db_file_ptr);
        if (OK != return_value)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Closing file %s failed", copy_db_file_name);
        }
        return_value = remove(g_db_file_name);
        if (OK != return_value)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Removing file %s failed", copy_db_file_name);
        }
        return_value = rename(copy_db_file_name, g_db_file_name);
        if (OK != return_value)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "Renaming file %s to %s failed", copy_db_file_name, g_db_file_name);
        }
        /* File should be opened once again */
        mainp->db_fp = fopen(g_db_file_name, "a+");
        if(NULL == mainp->db_fp)
        {
            if (is_lock_acquired)
            {
                server_mutex_unlock(&mainp->db_mutex);
                is_lock_acquired = 0;
            } 
            goto exit;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "Input file %s opened successfully\n", g_db_file_name);
    }
    if (is_lock_acquired)
    {
        server_mutex_unlock(&mainp->db_mutex);
        is_lock_acquired = 0;
    } 
    return OK;
exit:
    pthread_exit(NULL);
}

int server_select_master(char *p_master_ip_address, unsigned int *p_master_port_num)
{
    char server_list_file_name[] = "../server_list";
    FILE *p_server_list_file_ptr;
    char read_write_buffer[MAXLINE + 1];
    char *p_temp_master_ip_address = NULL;
    char *p_temp_master_port_num = NULL;
    char *p_temp_master_priority = NULL;
    int priority;
    int default_priority = -1;
    unsigned int port_num;
    
    /* Opening file for reading */
    p_server_list_file_ptr = fopen(server_list_file_name, "r");
    if(NULL == p_server_list_file_ptr)
    {
        LOG_PERROR_MESSAGE("error in opening file");
        return NOT_OK;
    }
    LOG_SERVER_MESSAGE_TO_FILE("INFO", "Input file %s opened successfully\n", server_list_file_name);
     
    while (NULL != fgets(read_write_buffer, MAXLINE, p_server_list_file_ptr))
    {
        p_temp_master_ip_address = strtok(read_write_buffer, " ");
        p_temp_master_port_num = strtok(NULL, " ");
        p_temp_master_priority = strtok(NULL, " ");

        if ((NULL == p_temp_master_ip_address) || 
            (NULL == p_temp_master_port_num) || 
            (NULL == p_temp_master_priority))
        {
            continue;
        }   
        priority = atoi(p_temp_master_priority);
        if (default_priority < priority)
        {
            memcpy(p_master_ip_address, p_temp_master_ip_address, 
                   strlen(p_temp_master_ip_address));
            port_num = atoi(p_temp_master_port_num);
            *p_master_port_num = port_num;
            default_priority = priority; 
        } 
    }
    fclose(p_server_list_file_ptr);    
    return OK; 
}
 
int server_connect_to_master(char *p_master_server_ip_address, unsigned int master_server_port_num)
{
    int return_value;
    struct sockaddr_in6 server_ipv6_address_details;
    struct sockaddr_in server_ipv4_address_details;
    char read_write_buffer[MAXLINE + 1];

    if (IPV4_ADDRESS_MAX_LEN > strlen(p_master_server_ip_address))
    {
        /* Open a stream (TCP) IPv4 socket, and check if successful */
        server_sync_slave_sock_desc = socket(AF_INET, SOCK_STREAM, 0);
        if (server_sync_slave_sock_desc < 0)
        {
            LOG_PERROR_MESSAGE("socket error");
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "Socket with id %d is created successfully\n", server_sync_slave_sock_desc);

        memset(&server_ipv4_address_details, 0, sizeof(server_ipv4_address_details));
        server_ipv4_address_details.sin_family = AF_INET;
        server_ipv4_address_details.sin_port   = htons(master_server_port_num);
        if (inet_pton(AF_INET, p_master_server_ip_address, &server_ipv4_address_details.sin_addr) <= 0)
        {
            LOG_SERVER_MESSAGE_TO_FILE("ERROR", "inet_pton failed for %s address\n", p_master_server_ip_address);
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "IP address converted to network format successfully\n");

        /* Connect to IP address and port indicated by server_ipv4_address_details and
           check if it was succesful */
        return_value = connect(server_sync_slave_sock_desc, (struct sockaddr*) &server_ipv4_address_details,
                               sizeof(server_ipv4_address_details));
        if (OK != return_value)
        {
            LOG_PERROR_MESSAGE("connect error");
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "Connected to server address successfully\n");
    }
    else
    {
        /* Open a stream (TCP) IPv6 socket, and check if successful */
        server_sync_slave_sock_desc = socket(AF_INET6, SOCK_STREAM, 0);
        if (server_sync_slave_sock_desc < 0)
        {
            LOG_PERROR_MESSAGE("socket error");
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "Socket with id %d is created successfully\n", server_sync_slave_sock_desc);
        memset(&server_ipv6_address_details, 0, sizeof(server_ipv6_address_details));
        server_ipv6_address_details.sin6_family = AF_INET6;
        server_ipv6_address_details.sin6_port   = htons(master_server_port_num);
        if (0 >= (inet_pton(AF_INET6, p_master_server_ip_address, &server_ipv6_address_details.sin6_addr)))
        {
            LOG_PERROR_MESSAGE("inet_pton failed");
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "IP address converted to network format successfully\n");

        /* Connect to IP address and port indicated by servaddr and 
           check if it was succesful */
        return_value = connect(server_sync_slave_sock_desc, (struct sockaddr*)&server_ipv6_address_details,
                              sizeof(server_ipv6_address_details));
        if (OK != return_value)
        {
            LOG_PERROR_MESSAGE("connect error");
            return F_FAILURE;
        }
        LOG_SERVER_MESSAGE_TO_FILE("INFO", "Connected to server address successfully\n");
    }
    memset(read_write_buffer, 0, (MAXLINE + 1));
    memcpy(read_write_buffer, SECURITY_KEY, strlen(SECURITY_KEY)); 
    if (0 > (write(server_sync_slave_sock_desc, read_write_buffer, MAXLINE))) 
    {
        LOG_SERVER_MESSAGE_TO_FILE("ERROR", "write error");
        return F_FAILURE;
    }
    return OK;
} 
#endif /* NAME_SERVER_SLAVE */

/************************* END OF FILE *************************/
