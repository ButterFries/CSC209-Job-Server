#ifndef __JOB_SERVER_H__
#define __JOB_SERVER_H__

#ifndef PORT
  #define PORT 55555
#endif

#ifndef MAX_JOBS
    #define MAX_JOBS 32
#endif

// No paths or lines may be larger than the BUFSIZE below
#define BUFSIZE 256

struct Client {
    int fd;
    char *buf;
    int inbuf;
};


// TODO: Add any extern variable declarations or struct declarations needed.
int parse_input(char* input, int size, int fd);

void sigint_handler(int code);

void sigchld_handler(int code);

int setup_new_client(int listen_fd, struct Client *client_fds[]);

void decline_new_client(int listen_fd);

void disconnect_user(int index);

void write_to_users(char* message);

void write_to_user(char* message, int fd);

void run_job(char **argv, int fd);

void kill_job();

void kill_all_jobs();

#endif
