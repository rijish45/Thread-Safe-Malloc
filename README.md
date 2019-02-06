# Thread-Safe Malloc

For this assignment, we implemented two versions of thread-safe **malloc()** and **free()** using the best-fit memory allocation policy. In version one of the thread-safe malloc/free functions, we used lock-based synchronization to prevent race conditions that would lead to incorrect results. In version two of the thread-safe malloc/free functions, we don't use locks, with one exception. Because the **sbrk** function is not thread-safe, we acquire a lock immediately before calling sbrk and release a lock immediately after calling sbrk.

Best-Fit Algorithm
------------------

Performance of the best-fit algorithm for memory allocation:

The algorithm is as follow:

We start at the beginning of the list of blocks. We sequence through the list using a few conditions in the while loop. We have a current block used for iteration and a best_block which is the required desired block. The current block is initially set to point to the head of our linked list data structure. As long as the current block is free and the size of the current block is greater than the sum of the size requested by malloc and the size of the metadata and either the best_block is null or the size of the current block is less than the sum of the metadata size and size of the best_block, we iterate through the loop. As we iterate through the loop, we keep on updating our best_block. We optimize the iteration by having an if condition in which we break out of the loop if the size of the best_block is exactly equal to the sum of requested size and size of metadata. We then iterate through the linked list once more to set the last block which is utilized for growing the heap.

Implementation
--------------

```C
block find_best_fit_block_BF(block * last, size_t size){

block best_block = NULL;
block current = head;
while(current != NULL) {
    if ((current->free && (current->size >= size + BLOCK_SIZE)) &&
(best_block == NULL || current->size < BLOCK_SIZE + best_block->size)) {

best_block = current;
if (best_block->size == size + BLOCK_SIZE)
        break;
}
current = current->next;
}
current = head;

while(current != NULL){

if (((best_block - current) == 0))
    return current;

*last = current;
current = current->next;

}
return current; }
```

Thread-Safe Malloc and Free with Locks
--------------------------------------

If all the threads are using the same data structure, then we must have only one thread editing the linked-list at a time. We do this with the help of a mutex lock. The lock is set at the beginning of the **ts_malloc_lock()** and **ts_free_lock()** and is unlocked before the functions return. In this manner, we ensure that only one thread is working on the linked-list at a particular moment in time. We also implement a lock for the **sbrk()** function as it is not thread-safe.

Implementation
--------------

```C
void * ts_malloc_lock(size_t size){

pthread_mutex_lock(&lock);
if( size <= 0){ //Requested size is less than zero
	pthread_mutex_unlock(&lock);
	return NULL;
}
else { //Size is greater than zero

	block my_block;
	block last_block;

	if(!head){ //We are calling malloc for the first time, the head of the Linked List is NULL

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
```


Thread-Safe Malloc and Free without Locks
-----------------------------------------
In order implement thread-safe **Malloc()** and **Free()** without locks, we use the concept of Thread-local storage (TLS). Thread-local storage is a method that uses static or global memory local to a thread. It is a mechanism by which variables are allocated such that there is one instance of the variable per extant thread. A global head node called **global_head** is used which uses thread-local storage. A different copy of this global head node is used for every thread. As a result of this, each thread has it's own data structure and can work concurrently. We also implement a lock for the **sbrk()** function as it is not thread-safe.
The code structure is similar to the implementation of the thread-safe malloc and free with locks, however we don't need to use any locks for this instance. 



Performance and Analysis
========================


LOCK VERSION
-------------

| 	Run    | Execution Time|  Data Segment   | 
|----------|---------------|-----------------|     		
| 	1	   |  104.95990    |  42986016 	     |
| 	2	   |  106.44387    |  42865408 	     |
| 	3	   |  105.74988    |  43062336	     |
| 	4  	   |  107.89101    |  42780736       |
| 	5	   |  104.72402    |  42775808       |

**Average Execution Time**: 105.9537 seconds
**Average Data-Segment Size**: 42894061 bytes

NO-LOCK VERSION
---------------

| 	Run    | Execution Time|  Data Segment   |
|----------|---------------|-----------------|     		
| 	1	   |  0.228616     |   48729536 	 |
| 	2	   |  0.167109     |   48729536 	 |
| 	3	   |  0.171215     |   48729536	     |
| 	4      |  0.199377	   |   48729536      |
| 	5	   |  0.218044	   |   48729536      |

**Average Execution Time**: 0.196872 seconds.  
**Average Data-Segment Size**: 48729536 bytes.  


Analysis:
---------

We expected that the thread-safe implementation with locks would take more time to execute as each thread has to acquire a lock before a critical section is executed. THe other threads need to wait before the lock gets released. We expected the locked version to have a smaller data segment size as each thread in the lockless version has it's own linked-list.
Our experimental data agrees with our theory as we observe that the average execution time for the lock-less version is 0.196872 seconds which is much smaller compared to the execution time of the lock version which was about 105.9537 seconds. We also observe that the average data segment size for the lock-less version is greater than the lock version, we serves as a testament to our initial theory.







