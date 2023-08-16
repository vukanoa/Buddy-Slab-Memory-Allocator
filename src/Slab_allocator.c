#pragma once


#include "Slab_allocator.h"
#include <math.h>
#include <stdint.h>


#define REST_OF_FIRST_BLOCK_IN_BYTES	( ( (BLOCK_SIZE) -  (sizeof(buddy_allocator_t)) - (sizeof(slab_allocator_t)) ) )
#define REST_OF_BLOCK_IN_CACHES_NUM	    ( (REST_OF_FIRST_BLOCK_IN_BYTES) / ( (sizeof(kmem_cache_s) + sizeof(uint8_t) ) )  - 1)

#define BUDDY_AND_SLAB_ALLOCATOR ( (sizeof(buddy_allocator_t)) + (sizeof(slab_allocator_t)) )
#define BUDDY_SLAB_ARRAY ( (BUDDY_AND_SLAB_ALLOCATOR) + (sizeof(uint8_t)) * (REST_OF_BLOCK_IN_CACHES_NUM) )

static buddy_allocator_t* buddy = NULL;
static slab_allocator_t* slab_allocator = NULL;
static void* first_cache;

uint8_t shrink_flag = 0; // 0x1 indicates that there was a need for a new slab;


void
display_buddy()
{
	buddy_display(buddy);
}


void
kmem_init(void* space, int block_num)
{
	buddy = buddy_initialize(space, block_num);

	void* adr = (uintptr_t)space + sizeof(buddy_allocator_t);
	slab_allocator = (slab_allocator_t*)adr;

	slab_allocator->free_caches_array = (uintptr_t)space + BLOCK_SIZE - REST_OF_FIRST_BLOCK_IN_BYTES;
	slab_allocator->mutex = CreateMutex(NULL, FALSE, "Mutex_Slab_Allocator");

	// All caches are free <=> 1
	for (int i = 0; i < REST_OF_BLOCK_IN_CACHES_NUM; i++)
		*((uint8_t*)space + BUDDY_AND_SLAB_ALLOCATOR + i) = 1;

	first_cache = (uintptr_t)space + BUDDY_SLAB_ARRAY;

	small_buffer_caches_init();
}


void
small_buffer_caches_init()
{
	assert(slab_allocator);

	kmem_cache_t* beginning_cache = slab_allocator;

	for (int i = MIN_BUF; i < NUM_OF_BUFFERS + MIN_BUF; i++)
	{
		kmem_cache_t* iter_cache = beginning_cache + i - MIN_BUF;

		//strncpy_s(iter_cache->name, NAME_LENGTH - 1, "Cache_small_buffer", NAME_LENGTH - 1);

		if (i == 5)
			sprintf_s(iter_cache->name, NAME_LENGTH, "Cache_small_buffer_%d(%dst)", i, i - MIN_BUF + 1);
		else if (i == 6)
			sprintf_s(iter_cache->name, NAME_LENGTH, "Cache_small_buffer_%d(%dnd)", i, i - MIN_BUF + 1);
		else if (i == 7)
			sprintf_s(iter_cache->name, NAME_LENGTH, "Cache_small_buffer_%d(%drd)", i, i - MIN_BUF + 1);
		else
			sprintf_s(iter_cache->name, NAME_LENGTH, "Cache_small_buffer_%d(%dth)", i, i - MIN_BUF + 1);


		iter_cache->ctor = NULL;
		iter_cache->dtor = NULL;

		iter_cache->empty = NULL;
		iter_cache->partial = NULL;
		iter_cache->full = NULL;

		iter_cache->size_of_slot = 0x1 << i;

		// Offset after slab_header
		iter_cache->L1_offset = 0;
		iter_cache->layer = 1;


		size_t x = BLOCK_SIZE;

		// At least 4 objects per slab. Otherwise partial_slab makes no sense
		//for (x = BLOCK_SIZE; ((x - sizeof(slab_header_t)) / (beginning_cache->size_of_slot + 1 / 8) + 1) < 5; x += BLOCK_SIZE);

		//while (x - sizeof(slab_header_t) < iter_cache->size_of_slot)
		//	x += BLOCK_SIZE;

		iter_cache->slab_size_in_bytes = (unsigned)ceil(iter_cache->size_of_slot * 1.0 / BLOCK_SIZE) * BLOCK_SIZE;
		iter_cache->num_of_slots_in_slab = floor((iter_cache->slab_size_in_bytes - sizeof(slab_header_t)) / (iter_cache->size_of_slot + 1.0 / 32));

		if (iter_cache->num_of_slots_in_slab == 0)
			iter_cache->num_of_slots_in_slab = 1;

		if (iter_cache->size_of_slot >= BLOCK_SIZE)
			iter_cache->size_of_slot = iter_cache->slab_size_in_bytes - sizeof(slab_header_t) - 32;
	}
}


