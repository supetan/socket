/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: server_logging.c
 *
 *  This file contains logging functions used by the master/slave 
 *  servers.
 *
 ***************************************************************/

#include "../flash_main.h"

#include "server_main.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
#define SERVER_NO_OF_LOGS 100
#define MAX_LIMIT_FOR_SERVER_LOGS (2048 * 1024)
#define MAX_NO_OF_LINES_PER_LOG_FILE 850
#define SERVER_LOGS_OVERFLOW_THRESHOLD_VALUE 0.9
#define SERVER_LOG_TIMER_VALUE 3 

int g_max_log_size_in_bytes;
int g_current_log_size_in_bytes = 0;
char g_server_logs_path[FILENAME_MAX];
extern int g_enable_debug_messages;
int current_no_of_server_logs = 0;
int no_of_files = 0;
file_names *p_file_names_list; 
char *p_log_messages_list[SERVER_NO_OF_LOGS];
int max_no_of_server_logs = SERVER_NO_OF_LOGS;
pthread_mutex_t g_log_mutex; 
extern FILE *p_log_file_ptr;
extern unsigned int random_value;

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
int server_logging_init(void)
{
#ifdef NAME_SERVER_MASTER 
    int return_value;
#endif /* NAME_SERVER_MASTER */
    g_max_log_size_in_bytes = MAX_LIMIT_FOR_SERVER_LOGS;

    /* Get the current working directory */         
    if (NULL == getcwd(g_server_logs_path, sizeof(g_server_logs_path))) 
    {
       return F_FAILURE;  
    }

#ifdef NAME_SERVER_MASTER 
    /* Add new directory to existing path */
    if (NULL == strncat(g_server_logs_path, "/master_logs", strlen("/master_logs"))) 
    {
       return F_FAILURE;  
    }
#elif NAME_SERVER_SLAVE
    /* Add new directory to existing path */
    if (NULL == strncat(g_server_logs_path, "/slave_logs", strlen("/slave_logs"))) 
    {
       return F_FAILURE;  
    }
#endif
    
#ifdef NAME_SERVER_MASTER 
    /* Remove directory if it already exists */
    return_value = server_delete_logs(g_server_logs_path);
    if (F_SUCCESS != return_value)
    {
       LOG_MESSAGE(ERROR, stdout, "Server logging is collapsed");
    }
#endif /* NAME_SERVER_MASTER */
   
#ifdef NAME_SERVER_MASTER 
    /* Create a new "logs" directory */
    mkdir("master_logs", 0777);  
#elif NAME_SERVER_SLAVE
    /* Create a new "logs" directory */
    mkdir("slave_logs", 0777);  
#endif
    
    if (F_SUCCESS != server_mutex_init(&g_log_mutex))
    {
        LOG_MESSAGE(ERROR, stdout, "Server lock failed.")
        return F_SUCCESS;
    }
    if (F_SUCCESS != server_logging_thread_create())
    {
        LOG_MESSAGE(ERROR, stdout, "Server logging thread creation failed.")
        return F_SUCCESS;
    }
    return F_SUCCESS; 
}

void server_move_logs_to_file(void)
{
    server_empty_log_buffer();
}

void server_empty_log_buffer(void)
{
    static char file_name[FILENAME_MAX];
    static int files_present = 0;
    int size;                                          
    struct stat st; 
    unsigned int index;
    static int no_of_lines = 0; 
        
    /* Copy logs to file */ 
    if (g_current_log_size_in_bytes >= (g_max_log_size_in_bytes * SERVER_LOGS_OVERFLOW_THRESHOLD_VALUE)) 
    {
        /* Delete oldest file */
        memset(file_name, 0, FILENAME_MAX);
        remove_files_from_list(file_name);
        
        /* Add log size */  
        stat(file_name, &st);                                    
        size = st.st_size;                                 
        g_current_log_size_in_bytes = g_current_log_size_in_bytes - size;     
        remove(file_name); 
    } 
    
    if (0 == no_of_lines)
    {    
        /* Open file to write logs */
        memset(file_name, 0, FILENAME_MAX);
#ifdef NAME_SERVER_MASTER 
        snprintf(file_name, FILENAME_MAX, "master_logs/logs_%u.txt", ++files_present); 
#elif NAME_SERVER_SLAVE 
        snprintf(file_name, FILENAME_MAX, "slave_logs/logs_%u_%u.txt", ++files_present, random_value); 
#endif
        add_files_to_list(file_name); 
        p_log_file_ptr = fopen(file_name, "w");
        if (NULL == p_log_file_ptr)
        {
            return;  
        }
    } 
        
    /* Copy everything from ring buffer to new file */
    for (index = 0; index < current_no_of_server_logs; index++)
    {
        fwrite(p_log_messages_list[index], 1, strlen(p_log_messages_list[index]), p_log_file_ptr); 
        free(p_log_messages_list[index]);
        no_of_lines++; 
    }
    current_no_of_server_logs = 0;
 
    /* MAX_NO_OF_LINES_PER_LOG_FILE lines approximately creates 75kb to 110 kb file size */ 
    if (MAX_NO_OF_LINES_PER_LOG_FILE <= no_of_lines)
    {
        /* Close file */
        fclose(p_log_file_ptr);  
      
        /* Add log size */  
        stat(file_name, &st);                                    
        size = st.st_size;                                 
        g_current_log_size_in_bytes = size + g_current_log_size_in_bytes;     
        no_of_lines = 0; 
    }
}

