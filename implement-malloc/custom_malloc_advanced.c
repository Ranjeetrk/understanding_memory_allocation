#define _DEFAULT_SOURCE
#include<stddef.h> // for size_t 
#include<stdint.h> // for intptr_t
#include<unistd.h> //for sbrk on linux
#include<stdio.h>
#include<stdlib.h> // for malloc

typedef struct Block {
    size_t size;
    int free;
    struct Block *next;
    struct Block *prev;
} Block;

#define BLOCK_SIZE sizeof(struct Block)
Block* memory_list = NULL;
Block* memory_tail = NULL; // Tail pointer for O(1) insertion

// Mock sbrk implementation for Windows/MinGW using static heap
#define HEAP_SIZE (1024 * 1024)  // 1MB heap
static char heap[HEAP_SIZE];
static char* break_ptr = heap;

void *custom_sbrk(intptr_t increment) {
    if (increment == 0) {
        return break_ptr;
    }
    
    if (break_ptr + increment > heap + HEAP_SIZE) {
        return (void*)-1;  // Out of memory
    }
    
    char* old_break = break_ptr;
    break_ptr += increment;
    
    return old_break;
}

// Function to find the best fit free block
Block* find_best_fit(size_t size) {
    Block* curr = memory_list;
    Block* best = NULL;
    while (curr) {
        if (curr->free && curr->size >= size) {
            if (!best || curr->size < best->size) {
                best = curr;
            }
        }
        curr = curr->next;
    }
    return best;
}

// Function to split a block if it's larger than needed
void split_block(Block* block, size_t size) {
    if (block->size >= size + BLOCK_SIZE + 1) {  // Ensure enough space for header + at least 1 byte
        //This skips the metadata header of the current block and 
        //points to the beginning of the actual data payload that the user uses.
        Block* new_block = (Block*)((char*)(block + 1) + size);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->free = 1;
        new_block->next = block->next;
        new_block->prev = block;
        if(block->next)
            block->next->prev = new_block;
        else
            memory_tail = new_block;
        
        block->next = new_block;
        block->size = size;
    }
}

void* custom_malloc(size_t size) {
    if (size <= 0) return NULL;

    // First, try to find and reuse a free block
    Block* block = find_best_fit(size);
    if (block) {
        block->free = 0;
        split_block(block, size);
        return (void*)(block + 1);
    }

    // No suitable free block, request new memory
    size_t total_size = BLOCK_SIZE + size;
    Block* new_block = (Block*)custom_sbrk(total_size);
    if (new_block == (Block*)-1) return NULL;

    new_block->size = size;
    new_block->free = 0;
    new_block->next = NULL;
    new_block->prev = NULL;

    if(memory_list == NULL)
    {
        memory_list = new_block;
    }else{
        memory_tail->next = new_block;
        new_block->prev = memory_tail;
    }
    memory_tail = new_block;
   
    return (void*)(new_block + 1);
}

void custom_free(void *ptr) {
    if (!ptr) return;

    // Standard metadata retrieval: back up one Block size from user pointer [cite: 1, 3]
    Block* block = (Block*)ptr - 1;
    block->free = 1;

    // 1. Merge with NEXT block if it's free
    if(block->next && block->next->free)
    {
        // IMPORTANT: Add BLOCK_SIZE because that header is now usable data
        block->size = block->size + block->next->size + BLOCK_SIZE;
        block->next = block->next->next;
        if(block->next)
            block->next->prev = block;
        else
            memory_tail = block;

    }

    // 2. Merge with PREVIOUS block if it's free
    if(block->prev && block->prev->free)
    {
        if(block == memory_tail)
            memory_tail = block->prev;

        block->prev->size = block->prev->size + block->size + BLOCK_SIZE; 
        block->prev->next = block->next;
        if(block->next)
            block->next->prev = block->prev;
    }
}

static void print_result(const char *name, int pass) {
    printf("%s: %s\n", name, pass ? "PASS" : "FAIL");
}

int main() {
    printf("size of block : %zu\n", BLOCK_SIZE);

    // Test allocations
    void *a = custom_malloc(100);
    print_result("allocate 100 bytes", a != NULL);

    void *b = custom_malloc(200);
    print_result("allocate 200 bytes", b != NULL);

    custom_free(a);
    void *c = custom_malloc(50);  // Should reuse the freed 100-byte block
    print_result("reuse freed block", c == a);

    void *d = custom_malloc(150);  // Should fit in remaining space after split
    print_result("allocate 150 bytes", d != NULL);

    custom_free(b);
    custom_free(d);
    // After freeing, merging should consolidate free space

    void *e = custom_malloc(300);  // Should use merged space
    print_result("allocate 300 bytes after merge", e != NULL);

    custom_free(c);
    custom_free(e);

    printf("All tests completed.\n");
    return 0;
}