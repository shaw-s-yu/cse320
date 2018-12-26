#include "transaction.h"
#include "debug.h"
#include "csapp.h"
/*
 * Initialize the transaction manager.
 */

sem_t sem;
unsigned int trans_count=0;
void trans_init(void){
  debug("initializing transaction manager");
  trans_list.prev=&trans_list;
  trans_list.next=&trans_list;
  Sem_init(&sem, 0, 1);
}

/*
 * Finalize the transaction manager.
 */
void trans_fini(void){
  trans_list.next = trans_list.prev;
  sem_destroy(&sem);
}
void release_dependents(TRANSACTION* tp);

/*
 * Create a new transaction.
 *
 * @return  A pointer to the new transaction (with reference count 1)
 * is returned if creation is successful, otherwise NULL is returned.
 */
TRANSACTION *trans_create(void){

  TRANSACTION* tp=Malloc(sizeof(TRANSACTION));
  memset(tp, 0, sizeof(TRANSACTION));
  Sem_init(&tp->sem, 0, 0);
  pthread_mutex_init(&tp->mutex, NULL);

  P(&sem);
  debug("create new transaction [%d]", tp->id);
  tp->id=trans_count;
  tp->next=&trans_list;
  tp->prev=trans_list.prev;

  trans_list.prev->next=tp;
  trans_list.prev=tp;
  trans_count++;
  V(&sem);

  tp=trans_ref(tp, "newly created transaction");
  return tp;
}

/*
 * Increase the reference count on a transaction.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the increase.
 * @return  The transaction pointer passed as the argument.
 */
TRANSACTION *trans_ref(TRANSACTION *tp, char *why){
  if(tp==NULL){
    debug("invalid transaction pointer in trans_ref");
    return NULL;
  }
  debug("Increase ref count on transaction %d (%d -> %d ) for %s", tp->id, tp->refcnt, tp->refcnt+1, why);
  pthread_mutex_lock(&tp->mutex);
  tp->refcnt+=1;
  pthread_mutex_unlock(&tp->mutex);
  return tp;
}

/*
 * Decrease the reference count on a transaction.
 * If the reference count reaches zero, the transaction is freed.
 *
 * @param tp  The transaction.
 * @param why  Short phrase explaining the purpose of the decrease.
 */
void trans_unref(TRANSACTION *tp, char *why){
  if(tp==NULL){
    debug("invalid transaction pointer in trans_ref");
    return;
  }
  debug("Decrease ref count on transaction %d (%d -> %d ) for %s", tp->id, tp->refcnt, tp->refcnt-1, why);

  if(tp->refcnt==0){
    debug("Transaction %d ref count would become negative", tp->id);
    abort();
  }
  pthread_mutex_lock(&tp->mutex);
  tp->refcnt-=1;
  pthread_mutex_unlock(&tp->mutex);

  if(tp->refcnt!=0)
    return;

  DEPENDENCY* dp=tp->depends;
  debug("Free transaction %d", tp->id);
  P(&sem);
  tp->prev->next=tp->next;
  tp->next->prev=tp->prev;
  V(&sem);
  pthread_mutex_destroy(&tp->mutex);
  while(dp!=NULL){
    trans_unref(dp->trans, "transaction in dependency");
    DEPENDENCY* temp=dp;
    dp=dp->next;
    Free(temp);
  }
  Free(tp);

}
/*
 * Add a transaction to the dependency set for this transaction.
 *
 * @param tp  The transaction to which the dependency is being added.
 * @param dtp  The transaction that is being added to the dependency set.
 */
void trans_add_dependency(TRANSACTION *tp, TRANSACTION *dtp){

  if(tp==NULL || dtp==NULL){
    debug("invalid pointer in trans_add_dependency");
    return;
  }

  debug("Making transaction %d depends on transaction %d", tp->id, dtp->id);

  pthread_mutex_lock(&tp->mutex);

  //create space for new dependency
  DEPENDENCY* dp_new=Malloc(sizeof(DEPENDENCY));
  memset(dp_new, 0, sizeof(DEPENDENCY));
  dp_new->trans=dtp;

  dp_new->next=tp->depends;
  tp->depends=dp_new;
  pthread_mutex_unlock(&tp->mutex);


  tp=trans_ref(dtp, "transaction in dependency");
}


