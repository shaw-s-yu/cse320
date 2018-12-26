#include "debug.h"
#include "client_registry.h"
#include "transaction.h"
#include "store.h"
#include "server.h"
#include "protocol.h"
#include "data.h"
#include "csapp.h"
#include <signal.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include<netinet/in.h>
#include<sys/types.h>

#define MAXSIZE 100

static void terminate(int status);
int get_port(char* argv);
void sig_handler_t(int sig);
CLIENT_REGISTRY *client_registry;
char* port=NULL;
char* host;
char buff[MAXSIZE];

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    struct sigaction s = {
        .sa_handler = sig_handler_t,
    };
    sigaction(SIGHUP, &s, 0);


    char optval;
    if (argc <= 1){
        fprintf(stderr, "Usage: %s [-p <port_number>]\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    while(optind < argc) {
        if((optval = getopt(argc, argv, "qp:h:")) != -1) {
            switch(optval) {
                case 'p':
                    if(!optarg){
                        fprintf(stderr, "Invalid port number\n");
                        exit(EXIT_FAILURE);
                    }
                    port=optarg;
                    break;
                case 'h':
                    host=strdup(optarg);

                    break;
                case '?':
                fprintf(stderr, "Usage: %s [-p <port_num>] [-h <host_name>] [-q]\n", argv[0]);
                exit(EXIT_FAILURE);
                break;
                default:

                break;
            }
       }
    }

    // Perform required initializations of the client_registry,
    // transaction manager, and object store.

    client_registry = creg_init();
    trans_init();
    store_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.


    //install a signal handler

    int listenfd, *connfdp;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd=Open_listenfd(port);

    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept (listenfd, (SA *) &clientaddr, &clientlen);

        Pthread_create(&tid, NULL, xacto_client_service, connfdp);
    }

    fprintf(stderr, "You have to finish implementing main() "
	    "before the Xacto server will function.\n");

    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    trans_fini();
    store_fini();

    debug("Xacto server terminating");
    exit(status);
}



void sig_handler_t(int sig){
    terminate(0);
}