kmem_cache_t*
kmem_cache_create(const char* name, size_t size, void (*ctor)(void*), void (*dtor)(void*))
{
	kmem_cache_t* ret_cache = give_free_cache();

	if (ret_cache == NULL)
		return NULL;

	strncpy_s(ret_cache->name, NAME_LENGTH - 1, name, NAME_LENGTH - 1);

	ret_cache->ctor = ctor;
	ret_cache->dtor = dtor;

	ret_cache->empty = NULL;
	ret_cache->partial = NULL;
	ret_cache->full = NULL;

	// Offset odmah iza hedera Slab-a
	ret_cache->L1_offset = 0;

	ret_cache->error_flags = 0;

	ret_cache->size_of_slot = size;

	unsigned x = BLOCK_SIZE;

	x = (unsigned)ceil(((size + sizeof(slab_header_t) * 1.) / BLOCK_SIZE))* BLOCK_SIZE;

	ret_cache->slab_size_in_bytes = x;


	/*
	* 
		s - slab size 
		h - header size
		p = s- h  // Space without the Header
		st - slot size
		num - Number of Slots
		l - array level of bits


		p = num * (st + (1/32))
		num = floor(p*1. / (st + (1/32)))

		l = (unsigned)ceil(num*1. /32 )
	*/
	unsigned s = ret_cache->slab_size_in_bytes;
	unsigned h = sizeof(slab_header_t);
	unsigned p = s - h;
	unsigned st = ret_cache->size_of_slot;
	unsigned l = p - p /st *st; // array of bits
	unsigned num = (unsigned)floor(( p - l *4) * 1. / st);
	if (num == 0) {
		num = 1;
		l = 1;
		// pocetni slot
	}
	l = (unsigned)ceil(num * 1. / 32);

	l = 1;
	num = 1;
	while (num * st + l * 4 < p) {
		num++;
		l = (unsigned)ceil(num * 1. / 32); 
	}
	num--;
	ret_cache->num_of_slots_in_slab = num;
	ret_cache->layer = l;


	printf("\n");
	printf("==============================\n");
	printf("==== NORMAL CACHE CREATED ====\n");
	printf("==============================\n");
	printf("-> NAME_OF_CACHE: %s\n", ret_cache->name);
	printf("-> BLOCKS: \t  %d\n", x / BLOCK_SIZE);
	printf("-> NUM_OF_SLOTS : %d\n", ret_cache->num_of_slots_in_slab);
	printf("==============================\n");
	printf("\n");

	return ret_cache;
}


kmem_cache_t*
give_free_cache()
{
	uint8_t* q = slab_allocator->free_caches_array;
	for (int i = 0; i < REST_OF_BLOCK_IN_CACHES_NUM; i++)
	{

		if (*((uint8_t*)q + i) == 1)
		{
			*((uint8_t*)q + i) = 0;

			return (kmem_cache_t*)first_cache + i;
		}
	}

	return NULL;
}


void
print_array_of_free_caches()
{
	uint8_t* q = slab_allocator->free_caches_array;

	for (int i = 0; i < REST_OF_BLOCK_IN_CACHES_NUM; i++)
		printf("%d. -> %d\n", i, *((uint8_t*)q + i));
}


int
set_free_cache(kmem_cache_t* cachep)
{
	uint8_t* q = slab_allocator->free_caches_array;
	for (int i = 0; i < REST_OF_BLOCK_IN_CACHES_NUM; i++)
	{
		if (cachep == (kmem_cache_t*)first_cache + i)
		{
			if (*((uint8_t*)q + i) == 0)
			{
				printf("VRATIO %d. cache\n", i);
				*((uint8_t*)q + i) = 1; // 1 Indicates that the cache is free for allocation

				return 0;
			}
		}
	}

	return -1;
}


