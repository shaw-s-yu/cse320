#include "data.h"
#include "debug.h"
#include "csapp.h"
#include <string.h>
#include <stdio.h>
/*
 * Create a blob with given content and size.
 * The content is copied, rather than shared with the caller.
 * The returned blob has one reference, which becomes the caller's
 * responsibility.
 *
 * @param content  The content of the blob.
 * @param size  The size in bytes of the content.
 * @return  The new blob, which has reference count 1.
 */
BLOB *blob_create(char *content, size_t size){
    BLOB* blobp = Malloc(sizeof(BLOB));
    pthread_mutex_init(&blobp->mutex, NULL);
    blobp->refcnt = 0;
    blobp->size = size;

    blobp->content = Malloc(size);
    memcpy(blobp->content, content, size);

    size_t prefix_len = (size+1) >= 11 ? 11 : (size+1);
    blobp->prefix = Malloc(prefix_len);
    memcpy(blobp->prefix, content, prefix_len - 1);
    *(blobp->prefix + prefix_len - 1) = '\0';

    debug("create blob with content %p, size of %lu, -> %p", content, size, blobp);
    blob_ref(blobp, "for newly created blob");
    return blobp;
}

/*
 * Increase the reference count on a blob.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The blob pointer passed as the argument.
 */
BLOB *blob_ref(BLOB *bp, char *why){
    debug("blob ref for: %s, its prefix: %s, [%p]",why, bp->prefix, bp);
    pthread_mutex_lock(&bp->mutex);
    bp->refcnt+=1;
    pthread_mutex_unlock(&bp->mutex);
    return bp;
}

/*
 * Decrease the reference count on a blob.
 * If the reference count reaches zero, the blob is freed.
 *
 * @param bp  The blob.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void blob_unref(BLOB *bp, char *why){
    debug("blob unref for: %s, its prefix: %s, [%p]", why, bp->prefix, bp);
    pthread_mutex_lock(&bp->mutex);
    bp->refcnt-=1;
    pthread_mutex_unlock(&bp->mutex);
    if(bp->refcnt==0){
        pthread_mutex_destroy(&bp->mutex);
        Free(bp->content);
        Free(bp->prefix);
        Free(bp);
    }
}

/*
 * Compare two blobs for equality of their content.
 *
 * @param bp1  The first blob.
 * @param bp2  The second blob.
 * @return 0 if the blobs have equal content, nonzero otherwise.
 */
int blob_compare(BLOB *bp1, BLOB *bp2){
    if(bp1==NULL || bp2==NULL){
        debug("blob compare NULL pointer");
        return 1;
    }

    debug("blob compare: %p, %p", bp1, bp2);
    if(bp1->size!=bp2->size){
        return 1;
    }

    char* c1 = bp1->content;
    char* c2 = bp2->content;
    for (int i = 0; i < bp1->size; i++){
        if (*c1 != *c2)
            return 1;
        c1+=i;
        c2+=i;
    }
    return 0;
}

 // * Hash function for hashing the content of a blob.
 // *
 // * @param bp  The blob.
 // * @return  Hash of the blob.

int blob_hash(BLOB *bp){

     char *str=bp->content;
    unsigned int seed = 131;
    unsigned int hash = 0;
     int i=0;
    while (bp->size > i )
    {
        hash = hash * seed + (*str++);
        i++;
    }

    return (hash & 0x7FFFFFFF);
}

/*
 * Create a key from a blob.
 * The key inherits the caller's reference to the blob.
 *
 * @param bp  The blob.
 * @return  the newly created key.
 */
KEY *key_create(BLOB *bp){
    if(bp==NULL){
        debug("in key create, blop points to NULL");
        return NULL;
    }

    KEY* temp=Malloc(sizeof(KEY));
    temp->blob=bp;
    temp->hash=blob_hash(bp);

    debug("Create key from blob %p -> %p [%d]", bp, temp, temp->hash);

    return temp;
}

/*
 * Dispose of a key, decreasing the reference count of the contained blob.
 * A key must be disposed of only once and must not be referred to again
 * after it has been disposed.
 *
 * @param kp  The key.
 */
void key_dispose(KEY *kp){
    if(kp==NULL){
        debug("int key dispose, key points to NULL");
        return;
    }

    debug("desposing key %p, [%s]", kp, kp->blob->prefix);

    blob_unref(kp->blob, "disposing key");
    Free(kp);
}

/*
 * Compare two keys for equality.
 *
 * @param kp1  The first key.
 * @param kp2  The second key.
 * @return  0 if the keys are equal, otherwise nonzero.
 */
int key_compare(KEY *kp1, KEY *kp2){
    if(kp1==NULL || kp2==NULL){
        debug("in key compare, a key points to NULL");
        return 1;
    }

    if(kp1->hash!=kp2->hash){

        return 1;
    }

    return blob_compare(kp1->blob,kp2->blob);
}

/*
 * Create a version of a blob for a specified creator transaction.
 * The version inherits the caller's reference to the blob.
 * The reference count of the creator transaction is increased to
 * account for the reference that is stored in the version.
 *
 * @param tp  The creator transaction.
 * @param bp  The blob.
 * @return  The newly created version.
 */
VERSION *version_create(TRANSACTION *tp, BLOB *bp){
    VERSION* temp=Malloc(sizeof(VERSION));
    temp->prev=NULL;
    temp->next=NULL;
    temp->blob=bp;
    temp->creator=tp;
    debug("created version %p for transaction %d", temp, tp->id);
    trans_ref(tp, "version create");
    return temp;
}

/*
 * Dispose of a version, decreasing the reference count of the
 * creator transaction and contained blob.  A version must be
 * disposed of only once and must not be referred to again once
 * it has been disposed.
 *
 * @param vp  The version to be disposed.
 */
void version_dispose(VERSION *vp){
    if(vp->creator != NULL){
        trans_unref(vp->creator, "for version dispose");
    }
    if(vp->blob !=NULL){
        blob_unref(vp->blob, "for version dispose");
    }
    Free(vp);
}
