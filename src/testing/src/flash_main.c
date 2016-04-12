/****************************************************************
 *             ELEC-E7320 TEAM DELTA AINS PROJECT
 *
 *  Filename: flash_main.c
 *
 *  This file contains init functions used by the client
 *  and server functions.
 *
 ***************************************************************/

#include "flash_main.h"

/****************************************************************
 *    GLOBAL VARIABLE DEFINITIONS  &  MACRO DEFINITIONS
 ***************************************************************/
int g_enable_debug_messages = TRUE;

/****************************************************************
 *                   FUNCTION DEFINITIONS
 ***************************************************************/
int main()
{
#ifdef NAME_SERVER
    /* Process ID and Session ID */
    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0)
    {
      exit(EXIT_FAILURE);
    }
    /* If parent ID, then exit the parent process. */
    if (pid > 0)
    {
      exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0)
    {
        exit(EXIT_FAILURE);
    }
    printf("\nServer is running as Daemon, process id: %d\n",sid);
    signal_registration();

    /* Close out the standard file descriptors */
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    /* Daemon-specific initialization goes here */
    if (F_SUCCESS != server_init())
    {
        return F_FAILURE;
    }
#endif /* NAME_SERVER */

#ifdef CLIENT
    if (F_SUCCESS != client_init())
    {
        return F_FAILURE;
    }
#endif
    return F_SUCCESS;
}

/************************* END OF FILE *************************/