void*
kmem_cache_alloc(kmem_cache_t* cachep)
{
	WaitForSingleObject(slab_allocator->mutex, INFINITE);

	uint8_t nothing = 0;

	for (int i = 0; i < REST_OF_BLOCK_IN_CACHES_NUM; i++)
	{
		if (cachep == ((kmem_cache_t*)first_cache + i))
		{
			nothing = 1;
			break;
		}
	}

	// Small buffers
	for (int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		if (nothing)
			break;

		if (cachep == ((kmem_cache_t*)slab_allocator + i))
		{
			nothing = 1;
			break;
		}
	}

	if (nothing == 0) {
		ReleaseMutex(slab_allocator->mutex);
		return NULL;
	}


	slab_header_t* slab = NULL;

	if (cachep->partial)
	{
		slab = cachep->partial;
		cachep->partial = slab->next;

		slab->next = NULL;
	}
	else if (cachep->empty)
	{
		slab = cachep->empty;
		cachep->empty = slab->next;

		slab->next = NULL;
	}
	else
	{
		slab_header_t* slab_tmpt = add_slab(cachep);

		if (!slab_tmpt)
		{
			cachep->error_flags |= ERROR_ADDING_SLAB;

			ReleaseMutex(slab_allocator->mutex);
			return NULL;
		}

		slab = slab_tmpt;
	}

	// Bit that represent the number of free slot
	int th_bit = position_of_free_slot_in_bits(slab);

	if (th_bit == -1)
	{
		printf("\n\nSlab allocation ERROR\n\n");
		//position_of_free_slot_in_bits(slab);
		ReleaseMutex(slab_allocator->mutex);
		return NULL;
	}

	if (slab->num_of_free_slots == 0)
	{
		slab->next = cachep->full;
		cachep->full = slab;
	}
	else
	{
		slab->next = cachep->partial;
		cachep->partial = slab;
	}

	uintptr_t rndm = slab;
	uintptr_t slabic = slab->beginning_slot - rndm;

	if(th_bit > 102)
		rndm = slab->beginning_slot + th_bit * (unsigned long long) cachep->size_of_slot;

	uintptr_t slot = slab->beginning_slot + th_bit * (unsigned long long) cachep->size_of_slot;

	uintptr_t razlika = slot - (uintptr_t)slab;

	if(slot  > (uintptr_t)slab + cachep->slab_size_in_bytes)
		razlika = slot - (uintptr_t)slab;


	if (cachep->ctor)
		cachep->ctor(slot);

	ReleaseMutex(slab_allocator->mutex);
	//return slab->beginning_slot + th_bit * cachep->size_of_slot;
	return slot;
}


void*
add_slab(kmem_cache_t* cachep)
{
	uintptr_t block = buddy_allocation(cachep->slab_size_in_bytes, buddy);

	if (block == NULL)
	{
		printf("\n\nBUDDY_ALLOCATION_ERROR\n\n");
		cachep->error_flags |= ERROR_BUDDY_ALLOCATOR_FAIL;
		return NULL;
	}

	slab_header_t* tmp_slab = block;
	tmp_slab->cache_owner = cachep;


	tmp_slab->num_of_free_slots = cachep->num_of_slots_in_slab;

	// Bits
	tmp_slab->levels_of_array_of_bits = cachep->layer;
	tmp_slab->total_num_of_bits = cachep->num_of_slots_in_slab;

	// Starting addresses
	tmp_slab->beginning_of_bits_array = block + sizeof(slab_header_t);
	tmp_slab->beginning_slot = (uintptr_t)tmp_slab->beginning_of_bits_array + (unsigned long long) tmp_slab->levels_of_array_of_bits * 4;

	// Internal Fragmentation
	tmp_slab->internal_fragmentation_in_bytes = (cachep->slab_size_in_bytes - sizeof(slab_header_t) - tmp_slab->levels_of_array_of_bits * sizeof(unsigned)) % ((cachep->size_of_slot));
	
	memset(tmp_slab->beginning_of_bits_array, 0, tmp_slab->levels_of_array_of_bits * sizeof(unsigned));


	//l1
	tmp_slab->next = NULL;

	return tmp_slab;
}


int
position_of_free_slot_in_bits(slab_header_t* slab)
{
	unsigned* begin = slab->beginning_of_bits_array;
	unsigned mask = 0;

	int flag = 0;
	int broj = slab->total_num_of_bits;

	int th_bit = -1;

	for (int i = 0; i < slab->levels_of_array_of_bits; i++)
	{
		for (int j = 0; j < 32; j++)
		{

			mask = 0x80000000;
			mask >>= j;
			if ((mask & begin[i]) == 0)
			{
				/*	printf("%s [%d, %d] PRE \n", slab->cache_owner->name,i, j);
					print_bits(begin[i]);*/
				begin[i] |= mask;
				th_bit = i * 32 + j;
				/*	printf("%s [%d, %d] POSLE \n", slab->cache_owner->name, i, j);
					print_bits(begin[i]);*/

				slab->num_of_free_slots--;

				flag = 1;
				break;
			}

			if (j == broj - 1)
			{
				flag = 1;
				break;
			}

		}
		broj -= 32;

		if (flag)
			break;

	}

	return th_bit;
}