// tp wait for its dependent
TRANS_STATUS wait_for_dependency(TRANSACTION* tp){

  debug("Transaction %d checking status of dependency %d", tp->id, tp->depends->trans->id);
  if(trans_get_status(tp->depends->trans)==TRANS_ABORTED){
    debug("Transaction %d has already aborted", tp->depends->trans->id);
    return TRANS_ABORTED;
  }
  else if(trans_get_status(tp->depends->trans)==TRANS_COMMITTED){
    debug("Transaction %d has already completed", tp->depends->trans->id);
    return TRANS_COMMITTED;
  }

  else{
    debug("Transaction %d waiting for dependency %d", tp->id, tp->depends->trans->id);
    pthread_mutex_lock(&tp->depends->trans->mutex);
    tp->depends->trans->waitcnt++;
    pthread_mutex_unlock(&tp->depends->trans->mutex);
    P(&tp->depends->trans->sem);
    return TRANS_COMMITTED;
  }
}



 // * Try to commit a transaction.  Committing a transaction requires waiting
 // * for all transactions in its dependency set to either commit or abort.
 // * If any transaction in the dependency set abort, then the dependent
 // * transaction must also abort.  If all transactions in the dependency set
 // * commit, then the dependent transaction may also commit.
 // *
 // * In all cases, this function consumes a single reference to the transaction
 // * object.
 // *
 // * @param tp  The transaction to be committed.
 // * @return  The final status of the transaction: either TRANS_ABORTED,
 // * or TRANS_COMMITTED.

TRANS_STATUS trans_commit(TRANSACTION *tp){

  debug("transaction %d trying to commit", tp->id);

  //no dependency
  if(tp->depends!=NULL){
    tp->status=wait_for_dependency(tp);
  }else{
    tp->status=TRANS_COMMITTED;
  }


  debug("transaction %d commits", tp->id);
  release_dependents(tp);
  //updata tp status if any one of dependents aborted
  DEPENDENCY* curr=tp->depends;
  while(curr!=NULL){
    if(curr->trans->status==TRANS_ABORTED){
      tp->status=TRANS_ABORTED;
      break;
    }
    curr=curr->next;
  }

  trans_unref(tp, "attempting to commit transaction");

  return TRANS_COMMITTED;
}

/*
 * Abort a transaction.  If the transaction has already committed, it is
 * a fatal error and the program crashes.  If the transaction has already
 * aborted, no change is made to its state.  If the transaction is pending,
 * then it is set to the aborted state, and any transactions dependent on
 * this transaction must also abort.
 *
 * In all cases, this function consumes a single reference to the transaction
 * object.
 *
 * @param tp  The transaction to be aborted.
 * @return  TRANS_ABORTED.
 */
TRANS_STATUS trans_abort(TRANSACTION *tp){
  if(tp->status==TRANS_ABORTED)
    return TRANS_ABORTED;
  if(tp->status==TRANS_COMMITTED){
    debug("fatal error on abort: transaction %d already committed", tp->id);
    abort();
  }
  else{
    tp->status=TRANS_ABORTED;
    release_dependents(tp);
    trans_unref(tp, "aborting transaction");
    return TRANS_ABORTED;
  }
}

/*
 * Get the current status of a transaction.
 * If the value returned is TRANS_PENDING, then we learn nothing,
 * because unless we are holding the transaction mutex the transaction
 * could be aborted at any time.  However, if the value returned is
 * either TRANS_COMMITTED or TRANS_ABORTED, then that value is the
 * stable final status of the transaction.
 *
 * @param tp  The transaction.
 * @return  The status of the transaction, as it was at the time of call.
 */
TRANS_STATUS trans_get_status(TRANSACTION *tp){
  if(tp==NULL){
    debug("invalid transaction pointer, cannot get status");
    return -1;
  }

  return tp->status;
}

/*
 * Print information about a transaction to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 *
 * @param tp  The transaction to be shown.
 */
void trans_show(TRANSACTION *tp){
  DEPENDENCY* dp=tp->depends;
  char* a[]={"pending  ", "committed", "aborted  "};

  fprintf(stderr, "[id=%d, status=%s, refcnt=%d, waitcnt=%d]->",
        tp->id, a[trans_get_status(tp)], tp->refcnt, tp->waitcnt);
  while(dp!=NULL){
      fprintf(stderr, "[id=%d, status=%s, refcnt=%d, waitcnt=%d]->",
        dp->trans->id, a[trans_get_status(tp)], dp->trans->refcnt, dp->trans->waitcnt);
      dp=dp->next;
  }
}

/*
 * Print information about all transactions to stderr.
 * No locking is performed, so this is not thread-safe.
 * This should only be used for debugging.
 */
void trans_show_all(void){
  fprintf(stderr, "TRANSACTIONS:\n");

  TRANSACTION* curr=trans_list.next;
  while(curr!=&trans_list){
    trans_show(curr);
    curr=curr->next;
    fprintf(stderr, "\n");
  }
}


void release_dependents(TRANSACTION* tp){
  debug("release %d waiters in transaction %d", tp->waitcnt, tp->id);
  for(;tp->waitcnt>0;tp->waitcnt--){
    V(&tp->sem);
  }
}

