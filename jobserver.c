#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <ctype.h>
#include <errno.h>
#include <sys/wait.h>

#include "socket.h"
#include "jobserver.h"

#define QUEUE_LENGTH 5
#define MAX_CLIENTS 20

#ifndef JOBS_DIR
    #define JOBS_DIR "jobs/"
#endif


// Global list of clients
struct Client *client_fds = NULL;

// Flag to keep track of SIGINT received
int sigint_received;

// Global list of jobs
int* job_pids = NULL;
int** job_fds = NULL; //0 is read, 1 is write
int** job_fds_err = NULL; //0 is read, 1 is write
int* job_read = NULL;
int* job_kill = NULL;
int* job_called_by = NULL;

char** job_buffer = NULL;

int max_fd = 0, active_fds = 0;

fd_set all_fds, listen_fds;

int main(void) {
    
    job_pids = calloc(MAX_JOBS, sizeof(int));
    job_read = calloc(MAX_JOBS, sizeof(int));
    job_kill = calloc(MAX_JOBS, sizeof(int));
    job_called_by = calloc(MAX_JOBS, sizeof(int));
    
    job_fds = malloc(sizeof(int*)*MAX_JOBS);
    job_fds_err = malloc(sizeof(int*)*MAX_JOBS);
    
    for (int i = 0; i < MAX_JOBS; i++) job_fds[i] = calloc(2, sizeof(int));
    for (int i = 0; i < MAX_JOBS; i++) job_fds_err[i] = calloc(2, sizeof(int));
    
    //client_fds = calloc(MAX_CLIENTS, sizeof(struct Client));
    client_fds = malloc(MAX_CLIENTS * sizeof(struct Client));
    
    for (int i = 0; i < MAX_CLIENTS; i++) {
        client_fds[i].buf = malloc(sizeof(char)*BUFSIZE);
        client_fds[i].fd = 0;
        client_fds[i].inbuf = 0;
    }
    
    // Reset SIGINT received flag.
    sigint_received = 0;
    
    // This line causes stdout and stderr not to be buffered.
    // Don't change this! Necessary for autotesting.
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    
    
    // Set up SIGCHLD handler
    struct sigaction sigchldact;
    sigchldact.sa_handler = sigchld_handler;
    sigchldact.sa_flags = 0;
    sigemptyset(&sigchldact.sa_mask);
    
    if(sigaction(SIGCHLD, &sigchldact, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    // Set up SIGINT handler
    struct sigaction sigintact;
    sigintact.sa_handler = sigint_handler;
    sigintact.sa_flags = 0;
    sigemptyset(&sigintact.sa_mask);
    
    if(sigaction(SIGINT, &sigintact, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    // TODO: Set up server socket
    struct sockaddr_in *peer = init_server_addr(PORT);
    int sock = setup_server_socket(peer, QUEUE_LENGTH);
    
    // TODO: Initialize client tracking structure (array list)
    
    
    // TODO: Initialize job tracking structure (linked list)
    
    
    // TODO: Set up fd set(s) that we want to pass to select()
    FD_ZERO(&all_fds);
    
    FD_SET(sock, &all_fds);
    
    
    /* Here is a snippet of code to create the name of an executable
     * to execute:
     *
     * char exe_file[BUFSIZE];
     * snprintf(exe_file, BUFSIZE, "%s/%s", JOBS_DIR, <job_name>);
     */
    
    //sock is already active
    active_fds = 1;
    max_fd = sock;
    
    
    while (!sigint_received) {
        
        kill_job();
        
    	// Use select to wait on fds, also perform any necessary checks 
    	// for errors or received signals
        listen_fds = all_fds;
        
        int nready = select(max_fd + 1, &listen_fds, NULL, NULL, NULL);
        
        if (nready == -1) {
            if (errno != 4) {
                perror("server: select");
                exit(1);
            }
            else {
                continue;
            }
        }
        
        // Is it the original socket? Create a new connection ...
        if (FD_ISSET(sock, &listen_fds)) {
            int client_fd = setup_new_client(sock, &client_fds);
            
            // full
            if (client_fd < 0) {
                continue;
            }
            
            FD_SET(client_fd, &all_fds);
            active_fds+=1;
            if(client_fd > max_fd)
                max_fd = client_fd;
            
            printf("Accepted connection\n");
        }
        
        
        // Accept incoming connections
        for (int i = 0; i < MAX_CLIENTS; i++) {
            
            if (client_fds[i].fd > 0 && FD_ISSET(client_fds[i].fd, &listen_fds)) {
                
                char *startbuf = client_fds[i].buf + client_fds[i].inbuf;
                int room = BUFSIZE - client_fds[i].inbuf - 1;
                int crlf;
                char *tok, *cr, *lf;
                
                if (room <= 1) { //buffer full
                    continue;
                }
                int len = read(client_fds[i].fd, startbuf, room-1);
                if(len <= 0) {
                    disconnect_user(i);
                }
                
                client_fds[i].inbuf += len;
                client_fds[i].buf[client_fds[i].inbuf] = '\0';
                lf = strchr(client_fds[i].buf, '\n');
                cr = strchr(client_fds[i].buf, '\r');
                
                if (!lf && !cr)
                    continue; //No complete line
                
                tok = strtok(client_fds[i].buf, "\r\n");
                
                int rtn = 0;
                if (tok) {
                    
                    printf("[CLIENT %d] %s\n", client_fds[i].fd, tok);
                    
                    int size = 0;
                    for (int i = 0; tok[i] != '\0'; i++)
                        size++;
                    rtn = parse_input(tok, size, client_fds[i].fd);
                    if (rtn < 0) {
                        char send[BUFSIZE];
                        sprintf(send, "[SERVER] Invalid command: %s\n", tok);
                        printf("%s", send);

                        //write_to_users(send);
                        write_to_user(send, client_fds[i].fd);
                        
                        
                    }
    
                }
                
                if (client_fds[i].fd == 0)
                    continue;
                
                if (!lf)
                    crlf = cr - client_fds[i].buf;
                else if (!cr)
                    crlf = lf - client_fds[i].buf;
                else
                    crlf = (lf > cr) ? lf - client_fds[i].buf : cr - client_fds[i].buf;
                crlf++;
                
                client_fds[i].inbuf -= crlf;
                
                memmove(client_fds[i].buf, client_fds[i].buf + crlf, client_fds[i].inbuf);
                
                if(rtn == 1) { //Job ran
                    continue;
                }
            }
        }
        // Check our job pipes, update max_fd if we got children
        for (int i = 0; i < MAX_JOBS; i++) {

            if (FD_ISSET(job_fds[i][0], &listen_fds)) { //read data from jobs
                
                char temp[BUFSIZE-24], send[BUFSIZE];
                
                
                read(job_fds[i][0], temp, BUFSIZE-25);
                
                int location = -1;
                
                for (int i = 0; i < BUFSIZE-26; i++) {
                    if (temp[i] == '\n') {
                        location = i;
                        temp[i] = '\0';
                        temp[i+1] = '\n';
                        temp[i+2] = '\r';
                    }
                }
                
                if (location < 0) {
                    int temp = job_pids[i];
                    if (temp != 0 && job_read[i] == 0) {
                        
                        sprintf(send, "*(SERVER)* Buffer from job %d is full. Aborting job.\n", temp);
                        printf("%s", send);
                        
                        //write_to_users(send);
                        write_to_user(send, job_called_by[i]);
                        
                        job_read[i] = 1;
                        //job_kill[i] = 1;
                    }
                    
                } else if (job_read[i] == 0) {
                    
                    sprintf(send, "[JOB %d] %s\n", job_pids[i], temp);
                    printf("%s", send);
                    
                    //write_to_users(send);
                    write_to_user(send, job_called_by[i]);
                    
                    job_read[i] = 1;
                    //job_kill[i] = 1;
                }
                
                
            }
        }
        for (int i = 0; i < MAX_JOBS; i++) {
            
            if (FD_ISSET(job_fds_err[i][0], &listen_fds)) { //read data from jobs
                
                char temp[BUFSIZE-24], send[BUFSIZE];
                
                
                read(job_fds_err[i][0], temp, BUFSIZE-25);
                
                
                int location = -1;
                
                for (int i = 0; i < BUFSIZE-26; i++) {
                    if (temp[i] == '\n') {
                        location = i;
                        temp[i] = '\0';
                    }
                }
                
                if (location < 0) {
                    int temp = job_pids[i];
                    if (temp != 0 && job_read[i] == 0) {
                        sprintf(send, "*(SERVER)* Buffer from job %d is full. Aborting job.\n", temp);
                        
                        //write_to_users(send);
                        write_to_user(send, job_called_by[i]);
                        
                        job_read[i] = 1;
                        //job_kill[i] = 1;
                    }
                    
                } else if (job_read[i] == 0) {
                    sprintf(send, "*(JOB %d)* %s\n", job_pids[i], temp);
                    printf("%s", send);
                    
                    //write_to_users(send);
                    write_to_user(send, job_called_by[i]);
                    
                    job_read[i] = 1;
                    //job_kill[i] = 1;
                }
                
                
            }
        }
        // Check on all the connected clients, process any requests
	    // or deal with any dead connections etc.
        
        
    }
    
    write_to_users("[SERVER] Shutting down\n");
    printf("[SERVER] Shutting down\n");
    
    for (int i = 0; i < MAX_CLIENTS && client_fds[i].fd != 0; i++) {
        close(client_fds[i].fd);
    }
    
    kill_all_jobs();
    
    free(peer);
    
    return 0;
}

int parse_input(char* input, int size, int fd) {
    // jobs, run jobname [arg], kill pid, **watch pid, exit
    char* firstSpace = strchr(input, ' ');
    char first_temp[BUFSIZE];
    char* first;
    int length = size;
    
    if (firstSpace != NULL) { // Additional arguments
        length = firstSpace - input;
        for (int i = 0; i < length; i++)
            first_temp[i] = input[i];
        first_temp[length] = '\0';
        first = first_temp;
    }
    else // No additional arugments
        first = input;
    
    if (strncmp(first, "jobs", length) == 0) {
        //check nothing else after
        
        if (length != size)
            return -1;
        
        char temp[BUFSIZE-10], send[BUFSIZE];
        
        int index = 0;
        for (int i = 0; i < MAX_JOBS && job_pids[i] > 0; i++)
            index += snprintf(&temp[index], (BUFSIZE-10)-index, "%d ", job_pids[i]);
        
        if (index > 0) { 
            temp[index-1] = '\0';
            sprintf(send, "[SERVER] %s\n", temp);
        }
        else {
            strncpy(send, "[SERVER] No currently running jobs\n", strlen("[SERVER] No currently running jobs\n")+1);
        }
        //write_to_users(send);
        write_to_user(send, fd);
        
        printf("%s", send);
        
    }
    else if (strncmp(first, "run", length) == 0) {
        //check if other input exists, and parse it
        int argc = 1; //extra one for NULL, but one less for run
        for (int i = 0; i < strlen(input); i++) {
            if (input[i] == ' ')
                argc++;
        }
        char* argv[argc+1];
        char* temp = strtok(input, " ");
        int index = 0;
        
        while (temp != NULL) {
            argv[index++] = temp;
            temp = strtok(NULL, " ");
        }
        
        if (index <= 1) {
            char* send = "[SERVER] Invalid argument\n";
            printf("%s", send);
            
            //write_to_users(send);
            write_to_user(send, fd);
            
            return -2;
        }
        
        argv[argc] = NULL;
        
        run_job(&argv[1], fd); //first argument is run, need to ignore
        return 1;
    }
    
    else if (strncmp(first, "kill", length) == 0) {
        //check if pid is after, and only pid
        int digit = 0;
        
        if (length == size) {
            return -1;
        }
        
        char num[BUFSIZE];
        int count = 0;
        
        for (int i = length+1; i < size; i++) {
            
            digit = isdigit(input[i]);
            if (digit == 0) {
                return -1;
            }
            num[count] = input[i];
            count++;
        }
        num[count] = '\0';
        int int_num = strtol(num, NULL, 10);
        int found = 0;
        
        if (int_num > 0) {
            
            for (int i = 0; i < MAX_JOBS; i++) {
            
                if (job_pids[i] == int_num) {
                    
                    close(job_fds[i][0]);
                    
                    FD_CLR(job_fds[i][0], &all_fds);
                    FD_CLR(job_fds_err[i][0], &all_fds);
                    
                    kill(job_pids[i], SIGTERM);
                    
                    job_pids[i] = 0;
                    job_fds[i][0] = 0;
                    job_fds[i][1] = 0;
                    
                    found = 1;
                    
                }
                else if (found) {
                    job_pids[i-1] = job_pids[i];
                    job_fds[i-1][0] = job_fds[i][0];
                    job_fds[i-1][1] = job_fds[i][1];
                    job_fds_err[i-1][0] = job_fds_err[i][0];
                    job_fds_err[i-1][1] = job_fds_err[i][1];
                    
                    job_pids[i] = 0;
                    job_fds[i][0] = 0;
                    job_fds[i][1] = 0;
                    job_fds_err[i][0] = 0;
                    job_fds_err[i][1] = 0;
                }
                
            }
        }
        if (found) return 0;
        
        char send[BUFSIZE];
        sprintf(send, "[SERVER] Job %d not found\n", int_num);
        
        //write_to_users(send);
        write_to_user(send, fd);
        
        printf("%s", send);
    }
    
    return 0;
}

/* SIGINT handler:
 * We are just raising the sigint_received flag here. Our program will
 * periodically check to see if this flag has been raised, and any necessary
 * work will be done in main() and/or the helper functions. Write your signal 
 * handlers with care, to avoid issues with async-signal-safety.
 */
void sigint_handler(int code) { // sigint closes the program, need to do cleanup in main if triggered
    sigint_received = 1;
}

void kill_all_jobs() {

    for (int i = 0; i < MAX_JOBS; i++) {
        
        if (job_pids[i] == 0)
            return;
        
        FD_CLR(job_fds[i][0], &all_fds);
        FD_CLR(job_fds_err[i][0], &all_fds);
        
        close(job_fds[i][0]);
        close(job_fds_err[i][0]);
        
        FD_CLR(job_fds[i][0], &all_fds);
        FD_CLR(job_fds_err[i][0], &all_fds);
        
        kill(job_pids[i], SIGTERM);
            
        job_pids[i] = 0;
    }
}

void kill_job() {
    int i;
    for (i = 0; i < MAX_JOBS; i++) {
        
        if (job_read[i] == 1 && job_kill[i] == 1) {
            
            FD_CLR(job_fds[i][0], &all_fds);
            FD_CLR(job_fds_err[i][0], &all_fds);
            
            close(job_fds[i][0]);
            close(job_fds_err[i][0]);
            
            FD_CLR(job_fds[i][0], &all_fds);
            FD_CLR(job_fds_err[i][0], &all_fds);
            
            kill(job_pids[i], SIGTERM);
                
            job_pids[i] = 0;
            job_called_by[i] = 0;
            
            for (int j = i+1; j < MAX_JOBS; j++) {
                
                job_pids[j-1] = job_pids[j];
                job_fds[j-1][0] = job_fds[j][0];
                job_fds[j-1][1] = job_fds[j][1];
                job_fds_err[j-1][0] = job_fds_err[j][0];
                job_fds_err[j-1][1] = job_fds_err[j][1];
                
                job_called_by[j-1] = job_called_by[j];
                
                job_read[j-1] = job_read[j];
                job_kill[j-1] = job_kill[j];
                
                job_pids[j] = 0;
                job_fds[j][0] = 0;
                job_fds[j][1] = 0;
                job_fds_err[j][0] = 0;
                job_fds_err[j][1] = 0;
                
                job_called_by[j] = 0;
                
                job_read[j] = 0;
                job_kill[j] = 0;
            }
            i--;
        }
    }
    
}

// SIGCHLD (child stopped or terminated) handler: mark jobs as dead
void sigchld_handler(int code) {
    
    pid_t pid;
    int status;
    int saved_errno = errno;
    while ((pid = waitpid(-1, &status, WNOHANG)) != -1) {
        
        for (int i = 0; i < MAX_JOBS; i++) {
            
            if (job_pids[i] == pid && pid != 0) {
                job_kill[i] = 1;
            }
        }
    }
    errno = saved_errno;
    
}


void run_job(char **argv, int fd) {
    
    int free_index = -1;
    
    for (int i = 0; i < MAX_JOBS; i++) {
        
        if (job_pids[i] == 0 && job_fds[i][0] == 0) {
            free_index = i;
            break;
        }
    }
    
    if (free_index < 0) {
        char* send = "[SERVER] MAXJOBS exceeded\n";
        
        //write_to_users(send);
        write_to_user(send, fd);
        
        printf("%s", send);
        return;
    }
    
    
    
    
    if (pipe(job_fds[free_index]) == -1) {
        perror("pipe");
        exit(1);
    }
    if (pipe(job_fds_err[free_index]) == -1) {
        perror("pipe");
        exit(1);
    }
    
    int result = fork();
    
    if (result == 0) { //child
        
        close(job_fds[free_index][0]); // Won't be reading
        close(job_fds_err[free_index][0]);
        
        dup2(job_fds[free_index][1], STDOUT_FILENO);
        dup2(job_fds_err[free_index][1], STDERR_FILENO);
        
        close(job_fds[free_index][1]); // Won't be writing anymore
        close(job_fds_err[free_index][1]);
        
        char exe_file[BUFSIZE];
        snprintf(exe_file, BUFSIZE, "%s/%s", JOBS_DIR, argv[0]);
        
        
        execv(exe_file, argv); //First argument is the name of the file, last one is NULL
        perror("execv");
        exit(1);
    }
    
    close(job_fds[free_index][1]); // Won't be writing
    close(job_fds_err[free_index][1]);
    
    FD_SET(job_fds[free_index][0], &all_fds);
    
    if(job_fds[free_index][0] > max_fd)
        max_fd = job_fds[free_index][0];
    
    FD_SET(job_fds_err[free_index][0], &all_fds);
    
    if(job_fds_err[free_index][0] > max_fd)
        max_fd = job_fds_err[free_index][0];
    
    job_pids[free_index] = result;
    
    job_called_by[free_index] = fd;
    
    return;
}



/*
 *  Client management
 */

/* Accept a connection and adds them to list of clients.
 * Return the new client's file descriptor or -1 on error.
 */
int setup_new_client(int listen_fd, struct Client *client_fds[]) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if ((*client_fds)[i].fd <= 0) {
            
            int client_fd = accept_connection(listen_fd);
            if (client_fd < 0) {
                return -1;
            }
            
            (*client_fds)[i].fd = client_fd;
            (*client_fds)[i].inbuf = 0;
            
            for (int j = 0; j < BUFSIZE; j++) {
                (*client_fds)[i].buf[j] = '\0';
            }
            
            return client_fd;
        }
    }
    decline_new_client(listen_fd);
    return -1;
}

void decline_new_client(int listen_fd) {
    int client_fd = accept_connection(listen_fd);
    char message[] = "[SERVER] Client disconnected: max concurrent connections reached\r\n";
    
    write(client_fd, message, strlen(message));
    printf("%s", message);

    close(client_fd);
}



void write_to_users(char* message) {
    char print_message[BUFSIZE];
    
    int i = 0;
    for (i = 0; message[i] != '\0' && i < BUFSIZE-3; i++) {
        print_message[i] = message[i];
    }
    
    print_message[i-1] = '\r';
    print_message[i] = '\n';
    
    for (int j = 0; j < MAX_CLIENTS; j++) {
        
        if (client_fds[j].fd <= 0)
            return;
        
        if (write(client_fds[j].fd, print_message, i+1) != i+1) {
            disconnect_user(j);
        }
        
    }
    
}

void write_to_user(char* message, int fd) {
    
    char print_message[BUFSIZE];
    
    int i = 0;
    for (i = 0; message[i] != '\0' && i < BUFSIZE-3; i++) {
        print_message[i] = message[i];
    }
    
    print_message[i-1] = '\r';
    print_message[i] = '\n';
    
    if (fd <= 0)
        return;
    
    if (write(fd, print_message, i+1) != i+1) {
        
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (client_fds[i].fd == fd) {
                disconnect_user(i);
                return;
            }
        }
    }
    
}

void disconnect_user(int index) {
    printf("[CLIENT %d] Connection closed\n", client_fds[index].fd);
    FD_CLR(client_fds[index].fd, &all_fds);
    FD_CLR(client_fds[index].fd, &listen_fds);
    
    close(client_fds[index].fd);
    client_fds[index].fd = 0;
    client_fds[index].inbuf = 0;
}



