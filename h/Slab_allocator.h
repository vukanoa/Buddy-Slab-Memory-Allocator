#pragma once

#include "slab.h"
#include "Buddy_allocator.h"
#include <Windows.h>

#define NAME_LENGTH (40)
#define MIN_BUF (5)
#define MAX_BUF (17)
#define NUM_OF_BUFFERS ((MAX_BUF) - (MIN_BUF) + (1))

// Error flags
#define ERROR_NO_MEMORY								(1)
#define ERROR_SLAB_ALLOCATOR_FAIL					(1 << 1)
#define ERROR_BUDDY_ALLOCATOR_FAIL					(1 << 2)
#define ERROR_ADDING_SLAB							(1 << 3)
#define ERROR_UNABLE_TO_FIND_SLOT_IN_TYPE_CACHES	(1 << 4)
#define ERROR_UNABLE_TO_FIND_SLOT_IN_BUF_CACHES 	(1 << 5)
#define SHRINKED									(1 << 6)
#define DESTROYED									(1 << 7)


#define NUMBER_OF_CACHES_IN_FIRST_BLOCK() ((sizeof(slab_allocator_t)) + sizeof(buddy));

typedef struct slab {

	uintptr_t beginning_slot;
	unsigned* beginning_of_bits_array;
	unsigned total_num_of_bits;
	unsigned levels_of_array_of_bits;

	size_t num_of_free_slots;
	size_t internal_fragmentation_in_bytes;

	kmem_cache_t* cache_owner;
	struct slab* next;

} slab_header_t;

typedef struct kmem_cache_s {
	char name[NAME_LENGTH];

	void(*ctor)(void*);
	void(*dtor)(void*);

	unsigned L1_offset;
	uint8_t error_flags;

	unsigned slab_size_in_bytes;
	unsigned num_of_slots_in_slab;
	unsigned size_of_slot;
	unsigned layer; // array of bits
	
	
	slab_header_t* empty;
	slab_header_t* partial;
	slab_header_t* full;

} kmem_cache_s;

typedef struct slab_allocator {
	kmem_cache_t small_buffer_caches[NUM_OF_BUFFERS];

	kmem_cache_t* cache_head;
	uint8_t* free_caches_array;

	HANDLE mutex;
} slab_allocator_t;


// Slab related
void small_buffer_caches_init();
void* add_slab(kmem_cache_t* cachep);
slab_header_t* get_slab_by_slot(kmem_cache_t* cachep, void* obj, int* what_is);




// Slab helpers
kmem_cache_t* give_free_cache();
int set_free_cache(kmem_cache_t* cachep);




// Helpers
int degre_counter(size_t size);
void print_bits(unsigned int num);
unsigned int rotate_bits_right(unsigned int value, int shift);
void print_array_of_free_caches();
void print_all_buffer_caches();
void print_error_code(kmem_cache_t* cachep);
void slab_display();
void display_buddy();


