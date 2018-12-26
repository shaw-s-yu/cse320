#include "store.h"
#include "debug.h"
#include "csapp.h"
#include "pthread.h"




void store_init(void){
    debug("Initializing object store");
    the_map.num_buckets=0;
    the_map.table=Malloc(sizeof(void*)*NUM_BUCKETS);
    memset(the_map.table, 0 , (sizeof(void*) * NUM_BUCKETS));
    the_map.num_buckets=NUM_BUCKETS;
    pthread_mutex_init(&the_map.mutex, NULL);

}

/*
 * Finalize the store.
 */
void store_fini(void){
    debug("Finalizing object stroe");
    for(int i=0; i<the_map.num_buckets;i++){
        if(the_map.table[i]!=NULL){
            MAP_ENTRY* curr=the_map.table[i];
            MAP_ENTRY* to_free=curr;
            while(curr->next!=NULL){
                curr=curr->next;
                Free(to_free);
            }
        }
    }
    pthread_mutex_destroy(&the_map.mutex);
}

int add_version(MAP_ENTRY* entry, TRANSACTION* tp, BLOB* blob);
MAP_ENTRY* find_map_entry(KEY* key);

/*
 * Put a key/value mapping in the store.  The key must not be NULL.
 * The value may be NULL, in which case this operation amounts to
 * deleting any existing mapping for the given key.
 *
 * This operation inherits the key and consumes one reference on
 * the value.
 *
 * @param tp  The transaction in which the operation is being performed.
 * @param key  The key.
 * @param value  The value.
 * @return  Updated status of the transation, either TRANS_PENDING,
 *   or TRANS_ABORTED.  The purpose is to be able to avoid doing further
 *   operations in an already aborted transaction.
 */
TRANS_STATUS store_put(TRANSACTION *tp, KEY *key, BLOB *value){
    debug("Put mapping (key=%p [%s] -> value=%p [%s]) in store for transaction %d",
        key, key->blob->prefix, value, value->prefix, tp->id);

    MAP_ENTRY* entry=find_map_entry(key);
    if(add_version(entry, tp, value)!=1){
        debug("add version failed");
    }

    return trans_get_status(tp);
}

/*
 * Get the value associated with a specified key.  A pointer to the
 * associated value is stored in the specified variable.
 *
 * This operation inherits the key.  The caller is responsible for
 * one reference on any returned value.
 *
 * @param tp  The transaction in which the operation is being performed.
 * @param key  The key.
 * @param valuep  A variable into which a returned value pointer may be
 *   stored.  The value pointer store may be NULL, indicating that there
 *   is no value currently associated in the store with the specified key.
 * @return  Updated status of the transation, either TRANS_PENDING,
 *   or TRANS_ABORTED.  The purpose is to be able to avoid doing further
 *   operations in an already aborted transaction.
 */
TRANS_STATUS store_get(TRANSACTION *tp, KEY *key, BLOB **valuep){
    debug("Get mapping of key=%p [%s] in store for transaction %d", key, key->blob->prefix, tp->id);
    MAP_ENTRY* entry=find_map_entry(key);
    if(add_version(entry, tp, *valuep)!=1){
        return TRANS_ABORTED;
    }
    debug("get from store");
    VERSION* curr=entry->versions;
    *valuep=curr->prev->blob;
    if(*valuep!=NULL)
        blob_ref(*valuep, "returnning from store");

    return trans_get_status(tp);
}

