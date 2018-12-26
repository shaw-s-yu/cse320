/**
 * All functions you make for the assignment must be implemented in this file.
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

#define WSIZE 8 //Word and header/footer size (bytes)
#define PAGE 4096 // page size (bytes)


void* change_prev_allocated(void* temp, int prev_allocated);
void* sf_init_alblock(int requested,int total, sf_header* temp_header);
void* sf_init_freeblock(int total, sf_header* temp_header);
sf_free_list_node* sf_get_list(int size);
void* sf_coalesce(sf_header* temp_header);
void* find_fit(int required, int size);
void* sf_add_free_block(sf_free_list_node* node, sf_header* temp_header);
void* sf_delete_free_block(sf_free_list_node* node, sf_header* temp_header);

void *sf_malloc(size_t size) {
    if(size==0)
        return NULL;

    //initiallize useful variables
    int total = size + WSIZE;
    int padding = 0;
    sf_header* temp_header;
    sf_prologue* temp_prologue;
    sf_epilogue* temp_epilogue;

    //Account for padding and total size of the block if not divisible by 16
    if(total%16!=0){
        padding = total % 16;
        padding = 16 - padding;
        total += padding;
    }
    //Account if it is divisible by 16
    else{
        padding = 0;
    }

    //Account for padding and total size of the block to store footer,
    //prev and next link for freelist when freed
    if(total<32){
        total+=16;
        padding+=16;
    }


    //get enough page for the first time call of sf_malloc()
    if(sf_mem_start()==sf_mem_end()){

        //add one page first and initialize
        if(sf_mem_grow()==NULL){
            sf_errno=ENOMEM;
            return NULL;
        }

        //initialize heap
        temp_prologue=sf_mem_start();
        temp_prologue->header.info.allocated=1;
        temp_prologue->footer.info.allocated=1;
        temp_epilogue=sf_mem_end()-sizeof(sf_epilogue);
        temp_epilogue->footer.info.allocated=1;

        //initialize free block
        int block_size=PAGE-(int)sizeof(sf_prologue)-(int)sizeof(sf_epilogue);
        temp_header=(void*)temp_prologue+sizeof(sf_prologue);
        temp_header=sf_init_freeblock(block_size,temp_header);
        temp_header=change_prev_allocated(temp_header,1);


        // add free block to free list;
        sf_free_list_node* temp_node=sf_add_free_list(block_size, &sf_free_list_head);
        temp_header=(sf_header*)sf_add_free_block(temp_node,temp_header);

    }

    //search for free list
    //if not find any block, add pages
    while((temp_header=(sf_header*)find_fit(size,total))==NULL){

        //create one more page
        if((temp_epilogue=sf_mem_grow())==NULL){
            sf_errno=ENOMEM;
            return NULL;
        }

        //initialize heap and free block

        temp_header=(sf_header*)((void*)temp_epilogue-WSIZE);
        temp_epilogue=sf_mem_end()-sizeof(sf_epilogue);
        temp_epilogue->footer.info.allocated=1;
        sf_init_freeblock(PAGE,temp_header);


        // add free block to free list;
        sf_free_list_node* temp_node=sf_get_list(PAGE);
        if(temp_node->size!=PAGE)
            temp_node=sf_add_free_list(PAGE, temp_node);
        temp_header=sf_add_free_block(temp_node,temp_header);

        //coalesce the new page block with previous page free block
        temp_header=sf_coalesce(temp_header);


    }
    //block found

    //check if the free block before epilogue has size <32


    return &temp_header->payload;
}

void sf_free(void *pp) {

    //if null pointer is entered
    if(pp==NULL){
        abort();
    }


    //check pointer position
    if(pp<sf_mem_start()+sizeof(sf_prologue) || pp>sf_mem_end()-sizeof(sf_epilogue))
        abort();


    sf_header* temp_header=pp-WSIZE;
    //check if the block is allocated
    if(temp_header->info.allocated==0)
        abort();
    // sf_footer* temp_footer=(sf_footer*)((void*)temp_header+(temp_header->info.block_size<<4)-WSIZE);
    // if(temp_footer->info.allocated==0)
    //     abort();
    //check if block size good
    if((temp_header->info.block_size<<4)%16!=0 || temp_header->info.block_size<<4<32)
        abort();

    //check if required size good
    if(temp_header->info.requested_size+WSIZE>(temp_header->info.block_size<<4))
        abort();

    //check previous block is free if pre_allocated=0
    if(temp_header->info.prev_allocated==0){
        sf_footer* temp_footer=(sf_footer*)((void*)temp_header-sizeof(sf_footer));
        if(temp_footer->info.allocated!=0)
            abort();
    }

    //reset the block to free block and add to list
    temp_header=sf_init_freeblock(temp_header->info.block_size<<4,temp_header);
    sf_free_list_node* temp_node=sf_get_list(temp_header->info.block_size<<4);
    if(temp_node->size!=temp_header->info.block_size<<4)
        temp_node=sf_add_free_list(temp_header->info.block_size<<4,temp_node);
    temp_header=sf_add_free_block(temp_node,temp_header);

    //coalesce with adjacent blocks
    temp_header=sf_coalesce(temp_header);

    return;
}

void *sf_realloc(void *pp, size_t rsize) {

    //if null pointer is entered
    if(pp==NULL){
        sf_errno=EINVAL;
        return NULL;
    }


    //check pointer position
    if(pp<sf_mem_start()+sizeof(sf_prologue) || pp>sf_mem_end()-sizeof(sf_epilogue)){
        sf_errno=EINVAL;
        return NULL;
    }

    sf_header* temp_header=pp-WSIZE;
    //check if the block is allocated
    if(temp_header->info.allocated==0){
        sf_errno=EINVAL;
        return NULL;
    }
    // sf_footer* temp_footer=(sf_footer*)((void*)temp_header+(temp_header->info.block_size<<4)-WSIZE);
    // if(temp_footer->info.allocated==0)
    //     abort();
    //check if block size good
    if((temp_header->info.block_size<<4)%16!=0 || temp_header->info.block_size<<4<32){
        sf_errno=EINVAL;
        return NULL;
    }

    //check if required size good
    if(temp_header->info.requested_size+WSIZE>(temp_header->info.block_size<<4)){
        sf_errno=EINVAL;
        return NULL;
    }

    //check previous block is free if pre_allocated=0
    if(temp_header->info.prev_allocated==0){
        sf_footer* temp_footer=(sf_footer*)((void*)temp_header-sizeof(sf_footer));
        if(temp_footer->info.allocated!=0){
            sf_errno=EINVAL;
            return NULL;
        }
    }

    if(rsize==0){
        sf_free(pp);
        return NULL;
    }

    //the old block size
    int size=(temp_header->info.block_size<<4);

    //rsize is larger than the block size
    if(rsize>size-WSIZE){


        void* new_ptr=sf_malloc(rsize);
        if(new_ptr==NULL)
            return NULL;


        new_ptr=memcpy(new_ptr,pp,size);


        //sf_show_heap();
        sf_free(pp);
        return new_ptr;
    }

    //rsize is smaller than the block size
    if(rsize<size){

        int padding;
        int total=rsize+WSIZE;  //the new block size

        //account for block size, if not divisible by 16, make it be
        if(total%16!=0){
            padding = total % 16;
            padding = 16 - padding;
            total += padding;
        }

        //if divisible by 16, no padding
        else
            padding = 0;

        //make sure minima block size is 32
        if(total<32){
            total+=16;
            padding+=16;
        }

        int padding_size=size-total;

        //get the next block header
        //sf_header* next_header=(void*)temp_header+size;

        //if the splinter is large enough to be a free block, split it,
        //and coalesce with adjacent free blocks
        if(padding_size>=32){

             //initialize the splinter to a new free block
            sf_header* padding_header=pp-WSIZE+total;
            padding_header=sf_init_freeblock(padding_size,padding_header);

            //add the free block(made from padding) to list
            sf_free_list_node* temp_node=sf_get_list(padding_size);
            if(temp_node->size!=padding_size)
                temp_node=sf_add_free_list(padding_size,temp_node);
            padding_header=sf_add_free_block(temp_node,padding_header);

            //coalecse the free block
            padding_header=sf_coalesce(padding_header);

            //initialize the new required block
            temp_header=sf_init_alblock(rsize,total,temp_header);
        }

        // //if next block is free block, and the splinter size is 16,
        // //then merge the block with the next free block
        // else if(padding_size==16 && next_header->info.allocated==0){

        //     //delete the next free block from its list
        //     sf_free_list_node* temp_node=sf_get_list(padding_size);
        //     next_header=sf_delete_free_block(temp_node, next_header);

        //     //merge and initialize the new free block
        //     sf_header* padding_header=(void*)temp_header+total;
        //     int total_size=padding_size+(next_header->info.block_size<<4);
        //     padding_header=sf_init_freeblock(total_size,padding_header);

        //     //add the new free block into the list
        //     temp_node=sf_get_list(total_size);
        //     if(temp_node->size!=total_size)
        //         temp_node=sf_add_free_list(total_size,temp_node);
        //     padding_header=sf_add_free_block(temp_node,padding_header);

        //     //initialize the new required block
        //     temp_header=sf_init_alblock(rsize,total,temp_header);

        // }

        // else cases are: 1. padding size is 8 or 24
        //                 2. padding size is 16 and next block is allocated
        else
            temp_header=sf_init_alblock(rsize,size, temp_header);



    }

    return pp;
}

void* sf_init_alblock(int requested, int total, sf_header* temp_header){
    temp_header->info.allocated = 1;
    temp_header->info.requested_size = requested;
    temp_header->info.block_size = total>>4;

    //initialize nex free block header
    return change_prev_allocated((void*)temp_header+total,1)-total;
}

void* sf_init_freeblock(int total, sf_header* temp_header){
    sf_footer* temp_footer = (sf_footer*)((void*)temp_header+total-WSIZE);
    temp_header->info.allocated=0;
    temp_header->info.requested_size=0;
    temp_header->info.block_size = total>>4;
    temp_footer->info=temp_header->info;

    //initialize next block header
    return change_prev_allocated((void*)temp_header+total,0)-total;
}

//find a free block from free list and returns the header of the free block
void* find_fit(int required, int size){
    //get the list that has enough size
    sf_free_list_node* temp_node=sf_get_list(size);
    sf_header* temp_header;


    //case of the list has no free blocks, go to next list
    while(temp_node->size>=size){

        //check if there is a block in the list
        //if not, go to next list
        if(temp_node->head.links.next->info.block_size<<4<size){
            temp_node=temp_node->next;
            continue;
        }


        //if there is a block in the list
        //break
        else{
            temp_header=temp_node->head.links.next;
            break;
        }
    }

    //case of no list found, return NULL
    if(temp_node->size<size){
        return NULL;
    }


    //initialize some useful variables
    int padding_size=(temp_header->info.block_size<<4)-size;


    //delete the block from list
    temp_header=sf_delete_free_block(temp_node,temp_header);

    //in case the next block is last free block in heap and its size<32
    if(padding_size<32){
        size+=padding_size;
    }

    //initialize the required new block
    temp_header=sf_init_alblock(required, size, temp_header);

    //found the list, check the total size
    //if size too large, make the remaining to a new block
    if(padding_size>=32){


        //initialize the padding to a new free block
        sf_header* padding_header=(sf_header*)((void*)temp_header+size);
        padding_header=sf_init_freeblock(padding_size,padding_header);

        //add the free block(made from padding) to list
        temp_node=sf_get_list(padding_size);
        if(temp_node->size!=padding_size)
            temp_node=sf_add_free_list(padding_size,temp_node);
        padding_header=sf_add_free_block(temp_node,padding_header);

        //coalese the free block
        padding_header=sf_coalesce(padding_header);
    }
    return (void*)temp_header;
}

//coalesce the block with the block previous to it and next to it
//check if previous and next blocks are allocated
//the first free block after coalesced
void* sf_coalesce(sf_header* temp_header){

    //delete the current block from list first
    int size=temp_header->info.block_size<<4;
    sf_free_list_node* temp_node=sf_get_list(size);
    temp_header=sf_delete_free_block(temp_node,temp_header);


    //check if previous block is allocated
    if(temp_header->info.prev_allocated==0){

        sf_footer* temp_footer=(sf_footer*)((void*)temp_header-WSIZE);
        int prev_size=temp_footer->info.block_size<<4;

        //get the header of the previous block and delete the blocks from the list
        sf_header* prev=(sf_header*)((void*)temp_header-prev_size);
        temp_node=sf_get_list(prev_size);

        prev=sf_delete_free_block(temp_node,prev);

        //merge the two blocks
        size+=prev_size;
        temp_header=(sf_header*)sf_init_freeblock(size,prev);
    }

    //check if next block is allocated by footer
    sf_header* next=(sf_header*)((void*)temp_header+size);

    //coalesce with the next block
    if(next->info.allocated==0){

        //get the size of next block
        int next_size=next->info.block_size<<4;

        //delete the blocks from the list
        temp_node=sf_get_list(next_size);
        next=sf_delete_free_block(temp_node,next);
        temp_header=sf_delete_free_block(temp_node,temp_header);


        //merge the two blocks
        size+=next_size;
        temp_header=(sf_header*)sf_init_freeblock(size,temp_header);
    }


    //get the list node of the new block
    temp_node=sf_get_list(size);

    //if the list size not match, add the free list and points to it
    if(temp_node->size!=size)
        temp_node=sf_add_free_list(size,temp_node);

    //add the new block to the new list
    temp_header=sf_add_free_block(temp_node,temp_header);


    return temp_header;
}


// return a pointer to a list that has the size
// or the largest size list that smaller than the required size
sf_free_list_node* sf_get_list(int size){

    sf_free_list_node* temp=sf_free_list_head.next;
    while(temp->size!=0){

        //if requested size is smaller than
        if(temp->size<size){
            temp=temp->next;
            continue;
        }

        //size matches
        if(temp->size>=size){
            return temp;
        }

    }
    return temp;
}

void* change_prev_allocated(void* temp, int prev_allocated){
    sf_footer* header=(sf_footer*)temp;
    header->info.prev_allocated=prev_allocated;

    //if this block is free block, change footer as well
    if(header->info.allocated==0){
        sf_footer* footer=(void*)header+(header->info.block_size<<4)-sizeof(sf_footer);
        footer->info=header->info;
    }
    return (void*)header;
}

//add the free block to a matched size list
//return a pointer of the new added block
void* sf_add_free_block(sf_free_list_node* node, sf_header* temp_header){


    //connect the new lock with the list
    //as the one next to list head
    temp_header->links.prev=&node->head;
    temp_header->links.next=node->head.links.next;

    //break the list link and  connect to the new block
    node->head.links.next->links.prev=temp_header;
    node->head.links.next=temp_header;

    return (void*)temp_header;
}

void* sf_delete_free_block(sf_free_list_node* node, sf_header* temp_header){


    //delete doubly links prev to the block
    temp_header->links.prev->links.next=temp_header->links.next;

    //delete doubly links next to the block
    temp_header->links.next->links.prev=temp_header->links.prev;

    return (void*)temp_header;
}