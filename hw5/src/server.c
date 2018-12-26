#include "server.h"
#include "client_registry.h"
#include "csapp.h"
#include "debug.h"
#include "transaction.h"
#include "protocol.h"
#include "data.h"
#include "store.h"
#include <time.h>

 // * Thread function for the thread that handles client requests.
 // *
 // * @param  Pointer to a variable that holds the file descriptor for
 // * the client connection.  This pointer must be freed once the file
 // * descriptor has been retrieved.

CLIENT_REGISTRY *client_registry;

int get_pkg(int connfd, TRANSACTION* tp);
int put_pkg(int connfd, TRANSACTION* tp);
int end_trans(int connfd, TRANS_STATUS t);


void *xacto_client_service(void *arg){

    int connfd=*((int*)arg);
    debug("the connfd is %d", connfd);
    Free(arg);
    Pthread_detach(Pthread_self());
    creg_register(client_registry,connfd);
    TRANSACTION* tp=trans_create();
    TRANS_STATUS t=TRANS_PENDING;

    XACTO_PACKET* pkg=Malloc(sizeof(XACTO_PACKET));

    while(proto_recv_packet(connfd, pkg, NULL)==0){

        if(pkg->type==XACTO_GET_PKT){
            t=get_pkg(connfd, tp);
        }

        else if(pkg->type==XACTO_PUT_PKT){
            t=put_pkg(connfd, tp)!=0;
        }

        else if(pkg->type==XACTO_COMMIT_PKT){
            t=trans_commit(tp);
        }

        if(t==TRANS_COMMITTED || t==TRANS_ABORTED){
            if(end_trans(connfd, t)!=0)
                debug("end client failed");
        }


        store_show();

        trans_show_all();
    }

    creg_unregister(client_registry, connfd);
    Free(pkg);
    return NULL;
}

int put_pkg(int connfd, TRANSACTION* tp){

    char* data=NULL;
    debug("now put");
    XACTO_PACKET pkg;
    memset(&pkg, 0, sizeof(XACTO_PACKET));


    //receive the key
    if(proto_recv_packet(connfd, &pkg, (void**)&data)){
        if(data!=NULL)
            Free(data);
        debug("put receive key failed");
        return TRANS_ABORTED;
    }


    BLOB* bp_key=blob_create(data, pkg.size);
    if(data!=NULL) Free(data);
    KEY* k=key_create(bp_key);
    debug("receive key packet fd[%d] [%s]", connfd, bp_key->prefix );

    //receive the value
    memset(&pkg, 0, sizeof(XACTO_PACKET));
    if(proto_recv_packet(connfd, &pkg, (void**)&data)){
        if(data!=NULL)
            Free(data);
        debug("put receive value failed");
        return TRANS_ABORTED;
    }
    BLOB* bp_value=blob_create(data, pkg.size);
    if(data!=NULL) Free(data);
    debug("receive value packet fd[%d] [%s]", connfd, bp_value->prefix );

    if(store_put(tp, k, bp_value)!=TRANS_PENDING){
        debug("store put failed due to transaction abort or commit");
        return TRANS_ABORTED;
    }

    //initialize the reply pkt
    struct timespec current;
    if(clock_gettime(CLOCK_MONOTONIC, &current)!=0){
        debug("get time failed");
        return TRANS_ABORTED;
    }

    memset(&pkg, 0, sizeof(XACTO_PACKET));
    pkg.type=XACTO_REPLY_PKT;
    pkg.size=0;
    pkg.null=0;
    pkg.status=0;
    pkg.timestamp_sec=current.tv_sec;
    pkg.timestamp_nsec=current.tv_nsec;

    if(proto_send_packet(connfd, &pkg, NULL)){
        debug("send packet failed");
        return TRANS_ABORTED;
    }
    return 0;
}

int get_pkg(int connfd, TRANSACTION* tp){
    char* data=NULL;
    debug("now get");
    XACTO_PACKET pkg;
    memset(&pkg, 0, sizeof(XACTO_PACKET));


    //receive the key
    if(proto_recv_packet(connfd, &pkg, (void**)&data)){
        if(data!=NULL)
            Free(data);
        debug("get receive key failed");
        return TRANS_ABORTED;
    }

    BLOB* bp_key=blob_create(data,pkg.size);
    debug("receive key packet fd[%d] [%s]", connfd, bp_key->prefix );
    if(data!=NULL) Free(data);
    BLOB* bp_value=NULL;
    KEY* key=key_create(bp_key);

    if(store_get(tp, key, &bp_value)==TRANS_PENDING){

        //send reply packet
        memset(&pkg, 0, sizeof(XACTO_PACKET));
        pkg.type=XACTO_REPLY_PKT;
        struct timespec current;
        if(clock_gettime(CLOCK_MONOTONIC, &current)!=0){
            debug("get time failed");
            return TRANS_ABORTED;
        }
        pkg.timestamp_sec=current.tv_sec;
        pkg.timestamp_nsec=current.tv_nsec;
        if(proto_send_packet(connfd, &pkg, NULL)){
            debug("send get reply pkg failed");
            return TRANS_ABORTED;
        }

        //send value packet);
        memset(&pkg, 0, sizeof(XACTO_PACKET));
        pkg.type=XACTO_DATA_PKT;
        if(clock_gettime(CLOCK_MONOTONIC, &current)!=0){
            debug("get time failed");
            return TRANS_ABORTED;
        }
        pkg.timestamp_sec=current.tv_sec;
        pkg.timestamp_nsec=current.tv_nsec;
        if(bp_value==NULL){
            debug("value is NULL");
            pkg.null=1;

            if(proto_send_packet(connfd, &pkg, NULL)){
                debug("get send value failed");
                return TRANS_ABORTED;
            }
        }else{
            pkg.size=bp_value->size;
            debug("send value %s", bp_value->prefix);

            if(proto_send_packet(connfd, &pkg, bp_value->content)){
                debug("get send value failed");
                return TRANS_ABORTED;
            }

            blob_unref(bp_value, "get from store");
        }

    }else{
        return TRANS_ABORTED;
    }



    return TRANS_PENDING;
}

int end_trans(int connfd, TRANS_STATUS t){

    XACTO_PACKET pkg;
    memset(&pkg, 0, sizeof(pkg));
    struct timespec current;
    if(clock_gettime(CLOCK_MONOTONIC, &current)!=0){
        debug("get time failed");
        return TRANS_ABORTED;
    }
    pkg.timestamp_sec=current.tv_sec;
    pkg.timestamp_nsec=current.tv_nsec;
    pkg.type=XACTO_REPLY_PKT;
    pkg.status=t;

    debug("trans_status: %d", t);
    proto_send_packet(connfd, &pkg, NULL);

    shutdown(connfd, SHUT_RD);

    return TRANS_PENDING;
}