int
degre_counter(size_t size)
{
	int x = 0x1;
	int ret = 0;
	while (x < size)
	{
		x <<= 1;
		ret++;
	}

	return ret;
}


void*
kmalloc(size_t size) // Alloacate one small memory buffer
{

	WaitForSingleObject(slab_allocator->mutex, INFINITE);
	int degree = degre_counter(size);


	if (degree < 5 || degree > 17)
	{
		printf("Premala ili prevelika vrednost zatrazena za male memorijske bafere\n");

		ReleaseMutex(slab_allocator->mutex);
		return NULL;
	}

	kmem_cache_t* cachep = (kmem_cache_t*)slab_allocator + degree - MIN_BUF;

	kmem_cache_info(cachep);


	slab_header_t* address_small = (slab_header_t*)kmem_cache_alloc(cachep);

	kmem_cache_info(cachep);

	ReleaseMutex(slab_allocator->mutex);
	return (void*)address_small;
}


unsigned int
rotate_bits_right(unsigned int value, int shift) {
	if ((shift &= 31) == 0)
		return value;
	return (value >> shift) | (value << (32 - shift));
}


void
print_bits(unsigned int num)
{
	int i;
	for (i = (sizeof(unsigned int) * 8) - 1; i >= 0; i--) {
		char c = (num & (1LL << i)) ? '1' : '0';
		putchar(c);
	}
	printf("\n");
}


slab_header_t*
get_slab_by_slot(kmem_cache_t* cachep, void* obj, int* what_is)
{
	slab_header_t* tmp_slab = cachep->partial;

	//unsigned* begin_addr = tmp_slab->beginning_of_bits_array;
	unsigned* begin_addr;

	unsigned mask = 0;

	while (tmp_slab)
	{
		begin_addr = tmp_slab->beginning_of_bits_array;

		for (int i = 0; i < tmp_slab->total_num_of_bits; i++)
		{
			for (int j = 0; j < 32; j++)
			{
				mask = 0x80000000 >> j;
				if (((mask & *((unsigned*)begin_addr + i)))) // Trazeni bit jeste 1, tj. zauzeta je lokacija
				{
					*what_is = 1;
					mask = rotate_bits_right(0x7fffffff, j);

					// Set that bit to 0, indicating that the certain slot is free for allocation again
					*((unsigned*)begin_addr + i) &= mask;

					tmp_slab->num_of_free_slots++;

					return tmp_slab;
				}
			}
		}
		tmp_slab = tmp_slab->next;
	}

	tmp_slab = cachep->full;

	while (tmp_slab)
	{
		begin_addr = tmp_slab->beginning_of_bits_array;

		for (int i = 0; i < tmp_slab->total_num_of_bits; i++)
		{
			for (int j = 0; j < 32; j++)
			{
				mask = 0x80000000 >> j;
				if (((mask & *((unsigned*)begin_addr + i)))) // Trazeni bit jeste 1, tj. zauzeta je lokacija
				{
					*what_is = 2;
					mask = rotate_bits_right(0x7fffffff, j);

					// Set that bit to 0, indicating that the certain slot is free for allocation again
					*((unsigned*)begin_addr + i) &= mask;

					tmp_slab->num_of_free_slots++;

					return tmp_slab;
				}
			}
		}
		tmp_slab = tmp_slab->next;
	}

	// Makes no sense to try and free slot from an emtpy slab;
	return NULL;
}