/*
 * Print the contents of the store to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void store_show(void){
    fprintf(stderr, "CONTENTS OF STORE:\n");
    char* status[]={"pending  ","committed","aborted  "};
    for(int i=0; i<NUM_BUCKETS; i++){
        MAP_ENTRY* curr=the_map.table[i];
        fprintf(stderr, "%d:", i);
        int t=0;
        while(curr!=NULL){
            if(t)   fprintf(stderr, "  ");


            fprintf(stderr, "\t{key:%p [%s], versions:",
                curr->key, curr->key->blob->prefix);

            VERSION* currv=curr->versions;
            do{

                fprintf(stderr, "{creator:%d ,(%s), ",
                    currv->creator->id, status[currv->creator->status]);
                if(currv->blob){
                    fprintf(stderr, "blob=%p ,[%s]}", currv->blob, currv->blob->prefix);
                }
                else
                    fprintf(stderr, "(NULL blob)}");
                currv=currv->next;
            }while(currv!=curr->versions);


            fprintf(stderr, "}");
            t=1;
            curr=curr->next;
        }
        fprintf(stderr, "\n");
    }
}

MAP_ENTRY* find_map_entry(KEY* key){

    int index=key->hash%NUM_BUCKETS;
    MAP_ENTRY* curr=the_map.table[index];
    while(curr!=NULL){
        if(key_compare(key, curr->key)==0){
            debug("Matching entry exists, disposing of redundant key %p [%s]", key, key->blob->prefix);
            key_dispose(key);
            return curr;
        }
        curr=curr->next;
    }

    debug("Create new map entry for key %p [%s] at table index %d", key, key->blob->prefix, index);
    pthread_mutex_lock(&the_map.mutex);
    MAP_ENTRY* new_entry=Malloc(sizeof(MAP_ENTRY));
    memset(new_entry, 0, sizeof(MAP_ENTRY));
    new_entry->key=key;
    new_entry->next=the_map.table[index];
    the_map.table[index]=new_entry;

    pthread_mutex_unlock(&the_map.mutex);
    return new_entry;
}

// return 0 if a version created is needed
// return 1 if a version edit is needed
int add_version(MAP_ENTRY* entry, TRANSACTION* tp, BLOB* blob){
    debug("Trying to put version in map entry for key %p [%s]",
        entry->key, entry->key->blob->prefix);

    VERSION* curr=entry->versions;



    //garbage collecting
    if(entry->versions!=NULL){
        do{

            debug("Examine version %p for key %p [%s]", curr, entry->key, entry->key->blob->prefix);
            //check if current is aborted
            if(trans_get_status(curr->creator)==TRANS_ABORTED){
                debug("Aborted version encountered (creator=%d), aborting subsequent versions",
                     curr->creator->id);

                //go through subsequence, remove already aborted, abort others
                do
                {
                    //if already aborted, remove the version
                    if(trans_get_status(curr->creator)==TRANS_ABORTED){
                        VERSION* to_dispose=curr;
                        trans_ref(to_dispose->creator, "reference to creator for aborting");

                        //if this is the head, head will be next
                        //if next is self, head will be null
                        if(curr==curr->next){
                            debug("remove the entry version %d because the entry version has only one aborted version %p, %d",
                                curr->creator->id, curr->blob, curr->blob->refcnt);
                            version_dispose(to_dispose);
                            pthread_mutex_lock(&the_map.mutex);
                            entry->versions=NULL;
                            pthread_mutex_unlock(&the_map.mutex);
                            break;
                        }else if(entry->versions==to_dispose){
                            debug("remove the aborted entry head version %d and then abort others in next %p, %d",
                                curr->creator->id, curr->blob, curr->blob->refcnt);
                            pthread_mutex_lock(&the_map.mutex);
                            curr=curr->next;
                            entry->versions=curr;
                            to_dispose->prev->next=curr;
                            curr->prev=to_dispose->prev;
                            pthread_mutex_unlock(&the_map.mutex);
                            version_dispose(to_dispose);
                            continue;
                        }else{
                            debug("remove an aborted version(not entry head) %d and then abort others in next",
                                curr->creator->id);
                            pthread_mutex_lock(&the_map.mutex);
                            curr=curr->next;
                            to_dispose->prev->next=curr;
                            curr->prev=to_dispose->prev;
                            pthread_mutex_unlock(&the_map.mutex);
                            version_dispose(to_dispose);
                            continue;
                        }

                    }else{
                        debug("aborting version with creator %d", curr->creator->id);
                        trans_abort(curr->creator);
                    }
                } while(1);
                debug("aborting done so no need to check committed version");
                break;
            }

            //check if commited
            if(trans_get_status(curr->creator)==TRANS_COMMITTED){
                //check if previous is committed, remove previous
                if(trans_get_status(curr->prev->creator)==TRANS_COMMITTED){
                    debug("Removing old committed version (creator=%d)", curr->prev->creator->id);

                    pthread_mutex_lock(&the_map.mutex);

                    curr->prev=curr->prev->prev;
                    version_dispose(entry->versions);
                    entry->versions=curr;
                    curr->prev->next=curr;

                    pthread_mutex_unlock(&the_map.mutex);
                }
            }
            curr=curr->next;
        }while(curr!=entry->versions);

    }

    VERSION* newv=version_create(tp, blob);

    //for an empty version entry
    if(entry->versions==NULL){
        debug("Add new version for key %p [%s]", entry->key, entry->key->blob->prefix);
        debug("not previous version");
        pthread_mutex_lock(&the_map.mutex);
        entry->versions=newv;
        newv->next=newv;
        newv->prev=newv;
        pthread_mutex_unlock(&the_map.mutex);
        return 1;
    }

    //for a not empty version entry
    int curr_id=tp->id;
    int creator_id=entry->versions->prev->creator->id;
    //check if there is a match version (same creator)
    //if current tp id is lower than its biggest, abort
    if(curr_id<creator_id){
        debug("Current transaction ID (%d) is less than version creator (%d) -- aborting",
            curr_id, creator_id);
        trans_abort(tp);
        version_dispose(newv);
        return 0;
    }

    //if current tp id is equal to its biggest, replace
    else if(curr_id==creator_id){
        debug("Replace existing version for key %p [%s]", entry->key, entry->key->blob->prefix);
        debug("tp=%p(%d), creator=%p(%d)", tp, tp->id, curr->prev->creator, creator_id);
        if(newv->blob==NULL){
            version_dispose(newv);
            return 1;
        }

        VERSION* to_dispose=curr;
        if(entry->versions->creator->id==creator_id){
            pthread_mutex_lock(&the_map.mutex);
            entry->versions=newv;
            newv->prev=newv;
            newv->next=newv;
            pthread_mutex_unlock(&the_map.mutex);
            version_dispose(to_dispose);
            return 1;
        }

        pthread_mutex_lock(&the_map.mutex);
        newv->next=curr->next;
        newv->prev=curr->prev;
        curr->prev->next=newv;
        curr->next->prev=newv;
        pthread_mutex_unlock(&the_map.mutex);
        version_dispose(to_dispose);
        return 1;
    }

    //if current tp id is greater than biggest, add new
    else{

        //go through all creator to make dependency
        curr=entry->versions;
        do{
            trans_add_dependency(tp, curr->creator);
            curr=curr->next;
        }while(curr!=entry->versions);


        debug("Add new version for key %p [%s]", entry->key, entry->key->blob->prefix);
        debug("Previous version is %p [%d]", curr->prev, curr->prev->creator->id);

        if(newv->blob==NULL){
            newv->blob=curr->prev->blob;
            blob_ref(newv->blob, "add new version in the end of version list");
        }

        pthread_mutex_lock(&the_map.mutex);

        newv->prev=curr->prev;
        newv->next=curr;

        curr->prev=newv;
        newv->prev->next=newv;

        pthread_mutex_unlock(&the_map.mutex);

        return 1;
    }
}
