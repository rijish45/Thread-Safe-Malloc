/*

	Rijish Ganguly
	ECE 650- System Programming
	rg239

*/

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
//#include <math.h>
#include "my_malloc.h"


pthread_mutex_t request_space_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t split = PTHREAD_MUTEX_INITIALIZER;




//head of our linked list

void * head = NULL;



//We check if we have a free block and return it if it is available using the first-fit algorithm

block find_free_block_FF(block * last, size_t size){

        block curr = head;

	//Now iterating throught the linked list
	while(curr && !(curr->free && curr->size >= (size + BLOCK_SIZE))){ //skips all the blocks which don't satisfy our criteria
		
		*last = curr; //assign the last node for requesting space
		curr = curr->next;

	}
	
	      return curr; //may return a NULL value 
}

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


/* If we don't find a free block, then we need to request it from OS using sbrk
sbrk(0) returns a pointer to the current top of the heap
sbrk(num) increments process data segment by num and returns a pointer to the previous top of the heap before change */


block new_space(block last, size_t size){

	block new_block;
	
	pthread_mutex_lock(&lock);
	new_block = sbrk(0); //current top of the heap
	void * new_space = sbrk(size + BLOCK_SIZE);
	pthread_mutex_unlock(&lock);
	
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
   
    pthread_mutex_lock(&lock);
	
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

	pthread_mutex_unlock(&lock);


}


//Combine free blocks of adjacent memory into a single memory chunk
//Initially tried deffered coalescing, but it took a long time
//After that switched to immediate coalescing

void coalesce(block my_block){

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


//Implement first-fit malloc now that we have our necessary helper functions

void *ff_malloc(size_t size){

	if( size <= 0){ //size has to be positive
		return NULL;
	}

	block my_block;
	block last_block;

	if(head == NULL){ //We are calling malloc for the first time, the head of the Linked List is NULL

		my_block = new_space(NULL, size); //Request new space
		if(!my_block){
			return NULL;
		}
		head = my_block; //Assign the head of the linked list
	}

	else{ //head is not NULL, malloc has been used atleast once

			last_block = head;
			my_block = find_free_block_FF(&last_block, size); //Search for the free block of memory 


			if(my_block != NULL){
				if(my_block->size >= size + BLOCK_SIZE){ //If only the size of the block found is greater than the requirement we call the split_block function
					split_block(my_block, size);
				}
			}

			else{ //The case where no free block was found

				my_block = new_space(last_block, size); //request new space
				if(!my_block)
					return NULL;
			}

	}


   return (my_block + 1); //we return b + 1 because we want to return a pointer to the region after block_meta_data

}



//bf_malloc is almost the same as ff_malloc but it used different function to choose the block

void *bf_malloc(size_t size){

	if( size <= 0){
		return NULL;
	}

	block my_block;
	block last_block;

	if(head == NULL){ //We are calling malloc for the first time, the head of the Linked List is NULL

		my_block = new_space(NULL, size); //Request new space
		if(!my_block){
			return NULL;
		}
		head = my_block; //Assign the head of the linked list
	}

	else{ //head is not NULL, malloc has been used atleast once

			last_block = head;
        	my_block = find_best_fit_block_BF(&last_block, size); //Search for the free block of memory


			if(my_block != NULL){
				if(my_block->size >= size + BLOCK_SIZE){ //If only the size of the block found is greater than the requirement we call the split_block function
					split_block(my_block, size);
				}
			}

			else{ //The case where no free block was found

				my_block = new_space(last_block, size);
				if(!my_block)
					return NULL;
			}

	}


   return (my_block + 1); //we return b + 1 because we want to return a pointer to the region after block_meta_data

}


//We need to get address of the struct i.e. where meta data is stored
block get_ptr(void * ptr){
  return((block)ptr -1);
}

//Implementation of ff_free

void ff_free(void * ptr){

	if (!ptr){
	    //printf("Cannot free a null pointer.");
		return;
	}

	block block_ptr = get_ptr(ptr);
	if(block_ptr){
	  block_ptr->free = 1; //free the block
	  coalesce(block_ptr); //merge the blocks
	}
	else //when the ptr is NULL
		return;
}


//Implementation of bf_free is same as ff_free
void bf_free(void * ptr){

	if (!ptr){
	    //printf("Cannot free a null pointer.");
		return;
	}

	block block_ptr = get_ptr(ptr);
	if(block_ptr){ //if not NULL
	  block_ptr->free = 1; //free the block
	  coalesce(block_ptr); //Do the merging of the blocks
	}
	else //ptr is a NULL
		return;


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


void *ts_malloc_lock(size_t size){

    pthread_mutex_lock(&lock); //initiate lock mechanism
	
	if( size <= 0){
		pthread_mutex_unlock(&lock);
		return NULL;
	}

	block my_block;
	block last_block;

	if(head == NULL){ //We are calling malloc for the first time, the head of the Linked List is NULL
       
        pthread_mutex_unlock(&lock);
		//pthread_mutex_lock(&request_space_mutex);
		my_block = new_space(NULL, size); //Request new space
		head = my_block; //Assign the head of the linked list
	}

	else{ //head is not NULL, malloc has been used atleast once

		    last_block = head;
        	my_block = find_best_fit_block_BF(&last_block, size); //Search for the free block of memory
			
			if(my_block != NULL){
				
				pthread_mutex_unlock(&lock);
				if(my_block->size >= size + BLOCK_SIZE){ //If only the size of the block found is greater than the requirement we call the split_block function
					split_block(my_block, size);
				}
			}

			else{ //The case where no free block was found
				pthread_mutex_unlock(&lock);
				my_block = new_space(last_block, size);
			  }
		}

   
   return (my_block + 1); //we return b + 1 because we want to return a pointer to the region after block_meta_data

}


void ts_free_lock(void *ptr){

pthread_mutex_lock(&lock); //initiate locking mechanism

if (!ptr){
	    //printf("Cannot free a null pointer.");
	    pthread_mutex_unlock(&lock);
		return;
}

block block_ptr = get_ptr(ptr);

else{

	if(block_ptr){ //if not NULL
	  pthread_mutex_unlock(&lock);
	  block_ptr->free = 1; //free the block
	  coalesce(block_ptr); //Do the merging of the blocks
	}
	else { //ptr is a NULL
			pthread_mutex_unlock(&lock);
			return;
		}
}

};