slab_header_t*
get_slab_of_buffer_by_slot(void* objp, int* what_is)
{
	kmem_cache_s* cachep;
	slab_header_t* tmp_slab;
	unsigned* begin_addr;

	unsigned mask = 0;

	for (int i = 0; i <= NUM_OF_BUFFERS; i++)
	{
		cachep = (kmem_cache_t*)slab_allocator + i;

		// Partial
		tmp_slab = cachep->partial; // mogui da ne
		while (tmp_slab)
		{
			begin_addr = tmp_slab->beginning_of_bits_array;

			for (int i = 0; i < tmp_slab->total_num_of_bits; i++)
			{
				for (int j = 0; j < 32; j++)
				{
					mask = 0x80000000 >> j;
					if (((mask & *((unsigned*)begin_addr + i)))) // Trazeni bit jeste 1, tj. zauzeta je lokacija
					{
						*what_is = 1;
						mask = rotate_bits_right(0x7fffffff, j);

						// Set that bit to 0, indicating that the certain slot is free for allocation again
						*((unsigned*)begin_addr + i) &= mask;

						tmp_slab->num_of_free_slots++;

						return tmp_slab;
					}
				}
			}
			tmp_slab = tmp_slab->next;
		}


		// Full
		tmp_slab = cachep->full;
		while (tmp_slab)
		{
			begin_addr = tmp_slab->beginning_of_bits_array;

			for (int i = 0; i < tmp_slab->total_num_of_bits; i++)
			{
				for (int j = 0; j < 32; j++)
				{
					mask = 0x80000000 >> j;
					if (((mask & *((unsigned*)begin_addr + i)))) // Trazeni bit jeste 1, tj. zauzeta je lokacija
					{
						*what_is = 2;
						mask = rotate_bits_right(0x7fffffff, j);

						// Set that bit to 0, indicating that the certain slot is free for allocation again
						*((unsigned*)begin_addr + i) &= mask;

						tmp_slab->num_of_free_slots++;

						return tmp_slab;
					}
				}
			}
			tmp_slab = tmp_slab->next;
		}


	}

	// There is no such object
	return NULL;
}


void
kmem_cache_free(kmem_cache_t* cachep, void* objp)
{
	WaitForSingleObject(slab_allocator->mutex, INFINITE);

	int what_is = 0;

	slab_header_t* slab = get_slab_by_slot(cachep, objp, &what_is);
	slab_header_t* cur = NULL;

	if (!what_is)
	{
		cachep->error_flags |= ERROR_UNABLE_TO_FIND_SLOT_IN_TYPE_CACHES;

		ReleaseMutex(slab_allocator->mutex);
		return NULL;
	}

	// Partial = 1;
	if (what_is == 1)
	{
		assert(slab);
		assert(cachep);
		// If partial is empty now, move it empty slab;
		if (slab->num_of_free_slots == cachep->num_of_slots_in_slab)
		{
			cur = cachep->partial;

			while (cur->next)
			{
				if (cur->next == slab)
					break;

				cur = cur->next;
			}

			if (cur == slab) 				      // Update list of partial slabs
				cachep->partial = cur->next;
			else if (cur->next == slab)			  // Update list of partial slabs	
				cur->next = cur->next->next;

			// Put in empty
			slab->next = cachep->empty;
			cachep->empty = slab;
		}
	}

	// Full = 2;
	if (what_is == 2)
	{
		cur = cachep->full;

		while (cur->next)
		{
			if (cur->next == slab)
				break;

			cur = cur->next;
		}

		if (cur == slab) 				      // Update list of full slabs
			cachep->full = cur->next;
		else if (cur->next == slab)			  // Update list of full slabs	
			cur->next = cur->next->next;


		if (cachep->num_of_slots_in_slab == 1)
		{
			slab->next = cachep->empty;
			cachep->empty = slab;
		}
		else
		{
			slab->next = cachep->partial;
			cachep->partial = slab;
		}

	}

	ReleaseMutex(slab_allocator->mutex);
}


void
kfree(const void* objp)
{
	WaitForSingleObject(slab_allocator->mutex, INFINITE);
	int what_is = 0;

	slab_header_t* slab = get_slab_of_buffer_by_slot(objp, &what_is);
	slab_header_t* cur = NULL;

	if (!what_is)
	{
		slab->cache_owner->error_flags |= ERROR_UNABLE_TO_FIND_SLOT_IN_BUF_CACHES;

		ReleaseMutex(slab_allocator->mutex);
		return;
	}

	kmem_cache_s* cachep = slab->cache_owner;


	// Partial = 1;
	if (what_is == 1)
	{
		assert(slab);
		// If partial is empty now, move it empty slab;
		if (slab->num_of_free_slots == cachep->num_of_slots_in_slab)
		{
			cur = cachep->partial;

			while (cur->next)
			{
				if (cur->next == slab)
					break;

				cur = cur->next;
			}
			buddy_free(buddy, slab, slab->cache_owner->slab_size_in_bytes);
			//if (cur == slab) 				      // Update list of partial slabs
			//	cachep->partial = cur->next;
			//else if (cur->next == slab)			  // Update list of partial slabs	
			//	cur->next = cur->next->next;

			//// Put in empty
			//slab->next = cachep->empty;
			//cachep->empty = slab;
		}
	}

	// Full = 2;
	if (what_is == 2)
	{
		cur = cachep->full;

		while (cur->next)
		{
			if (cur->next == slab)
				break;

			cur = cur->next;
		}

		if (cur == slab) 				      // Update list of full slabs
			cachep->full = cur->next;
		else if (cur->next == slab)			  // Update list of full slabs	
			cur->next = cur->next->next;



		/*if (cachep->num_of_slots_in_slab == 1)
		{
			slab->next = cachep->empty;
			cachep->empty = slab;
		}
		else
		{
			slab->next = cachep->partial;
			cachep->partial = slab;
		}*/

		// slab vrati badiju
		buddy_free(buddy, slab, slab->cache_owner->slab_size_in_bytes);

		
	}

	kmem_cache_info(cachep);

	ReleaseMutex(slab_allocator->mutex);
}


