/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: flash_main.h
 *
 *  This file has macros and enums used by the client
 *  and server functions.
 *
 ***************************************************************/
#ifndef _FLASH_MAIN_H_
#define _FLASH_MAIN_H_

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <signal.h>

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
enum
{
   INFO = 0,
   ERROR = 1
} debugLevel;

/*****
 * Error Codes
 * If negative values are used, please change uint8_t in the
 * return codes
 *****/
#define OK                      0
#define NOT_OK                  1
#define FALSE                   0
#define TRUE                    1
#define MAXLINE               256

#define F_FAILURE               1
#define F_SUCCESS               0
#define F_NO_SLAVE_SERVERS      2

/* Security key to avoid malicious incoming connections from other hosts */
#define SECURITY_KEY "flash" 
#define UDP_PORT_NUMBER 6543

#define NUM_OF_SERVERS 3
#define IPV4_STRING_SIZE 15
#define SERVER_LIST_PATH "../server_list"

/* Example:
LOG_MESSAGE (INFO, stdout, "Print statement with variables",
             variable1, variable2);
LOG_MESSAGE (ERROR, stderr, "Print statement with variables",
             variable1, variable2);
*/

/* Macro to print debug messages */
#define LOG_MESSAGE(priority, stream, msg, ...)             \
do                                                          \
{                                                           \
    char *str;                                              \
    if (priority == INFO)                                   \
    {                                                       \
        if (g_enable_debug_messages)                        \
        {                                                   \
            str = "INFO";                                   \
            fprintf(stream, "[%s]:%s:%s:%d:"msg" \n",       \
      str, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);\
        }                                                   \
    }                                                       \
    else if (priority == ERROR)                             \
    {                                                       \
        str = "ERR";                                        \
        fprintf(stream, "[%s]:%s:%s:%d:"msg" \n",           \
    str, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__);  \
    }                                                       \
}while(0);

/* Macro to print error messages */
#define LOG_PERROR_MESSAGE(msg)                         \
do                                                      \
{                                                       \
    perror(#msg);                                       \
}while(0);

#define LOG_SERVER_MESSAGE_TO_FILE(log_level, msg, ...)              \
do                                                                   \
{                                                                    \
    char temp_buffer[256];                                           \
    time_t my_time;                                                  \
    char *p_time;                                                    \
    my_time = time(NULL);                                            \
    p_time = ctime(&my_time);                                        \
    p_time[(strlen(p_time)) - 1]  = '\0';                            \
    memset(temp_buffer, 0, 256);                                     \
    if (g_is_server_log_init_successful)                             \
    {                                                                \
        if (0 != (strncmp("ENTRY", log_level, strlen("ENTRY"))))     \
        {                                                            \
            snprintf(temp_buffer, 256, "%s:%s:%s:%s:%d:"msg"",       \
                     log_level, p_time, __FILE__, __FUNCTION__,      \
                     __LINE__, ##__VA_ARGS__);                       \
            server_mutex_lock(&g_log_mutex);                         \
            add_log_message_to_buffer(temp_buffer);                  \
            server_mutex_unlock(&g_log_mutex);                       \
        }                                                            \
        else if (g_enable_debug_messages)                            \
        {                                                            \
            snprintf(temp_buffer, 256, "%s:%s:%s:%s:%d:"msg"",       \
                     log_level, p_time, __FILE__, __FUNCTION__,      \
                     __LINE__, ##__VA_ARGS__);                       \
            server_mutex_lock(&g_log_mutex);                         \
            add_log_message_to_buffer(temp_buffer);                  \
            server_mutex_unlock(&g_log_mutex);                       \
        }                                                            \
    }                                                                \
} while(0);
   
uint8_t client_init(void);    
uint8_t server_init(void);
void signal_registration(void);
void signal_handler(int sig_num);
 
#endif /* _FLASH_MAIN_H_ */

/************************* END OF FILE *************************/
