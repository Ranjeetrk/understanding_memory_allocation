#define _DEFAULT_SOURCE
#include<stddef.h> // for size_t 
#include<stdint.h> // for intptr_t
#include<unistd.h> //for sbrk on linux
#include<stdio.h>
#include<stdlib.h> // for malloc

// Mock sbrk implementation for Windows/MinGW using static heap
#define HEAP_SIZE (1024 * 1024)  // 1MB heap
static char heap[HEAP_SIZE];
static char* break_ptr = heap;

void *sbrk(intptr_t increment) {
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

typedef struct Block {
    size_t size;
    int free;
    struct Block *next;
} Block;

#define BLOCK_SIZE sizeof(struct Block)
Block* memory_list = NULL;

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
        Block* new_block = (Block*)((char*)(block + 1) + size);
        new_block->size = block->size - size - BLOCK_SIZE;
        new_block->free = 1;
        new_block->next = block->next;
        block->next = new_block;
        block->size = size;
    }
}

// Function to merge adjacent free blocks
void merge_blocks() {
    Block* curr = memory_list;
    while (curr && curr->next) {
        if (curr->free && curr->next->free) {
            curr->size += BLOCK_SIZE + curr->next->size;
            curr->next = curr->next->next;
        } else {
            curr = curr->next;
        }
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
    Block* new_block = (Block*)sbrk(total_size);
    if (new_block == (Block*)-1) return NULL;

    new_block->size = size;
    new_block->free = 0;
    new_block->next = memory_list;
    memory_list = new_block;

    return (void*)(new_block + 1);
}

void custom_free(void *ptr) {
    if (!ptr) return;

    Block* block = (Block*)ptr - 1;
    block->free = 1;

    // Merge adjacent free blocks
    merge_blocks();
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