void
kmem_cache_info(kmem_cache_t* cachep) {

	WaitForSingleObject(slab_allocator->mutex, INFINITE);

	if (cachep == NULL)
	{
		ReleaseMutex(slab_allocator->mutex);

		return;
	}

	int num_of_slabs = 0;
	int num_of_slots = 0;
	int max_num_of_slots = 0;
	int free = 0;

	int empty = 0;
	int partial = 0;
	int full = 0;

	slab_header_t* tmp = cachep->empty;
	while (tmp)
	{
		free += tmp->num_of_free_slots;
		max_num_of_slots += cachep->num_of_slots_in_slab;
		num_of_slots += (cachep->num_of_slots_in_slab - tmp->num_of_free_slots);

		num_of_slabs++;
		tmp = tmp->next;

		empty++;
		num_of_slots++;
	}

	tmp = cachep->partial;

	while (tmp)
	{
		free += tmp->num_of_free_slots;
		max_num_of_slots += cachep->num_of_slots_in_slab;
		num_of_slots += (cachep->num_of_slots_in_slab - tmp->num_of_free_slots);

		num_of_slabs++;
		tmp = tmp->next;

		partial++;
		num_of_slots++;
	}

	tmp = cachep->full;
	while (tmp)
	{
		free += tmp->num_of_free_slots;
		max_num_of_slots += cachep->num_of_slots_in_slab;
		num_of_slots += (cachep->num_of_slots_in_slab - tmp->num_of_free_slots);

		num_of_slabs++;
		tmp = tmp->next;

		full++;
		
	}

	// total_size_of_slots_in_bytes
	int total = num_of_slabs * cachep->num_of_slots_in_slab;
	int taken = total - free;

	// x/100 * total = taken  => x = taken*100/total; 
	float percentage = (float)(taken * 100.00 / total);

	printf
	(
		"===================================\n"
		"============= CACHE ===============\n"
		"===================================\n"
		"\tNAME: %s\n"
		"===================================\n"
		"=> SLOT SIZE:		     %d_B\n"
		"=> TOTAL NUM OF SLOTS:	     %d/%d\n"
		"=> SLOTS PER SLAB:	     %d\n"
		"=> NUM OF EMPTY SLABS:       %d\n"
		"=> NUM OF PARTIAL SLABS:     %d\n"
		"=> NUM OF FULL SLABS:	     %d\n"
		"=> PERCENTAGE OF USED SPACE: %.2f %%\n"

		"===================================\n\n",

		cachep->name,
		cachep->size_of_slot,
		num_of_slots, max_num_of_slots,
		cachep->num_of_slots_in_slab,
		empty,
		partial,
		full - 1,
		percentage
	);

	ReleaseMutex(slab_allocator->mutex);
}


int
kmem_cache_shrink(kmem_cache_t* cachep)
{
	unsigned flag = 0;

	slab_header_t* tmp_slab = cachep->empty;
	slab_header_t* slab_to_free;
	buddy_block_t* block_to_free;

	WaitForSingleObject(slab_allocator->mutex, INFINITE);

	while (cachep->empty)
	{
		slab_to_free = tmp_slab;
		cachep->empty = slab_to_free->next;

		block_to_free = (buddy_block_t*)slab_to_free;

		// Free
		buddy_free(buddy, block_to_free, cachep->slab_size_in_bytes);

		cachep->error_flags |= SHRINKED;

		flag += cachep->slab_size_in_bytes;
	}

	if (flag)
	{
		ReleaseMutex(slab_allocator->mutex);
		return flag;
	}


	// There was not a single empty slab to shrink
	ReleaseMutex(slab_allocator->mutex);
	return 0;
}


