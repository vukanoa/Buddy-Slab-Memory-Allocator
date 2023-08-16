#pragma once


#include <math.h>
#include "Buddy_allocator.h"

int
total_size_of_level(int i)
{
	return (0x1 << i) * BLOCK_SIZE;
}


int
level_of_best_fit_block(size_t req)
{
	unsigned int total = req + sizeof(buddy_block_t);
	unsigned int i = 0x1;

	while (i < total)
		i <<= 1;

	return i;
}


void
buddy_initialize_buffers(buddy_allocator_t* buddy)
{
	assert(buddy != NULL);

	for (int i = 0; i < buddy->levels; i++)
		buddy->array_of_levels[i] = NULL;

	unsigned int num_of_blocks = buddy->num_of_blocks;
	unsigned int index = floor(log(num_of_blocks) / log(2));

	uintptr_t mem_addr = (buddy_block_t*)buddy->beginning_addr;
	uintptr_t tmp = (buddy_block_t*)buddy->array_of_levels[index];


	for(; num_of_blocks; num_of_blocks -= (0x1 << index))
	{
		index = floor(log(num_of_blocks) / log(2));

		buddy->array_of_levels[index] = mem_addr;
		((buddy_block_t*)mem_addr)->next = NULL;
		
		mem_addr = (uintptr_t)mem_addr + total_size_of_level(index);
	}
}


buddy_allocator_t*
buddy_initialize(void* memory, unsigned int num_of_blocks)
{
	assert(num_of_blocks > 2);
	assert(memory != NULL);

	unsigned int levels = ceil(log(num_of_blocks) / log(2));

	buddy_allocator_t* buddy = (buddy_allocator_t*)memory;
	//void* offset = (uintptr_t)memory + BLOCK_SIZE;
	buddy_block_t* block = (buddy_block_t*)((uintptr_t)memory + BLOCK_SIZE);

	buddy->levels = levels;
	buddy->num_of_blocks = num_of_blocks - 1; // First one for the Slab_allocator
	buddy->beginning_addr = block;
	
	buddy_initialize_buffers(buddy);

	return buddy;
}



void
buddy_remove_from_array_of_levels(unsigned int level, buddy_allocator_t* buddy)
{
	assert(buddy != NULL);
	assert(buddy->array_of_levels[level] != NULL);

	buddy->array_of_levels[level] = buddy->array_of_levels[level]->next;
}


void
buddy_insert_in_array_of_levels(unsigned int level, buddy_allocator_t* buddy, uintptr_t tmp)
{
	assert(buddy != NULL);

	// Greater address => smaller array_of_level index. 
	buddy->array_of_levels[level] = (uintptr_t)tmp + BLOCK_SIZE * (unsigned long long) (0x1 << level);
	buddy->array_of_levels[level]->next = NULL;
}


void
buddy_split_once(unsigned int level, buddy_allocator_t* buddy, uintptr_t tmp, unsigned int first)
{
	if (first == 1) 
		buddy_remove_from_array_of_levels(level, buddy);

	buddy_insert_in_array_of_levels(level - 1, buddy, tmp);
}


void*
buddy_split(unsigned int level, buddy_allocator_t* buddy)
{
	unsigned int start_level = level - 1;

	while (buddy->array_of_levels[level] == NULL && level < 10)
		level++;

	if (level > 9)
		return NULL;

	unsigned int destination = level - start_level;
	unsigned int first = 1;

	void* tmp = buddy->array_of_levels[level];
	
	while (destination--)
	{
		buddy_split_once(level, buddy, tmp, first);
		level--;
		first = 0;
	}
	
	return tmp;
}


void*
buddy_allocation(size_t size, buddy_allocator_t* buddy)
{
	printf("------------------------\n");
	//buddy_display(buddy);

	uintptr_t ret = NULL;
	unsigned int num_of_blocks = size / BLOCK_SIZE;
	unsigned int level = ceil(log(num_of_blocks) / log(2));
	unsigned int tmp_level = level;
	
	//if(num_of_blocks == 1)
	//	printf("( %d ) Block requested!\n\n", num_of_blocks);
	//else
	//	printf("( %d ) Blocks requested!\n\n", num_of_blocks);

	if (num_of_blocks > 512 || num_of_blocks <= 0)
		return NULL;

	if (buddy->array_of_levels[level] != NULL)
	{
		
		ret =  buddy->array_of_levels[level];
		if (buddy->array_of_levels[level]->next)
			printf("sta je ovo ");
		buddy->array_of_levels[level] = buddy->array_of_levels[level]->next;

		//buddy_display(buddy);

		if(tmp_level)
			printf("\n==[ %d ] Blocks - Allocated successfully!\n\n", 0x1 << tmp_level);
		else
			printf("\n==[ %d ] Block - Allocated successfully!\n\n", 1);

		printf("------------------------\n");
		

		return ret;
	}
	else
	{
		ret = buddy_split(++level, buddy);
	}

	if (!ret) 
	{
		printf("Nema vise mesta!!!\n\n");
		printf("------------------------\n");

		return NULL;
	}
		
	//buddy_display(buddy);

	if (tmp_level)
		printf("\n==[ %d ] Blocks - Allocated successfully!\n\n", 0x1 << tmp_level);
	else
		printf("\n==[ %d ] Block - Allocated successfully!\n\n", 1);

	printf("------------------------\n");

	return ret;
}


void
buddy_display(buddy_allocator_t* buddy)
{
	unsigned int levels = buddy->levels;
	buddy_block_t* tmp_blk; 

	for (int i = 0; i < levels; i++)
	{
		if (buddy->array_of_levels[i] != NULL)
		{
			printf("%d. -> IMA", i);
			//printf(" %p\n", buddy->array_of_levels[i]);
			
			tmp_blk = buddy->array_of_levels[i];

			while (tmp_blk)
			{
	
				if (tmp_blk->next)
					printf(" %p,", tmp_blk);
				else
					printf(" %p", tmp_blk);

				tmp_blk = tmp_blk->next;
				

			}	
			printf("\n");		
		}
		else
			printf("%d. -> NULL\n", i);
	}
	printf("\n\n");
}


void
buddy_free(buddy_allocator_t* buddy, buddy_block_t* blk, size_t size)
{
	assert(blk  != NULL);
	assert(size != NULL);

	unsigned int num_of_blocks = size / BLOCK_SIZE;
	unsigned int level = ceil(log(num_of_blocks) / log(2));

	buddy_block_t* iter = buddy->array_of_levels[level];
	buddy_block_t* prev = NULL;

	while (iter)
	{
		//printf("BLK :%p + %x = %p\n\n", blk, BLOCK_SIZE * (0x1 << level), iter);
		//if ((char*)blk + BLOCK_SIZE * (0x1 << level) == iter)
		if ((uintptr_t)blk + BLOCK_SIZE * (0x1 << level) == iter)
		{
			if (prev != NULL)
				prev->next = iter->next;
			else
				buddy->array_of_levels[level] = NULL;
			
			break;
		}
		prev = iter;
		iter = iter->next;
	}

	// No buddy block to merge with
	if (iter == NULL)
	{
		if (buddy->array_of_levels[level])
			blk->next = buddy->array_of_levels[level];
		else
			blk->next = NULL;


		buddy->array_of_levels[level] = blk;

		return;
	}

	if (buddy->array_of_levels[level + 1])
		buddy_free(buddy, blk, 2 * size);	
	else
	{
		blk->next = NULL;
		buddy->array_of_levels[level + 1] = blk;

		return;
	}
}