void add_files_to_list(char *p_file_name)
{
    file_names *p_temp;
    file_names *p_new_node;

    p_new_node = (file_names*)malloc(sizeof(file_names));
    if (NULL == p_new_node) 
    {
        return;
    }   
    p_new_node->file_name = malloc(sizeof(char) * (strlen(p_file_name) + 1));
    if (NULL == p_new_node->file_name) 
    {
        return;
    }
    memset(p_new_node->file_name, 0, (strlen(p_file_name)));
    memcpy(p_new_node->file_name, p_file_name, strlen(p_file_name));
    p_new_node->next = NULL;
    p_temp = p_file_names_list;
    if (NULL == p_temp)
    {
        p_file_names_list = p_new_node;      
        return;
    } 
    while (NULL != p_temp->next)
    {
        p_temp = p_temp->next;
    }
    p_temp->next = p_new_node;
    return;
}

void remove_files_from_list(char *file_name)
{
    file_names *p_temp;
    file_names *p_temp_2;

    p_temp_2 = p_temp = p_file_names_list;

    memcpy(file_name, p_temp->file_name, strlen(p_temp->file_name));
    p_temp = p_temp->next;
    free(p_temp_2->file_name);
    free(p_temp_2);
    p_file_names_list = p_temp;
}

void add_log_message_to_buffer(char *p_temp_buffer)
{
    if (current_no_of_server_logs >= max_no_of_server_logs)
    {
       server_move_logs_to_file();
    }
    p_log_messages_list[current_no_of_server_logs] = malloc((strlen(p_temp_buffer) + 1) * sizeof(char));
    memset(p_log_messages_list[current_no_of_server_logs], '\0', (strlen(p_temp_buffer) + 1)); 
    memcpy(p_log_messages_list[current_no_of_server_logs], p_temp_buffer, strlen(p_temp_buffer));
    p_log_messages_list[current_no_of_server_logs][strlen(p_temp_buffer)] = '\n';
    current_no_of_server_logs++;
}

void timer_handler(int signal_num)
{
    server_mutex_lock(&g_log_mutex);
    server_move_logs_to_file();
    server_mutex_unlock(&g_log_mutex);
}

void server_logging_timer_init(void)
{
    struct sigaction sa;
    struct itimerval timer;

    /* Install timer_handler as the signal handler for SIGVTALRM. */
    memset (&sa, 0, sizeof(sa));
    sa.sa_handler = &timer_handler;
    sigaction(SIGVTALRM, &sa, NULL);

    /* Configure the timer to expire after SERVER_LOG_TIMER_VALUE sec... */
    timer.it_value.tv_sec = SERVER_LOG_TIMER_VALUE;
    timer.it_value.tv_usec = 0;

    /* ... and every SERVER_LOG_TIMER_VALUE after that. */
    timer.it_interval.tv_sec = SERVER_LOG_TIMER_VALUE;
    timer.it_interval.tv_usec = 0;

    /* Start a virtual timer. It counts down whenever this process is
     * executing. */
    setitimer(ITIMER_VIRTUAL, &timer, NULL);

    /* Do busy work. */
    while (1);
}

int server_logging_thread_create(void)
{
    pthread_t thread_id;
    int return_value;

    /* Create thread for server logging */
    return_value = pthread_create(&thread_id, NULL, (void*)&server_logging_timer_init, NULL);
    if (OK != return_value)
    {
        LOG_MESSAGE(ERROR, stderr, "pthread creation failed, error code:%d\n", return_value);
        return F_FAILURE;
    }
    mainp->t_server_log = thread_id; 
    return F_SUCCESS;
}

int server_delete_logs(char *p_directory_name) 
{
    DIR *p_directory_ptr;
    struct dirent *p_directory;
    char file_name[FILENAME_MAX];

    p_directory_ptr = opendir(p_directory_name);
    if (NULL != p_directory_ptr)
    {  
        while ((p_directory = (readdir(p_directory_ptr)))) 
        {
            struct stat stat_file_info;

            snprintf(file_name, FILENAME_MAX, "%s/%s", p_directory_name, p_directory->d_name);
            if (lstat(file_name, &stat_file_info) < 0)
            {
                LOG_PERROR_MESSAGE(file_name);
            }

            if (S_ISDIR(stat_file_info.st_mode)) 
            {
                if (strcmp(p_directory->d_name, ".") && 
                    strcmp(p_directory->d_name, "..")) 
                {
                    server_delete_logs(file_name);
                }
            } 
            else 
            {
                remove(file_name);
            }
        }
        closedir(p_directory_ptr);
    }
    else
    { 
        return F_SUCCESS;
    }
    remove(p_directory_name);
    return F_SUCCESS;
}

/************************* END OF FILE *************************/
