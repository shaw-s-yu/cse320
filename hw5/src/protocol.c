#include"protocol.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include "debug.h"
#include "csapp.h"
#include <unistd.h>

int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data){
debug("Send Packet fd[%d]", fd);

    XACTO_PACKET tmp = *pkt;
    tmp.size = htonl(tmp.size);
    tmp.timestamp_sec = htonl(tmp.timestamp_sec);
    tmp.timestamp_nsec = htonl(tmp.timestamp_nsec);

    if (rio_writen(fd, &tmp, sizeof(XACTO_PACKET)) != sizeof(XACTO_PACKET)){
        return -1;
    }

    if (pkt->size > 0 && data!=NULL){
        if (rio_writen(fd, data, pkt->size) <= 0 ){
            return -1;
        }
    }
    return 0;
}

int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap){
     debug("Recv Packet fd[%d]", fd);

    /*note: XACTO_PACKET struct size is 120 byte*/
    /*read header*/

    if (rio_readn(fd, pkt, sizeof(XACTO_PACKET)) == sizeof(XACTO_PACKET)){
        pkt->size = ntohl(pkt->size);
        pkt->timestamp_sec = ntohl(pkt->timestamp_sec);
        pkt->timestamp_nsec = ntohl(pkt->timestamp_nsec);
    }else{
        return -1;
    }

    if (pkt->size > 0){
        *datap = Malloc (pkt->size);
        debug("alloca space for datap , size of %u, %p", pkt->size, *datap);
        if (rio_readn(fd, *datap, pkt->size) <= 0 ){
            return -1;
        }
    }else if (datap != NULL){
        *datap = NULL;
    }
    return 0;
}
