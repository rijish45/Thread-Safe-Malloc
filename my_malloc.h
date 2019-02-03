#include <stddef.h>
#ifndef MY_MALLOC
#define MY_MALLOC
#define BLOCK_SIZE sizeof(struct block_meta_data)


//Functions to calculate fragmentation
unsigned long get_data_segment_size(); //total data segment in bytes
unsigned long get_data_segment_free_space_size(); //free data segment in bytes


//My data structure used for the implementation of malloc and free
struct block_meta_data {

	size_t size; //size of the block
	struct block_meta_data * next; //next meta-data block
    struct block_meta_data * prev; //previous meta-data block
	int free; //this is a flag variable descibing whether the block is free or not denoted by 0 and 1

};

typedef struct block_meta_data * block;

//Thread-Safe malloc and free with locks
void *ts_malloc_lock(size_t size);
void ts_free_lock(void *ptr);


//Thread-safe malloc  and free with no-locks
void *ts_malloc_nolock(size_t size);
void ts_free_nolock(void *ptr);

//Searching the appropriate block using the Best-Fit memory allocation policy
block find_best_fit_block_BF(block * last, size_t size);
block find_best_fit_block_BF_NL(block * last, size_t size); //No-lock version


//Other necessary functions to implement malloc and free
block new_space(block last, size_t size);
void split_block(block mblock, size_t size);
void coalesce(block my_block);
#endif