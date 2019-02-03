#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "my_malloc.h"



//head of our linked list

void * head = NULL;
__thread block global_head = NULL;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t extend_lock = PTHREAD_MUTEX_INITIALIZER;



//Implementation of the best fit algorithm
block find_best_fit_block_BF(block * last, size_t size){

	//Find the optimum block first 
	block best_block = NULL; 
	block current = head; 

	while(current != NULL) { 
	  if ((current->free && (current->size >= size + BLOCK_SIZE)) && (best_block == NULL || current->size < BLOCK_SIZE + best_block->size)) {
			 best_block = current; //assign the best block
			 if(best_block->size == size + BLOCK_SIZE) //break for optimization
			   break;
 		}
		
		current = current->next; //normal iteration
	} 

 

//Now assign the last block for requesting space in later situation

current = head;
//This loop is to assign the last node for requesting space
while(current != NULL){
		
		if(((best_block - current) == 0))
			return current;
		*last = current; //Assigned the last node if we haven't found a best block
		current = current->next;
	}

return current; //may return NULL
}


block find_best_fit_block_BF_NL(block * last, size_t size){

	//Find the optimum block first 
	block best_block = NULL; 
	block current = global_head; 

	while(current != NULL) { 
	  if ((current->free && (current->size >= size + BLOCK_SIZE)) && (best_block == NULL || current->size < BLOCK_SIZE + best_block->size)) {
			 best_block = current; //assign the best block
			 if(best_block->size == size + BLOCK_SIZE) //break for optimization
			   break;
 		}
		
		current = current->next; //normal iteration
	} 

 

//Now assign the last block for requesting space in later situation
current = global_head;
//This loop is to assign the last node for requesting space
while(current != NULL){
		
		if(((best_block - current) == 0))
			return current;
		*last = current; //Assigned the last node if we haven't found a best block
		current = current->next;
	}

return current; //may return NULL
}






/* If we don't find a free block, then we need to request it from OS using sbrk
sbrk(0) returns a pointer to the current top of the heap
sbrk(num) increments process data segment by num and returns a pointer to the previous top of the heap before change */


block new_space(block last, size_t size){

	block new_block;
	pthread_mutex_lock(&extend_lock);
	new_block = sbrk(0); //current top of the heap
	void * new_space = sbrk(size + BLOCK_SIZE);
	pthread_mutex_unlock(&extend_lock);
	if(new_space == (void*) - 1){
		return NULL; //sbrk has failed
	}

	if(last){ //when last is not NULL, i.e this is not the first call for malloc. We add a new block at the end of the last block
		last->next = new_block;
	}

    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL; //End of the list
    new_block->prev = last; //assigning the previous ptr for the new block requested

    return new_block;

}



/*If we find a free block which exactly fits the required size, we don't need to do splitting. Otherwise, if the block size is greater than the size of the 
requested slot, it's better to split the block into two partitions */

void split_block(block mblock, size_t size){
    
    if(mblock){ //if mblock is not NULL

	block new_block = (void *)((void *)mblock + size + BLOCK_SIZE);
	new_block->size = (mblock->size - size - BLOCK_SIZE);
	new_block->next = mblock->next;
	new_block->free = 1; //The new block is free 
	new_block->prev = mblock;

	mblock->size = size; //The size of the input block gets reduced
	mblock->free = 0; //allocated
	mblock->next = new_block;


	if(new_block->next){
			new_block->next->prev = new_block;
		}
	}


}

//We need to get address of the struct i.e. where meta data is stored
block get_ptr(void * ptr){
  return((block)ptr -1);
}


//Combine free blocks of adjacent memory into a single memory chunk
//Initially tried deffered coalescing, but it took a long time
//After that switched to immediate coalescing

void coalesce(block my_block){
  //The following two cases would be able to handle all merge conditions

 if(my_block->next){
		if(my_block->next == (block)0x1){
			return;
		}
		if(my_block->next->free){ //if the next block is free, merge the current block with the next block
			my_block->size += BLOCK_SIZE + my_block->next->size;
			my_block->next = my_block->next->next;

			if(my_block->next){	
				my_block->next->prev = my_block;
			}	

		}
  }

  if(my_block->prev){
		block temp;
		if(my_block->prev->free){ //if the previous blocks is free, merge with the previous block
			temp = my_block->prev;
			temp->size = temp->size + BLOCK_SIZE + my_block->size;
			temp->next = my_block->next;
			if(temp->next){
				temp->next->prev = temp;
			}	
		}

	}

}

//Function to get the total data segment size