void
kmem_cache_destroy(kmem_cache_t* cachep)
{

	slab_header_t* slab_to_free;
	buddy_block_t* block_to_free;

	WaitForSingleObject(slab_allocator->mutex, INFINITE);

	while (cachep->empty)
	{
		slab_to_free = cachep->empty;
		cachep->empty = slab_to_free->next;

		block_to_free = (buddy_block_t*)slab_to_free;

		// Free
		buddy_free(buddy, block_to_free, cachep->slab_size_in_bytes);
	}

	while (cachep->partial)
	{
		slab_to_free = cachep->partial;
		cachep->partial = slab_to_free->next;

		block_to_free = (buddy_block_t*)slab_to_free;

		// Free
		buddy_free(buddy, block_to_free, cachep->slab_size_in_bytes);
	}

	while (cachep->full)
	{
		slab_to_free = cachep->full;
		cachep->full = slab_to_free->next;

		block_to_free = (buddy_block_t*)slab_to_free;

		// Free
		buddy_free(buddy, block_to_free, cachep->slab_size_in_bytes);
	}

	set_free_cache(cachep);

	ReleaseMutex(slab_allocator->mutex);
}


void
print_all_buffer_caches()
{

	kmem_cache_s* cachep_1;

	for (int i = 0; i < NUM_OF_BUFFERS; i++)
	{
		cachep_1 = (kmem_cache_t*)slab_allocator + i;

		kmem_cache_info(cachep_1);
	}
}


int
kmem_cache_error(kmem_cache_t* cachep)
{
	print_error_code(cachep);
}


void
print_error_code(kmem_cache_t* cachep)
{
	uint8_t error_flags = cachep->error_flags;

	if (error_flags == 0)
		printf("EVERYTHING IS OK\n");
	
	if (error_flags & ERROR_NO_MEMORY)
		printf("ERROR_NO_MEMORY\n");

	if (error_flags & ERROR_SLAB_ALLOCATOR_FAIL)
		printf("ERROR_SLAB_ALLOCATOR_FAIL\n");

	if (error_flags & ERROR_BUDDY_ALLOCATOR_FAIL)
		printf("ERROR_BUDDY_ALLOCATOR_FAIL\n");

	if (error_flags & ERROR_ADDING_SLAB)
		printf("ERROR_ADDING_SLAB\n");

	if (error_flags & ERROR_UNABLE_TO_FIND_SLOT_IN_TYPE_CACHES)
		printf("ERROR_UNABLE_TO_FIND_SLOT_IN_TYPE_CACHES\n");

	if (error_flags & ERROR_UNABLE_TO_FIND_SLOT_IN_BUF_CACHES)
		printf("ERROR_UNABLE_TO_FIND_SLOT_IN_BUF_CACHES\n");

	if (error_flags & SHRINKED)
		printf("SHRINKED\n");

	if (error_flags & DESTROYED)
		printf("DESTROYED\n");
}




/*

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "slab.h"
#include "test.h"
#include "Buddy_allocator.h"
#include "Slab_allocator.h"

#define block (1)
#define niti (8)
#define BLOCK_NUMBER (1000 *  block)
#define THREAD_NUM (1 * niti) // granica za koliko niti rati
#define ITERATIONS (1000)
//#define ITERATIONS (8125) /// m
//#define THREAD_NUM (9) // gr

#define shared_size (7)


void construct(void *data) {
	static int i = 1;
	printf_s("%d Shared object constructed.\n", i++);
	memset(data, MASK, shared_size);
}

int check(void *data, size_t size) {
	int ret = 1;
	for (int i = 0; i < size; i++) {
		if (((unsigned char *)data)[i] != MASK) {
			ret = 0;
		}
	}

	return ret;
}

struct objects_s {
	kmem_cache_t *cache;
	void *data;
};

void work(void* pdata) {
	struct data_s data = *(struct data_s*) pdata;
	char buffer[1024];
	int size = 0;
	sprintf_s(buffer, 1024, "thread cache %d", data.id);
	kmem_cache_t *cache = kmem_cache_create(buffer, data.id, 0, 0);

	struct objects_s *objs = (struct objects_s*)(kmalloc(sizeof(struct objects_s) * data.iterations));

	for (int i = 0; i < data.iterations; i++) {

		if (i % 100 == 0) {
			objs[size].data = kmem_cache_alloc(data.shared);
			objs[size].cache = data.shared;
			assert(check(objs[size].data, shared_size));
		}
		else
		{
			objs[size].data = kmem_cache_alloc(cache);
			objs[size].cache = cache;
			memset(objs[size].data, MASK, data.id);

		}
		size++;
	}

	kmem_cache_info(cache);
	kmem_cache_info(data.shared);


	for (int i = 0; i < size; i++) {
		//printf("%d__\n", i);

		assert(check(objs[i].data, (cache == objs[i].cache) ? data.id : shared_size));
		kmem_cache_free(objs[i].cache, objs[i].data);
	}


	kfree(objs);
	kmem_cache_destroy(cache);
}

int main() {
	void *space = malloc(BLOCK_SIZE * BLOCK_NUMBER);


	kmem_init(space, BLOCK_NUMBER);
	kmem_cache_t *shared = kmem_cache_create("shared object", shared_size, construct, NULL);

	struct data_s data;
	data.shared = shared;
	data.iterations = ITERATIONS;
	run_threads(work, &data, THREAD_NUM);

	display_buddy();

	free(space);
	return 0;
}




*/



