#pragma once

#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#define BLOCK_SIZE 4096
#define NUMBER_OF_BLOCKS 1000

typedef struct buddy_block{
	struct buddy_block* next;
} buddy_block_t;

typedef struct buddy_allocator{
	unsigned int levels;
	unsigned int num_of_blocks;
	buddy_block_t* beginning_addr;
	buddy_block_t* array_of_levels[10];
}buddy_allocator_t;


// Buddy related
buddy_allocator_t* buddy_initialize(void* memory, unsigned int num_of_blocks);
void  buddy_initialize_buffers(buddy_allocator_t* buddy);
void* buddy_allocation(size_t size, buddy_allocator_t* buddy);
void* buddy_split(unsigned int level, buddy_allocator_t* buddy);
void  buddy_free(buddy_allocator_t* buddy, buddy_block_t* blk, size_t size);
void  buddy_display();


// Buddy helpers
void  buddy_split_once(unsigned int leve, buddy_allocator_t* buddy, void* tmp, unsigned int first);
void  buddy_remove_from_array_of_levels(unsigned int level, buddy_allocator_t* buddy);
void  buddy_insert_in_array_of_levels(unsigned int level, buddy_allocator_t* buddy, void* tmp);


// Helpers
int level_of_best_fit_block(size_t req);
int total_size_of_level(int i);

extern int global;