unsigned long get_data_segment_size(){
unsigned long value = (unsigned long)(sbrk(0) - head); //sbrk(0) gives the current top of the heap
return value;
}


//Function to get the total free segment size
unsigned long get_data_segment_free_space_size(){
block my_block = head;
unsigned long size = 0; 
	
	while( my_block != NULL){  //We iterate through the entire data segment and add the blocks which are marked free
         if(my_block->free){
			size = size + my_block->size; //Keep on adding the free spaces
        }
       my_block = my_block->next;
    }

return size;

}

void * ts_malloc_lock(size_t size){

pthread_mutex_lock(&lock);
if( size <= 0){ //Requested size is less than zero
		
	pthread_mutex_unlock(&lock);
	return NULL;
}
else { //Size is greater than zero

	block my_block;
	block last_block;

	if(head == NULL){ //We are calling malloc for the first time, the head of the Linked List is NULL

		my_block = new_space(NULL, size); //Request new space
		if(!my_block){ //if my_block is NULL

			pthread_mutex_unlock(&lock);
			return NULL;
		}
		else{ //my_block is not NULL
			head = my_block; //Assign the head of the linked list
			pthread_mutex_unlock(&lock);
			return (my_block + 1);
		}
	}

	else{ //head is not NULL, malloc has been used atleast once

			last_block = head;
        	my_block = find_best_fit_block_BF(&last_block, size); //Search for the free block of memory
			if(my_block != NULL){ //suitable block is found
				
				if(my_block->size >= size + BLOCK_SIZE){ //If only the size of the block found is greater than the requirement we call the split_block function
					split_block(my_block, size);
					my_block->free = 0;
				}
				pthread_mutex_unlock(&lock);
				return (my_block + 1);
			}

			else{ //The case where no free block was found

				my_block = new_space(last_block, size);
				if(!my_block){
					pthread_mutex_unlock(&lock);
					return NULL;
				}
				else {
					pthread_mutex_unlock(&lock);
					return my_block + 1;
				}

			}

	}

}

}



void ts_free_lock(void * ptr){

    pthread_mutex_lock(&lock);
	if (!ptr){
	    //printf("Cannot free a null pointer.");
	    pthread_mutex_unlock(&lock);
		return;
	}
    
    else { //valid ptr
		
		block block_ptr = get_ptr(ptr);
		if(block_ptr){
	  		block_ptr->free = 1; //free the block
	  		coalesce(block_ptr); //merge the blocks
	  		pthread_mutex_unlock(&lock);
	  		return;
		}
	else{ //when the ptr is NULL
		pthread_mutex_unlock(&lock);
		return;
	}
}

}



void *ts_malloc_nolock(size_t size){


if( size <= 0){ //Requested size is less than zero
	return NULL;
}
else { //Size is greater than zero

	block my_block;
	block last_block;

	if(global_head == NULL){ //We are calling malloc for the first time, the head of the Linked List is NULL

		my_block = new_space(NULL, size); //Request new space
		if(!my_block){ //if my_block is NULL
			return NULL;
		}
		else{ //my_block is not NULL
			global_head = my_block; //Assign the head of the linked list
			//pthread_mutex_unlock(&lock);
			return (my_block + 1);
		}
	}

	else{ //head is not NULL, malloc has been used atleast once

			last_block = global_head;
        	my_block = find_best_fit_block_BF(&last_block, size); //Search for the free block of memory
			if(my_block != NULL){ //suitable block is found
				
				if(my_block->size >= size + BLOCK_SIZE){ //If only the size of the block found is greater than the requirement we call the split_block function
					split_block(my_block, size);
					my_block->free = 0;
				}
				//pthread_mutex_unlock(&lock);
				return (my_block + 1);
			}

			else{ //The case where no free block was found

				my_block = new_space(last_block, size);
				if(!my_block){
					//pthread_mutex_unlock(&lock);
					return NULL;
				}
				else {
					//pthread_mutex_unlock(&lock);
					return my_block + 1;
				}

			}

	}

}


}

void ts_free_nolock(void *ptr){

	if (!ptr){
	    //printf("Cannot free a null pointer.");
	    //pthread_mutex_unlock(&lock);
		return;
	}
    
    else { //valid ptr
		
		block block_ptr = get_ptr(ptr);
		if(block_ptr){
	  		block_ptr->free = 1; //free the block
	  		coalesce(block_ptr); //merge the blocks
	  		//pthread_mutex_unlock(&lock);
	  		return;
		}
	else{ //when the ptr is NULL
		//pthread_mutex_unlock(&lock);
		return;
	}
}

}
