/*

//#include <Math.h>


#include "Slab_allocator.h"
#include <stdio.h>

#define BUDDY_BLOCK_SIZE 16

typedef struct nesto {
	buddy_block_t* blokic;
	struct nesto* next;
}nesto_t;

global = 999;

int
main()
{
	// Initialize, try 993 and allocated 2
	void* space = malloc(BLOCK_SIZE * NUMBER_OF_BLOCKS);
	//buddy_allocator_t* buddy = buddy_initialize(space, 1000);

	kmem_init(space, 1000);
	kmem_cache_t* shared = kmem_cache_create("shared object", 7, NULL, NULL);
	slab_header_t* slab = kmem_cache_alloc(shared);

	printf("nesto\n");

	// Display
	//buddy_display(buddy);

	// Allocation
	buddy_block_t* block = buddy_allocation(2*BLOCK_SIZE, buddy);
	global -= 2;
	buddy_display(buddy);

	// Allocation
	buddy_block_t* block_2 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	global -= 32;
	buddy_display(buddy);

	// Allocation
	buddy_block_t* block_3 = buddy_allocation(64 * BLOCK_SIZE, buddy);
	global -= 64;
	buddy_display(buddy);

	// Allocation
	buddy_block_t* block_5 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	global -= 32;
	buddy_display(buddy);

	// Allocation
	buddy_block_t* block_6 = buddy_allocation(0 * BLOCK_SIZE, buddy);
	buddy_display(buddy);

	buddy_block_t* block_7 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	buddy_block_t* block_8 = buddy_allocation(128 * BLOCK_SIZE, buddy);
	buddy_block_t* block_9 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	buddy_block_t* block_71 = buddy_allocation(32 * BLOCK_SIZE, buddy);

	// Free
	buddy_free(buddy, block, 2 * BLOCK_SIZE);
	global += 2;
	printf("\n Deallocated [ 2 ] Blocks!\n");

	// Display
	buddy_display(buddy);

	// Free
	buddy_free(buddy, block_2, 32 * BLOCK_SIZE);
	global += 32;
	printf("\n Deallocated [ 32 ] Blocks!\n");

	// Display
	buddy_display(buddy);

	// Free
	buddy_free(buddy, block_3, 64 * BLOCK_SIZE);
	global += 64;
	printf("\n Deallocated [ 64 ] Blocks!\n");

	// Display
	buddy_display(buddy);


	buddy_block_t* block_7 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	global -= 32;

	for (int i = 0; i < 936; i++) {

		if(buddy_allocation(1 * BLOCK_SIZE, buddy) != NULL)
			global--;
	}
	
	//buddy_block_t* block_11 = buddy_allocation(32 * BLOCK_SIZE, buddy);
	//buddy_block_t* block_8 = buddy_allocation(128 * BLOCK_SIZE, buddy);
	//buddy_block_t* block_9 = buddy_allocation(256 * BLOCK_SIZE, buddy);
	////buddy_block_t* block_71 = buddy_allocation(512 * BLOCK_SIZE, buddy);

	//buddy_block_t* block_74 = buddy_allocation(32 * BLOCK_SIZE, buddy);


	// Display
	buddy_display(buddy);

	printf("GLOBAL %d: \n", global);


	return 0;

}





*/