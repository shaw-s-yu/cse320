#include <pthread.h>
#include "client_registry.h"
#include "debug.h"
#include "csapp.h"

#define MAX 1024
sem_t mutex, w;
int creg_count=0;
struct client_registry{
    int fd;
    struct client_registry* next;
};

/*
 * Initialize a new client registry.
 *
 * @return  the newly initialized client registry.
 */
CLIENT_REGISTRY *creg_init(){
    debug("initializing client registry");
    Sem_init(&mutex, 0 ,1);
    Sem_init(&w, 0, 1);

    CLIENT_REGISTRY* temp=Malloc(sizeof(CLIENT_REGISTRY));
    temp->next=temp;
    return temp;
}

/*
 * Finalize a client registry.
 *
 * @param cr  The client registry to be finalized, which must not
 * be referenced again.
 */
void creg_fini(CLIENT_REGISTRY *cr){
    Free(cr);
}

/*
 * Register a client file descriptor.
 *
 * @param cr  The client registry.
 * @param fd  The file descriptor to be registered.
 */
void creg_register(CLIENT_REGISTRY *cr, int fd){

    debug("registering %d fd", fd);

    if(creg_count>=MAX){
        shutdown(fd, SHUT_RD);
        debug("creg_count exceeds MAX");
        return;
    }


    P(&mutex);

    //add a new client into the last of list
    CLIENT_REGISTRY* temp=Malloc(sizeof(CLIENT_REGISTRY));
    temp->fd=fd;
    temp->next=cr;

    CLIENT_REGISTRY* curr=cr;
    while(curr->next!=cr){
        curr=curr->next;
    }

    curr->next=temp;
    creg_count++;
    V(&mutex);
    debug("registered %d fd", fd);

}


 // * Unregister a client file descriptor, alerting anybody waiting
 // * for the registered set to become empty.
 // *
 // * @param cr  The client registry.
 // * @param fd  The file descriptor to be unregistered.

void creg_unregister(CLIENT_REGISTRY *cr, int fd){
    P(&mutex);

    CLIENT_REGISTRY* curr=cr;

    while(curr->next!= cr){
        if(curr->next->fd==fd)
            break;
        curr=curr->next;
    }

    if(curr->next==cr){
        debug("didn't find fd: %d",fd);
        V(&mutex);
        return;
    }

    CLIENT_REGISTRY* temp=curr->next;
    curr->next=curr->next->next;
    Free(temp);
    creg_count--;

    if(creg_count==0)
        V(&w);

    V(&mutex);
    debug("unregistered %d fd", fd);

}

/*
 * A thread calling this function will block in the call until
 * the number of registered clients has reached zero, at which
 * point the function will return.
 *
 * @param cr  The client registry.
 */
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    P(&w);
    if(cr->next==cr)
        debug("now empty, cnt: %d", creg_count);
    V(&w);
    return;
}

/*
 * Shut down all the currently registered client file descriptors.
 *
 * @param cr  The client registry.
 */
void creg_shutdown_all(CLIENT_REGISTRY *cr){

    P(&w);
    CLIENT_REGISTRY* curr=cr->next;
    while(curr!=cr){
        debug("shutdown client connection: fd[%d]", curr->fd);
        shutdown(curr->fd, SHUT_RD);

        curr = curr -> next;
    }
}
