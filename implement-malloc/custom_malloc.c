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
void* custom_malloc(size_t size)
{
    if(size <=0) return NULL;
/*
    Address:  1000                     1024

          |------------------------|------------------------|
Memory:   |   Header (Metadata)    |    Payload (Data)      |

          |------------------------|------------------------|
Pointers: ^                        ^
          curr                     curr + 1

*/
    Block* curr = memory_list; 

    //search for free block 
    while(curr)
    {
        if(curr->free && curr->size >= size)
        {

            curr->free = 0; 
            return (void*)(curr + 1); // payload data 
        }
        curr = curr->next;
    }
    printf(" calling sbrk\n");
    //free block not found, request memory from the OS 
    size_t total_size = BLOCK_SIZE + size; //metadata + payload 
    curr = (Block*)sbrk(total_size);
    if(curr == (Block*)-1) return NULL; //sbrk failed

    curr->size = size;
    curr->free = 0;
    curr->next = memory_list; //add to head of list
    memory_list = curr;

    return (void*)(curr +1);
}

void custom_free(void *ptr)
{
    if (!ptr) return;

    // Get the header by shifting the pointer back
    Block* curr = (Block*)ptr - 1;
    curr->free = 1;
}

static void print_result(const char *name, int pass)
{
    printf("%s: %s\n", name, pass ? "PASS" : "FAIL");
}

int main()
{
    printf("size of block : %zu\n", BLOCK_SIZE);

    unsigned char *a = (unsigned char*)custom_malloc(HEAP_SIZE/2);
    int pass1 = a != NULL;
    if (pass1) {
        for (size_t i = 0; i < HEAP_SIZE/2; ++i) {
            a[i] = (unsigned char)(i + 1);
        }
        for (size_t i = 0; i < HEAP_SIZE/2; ++i) {
            pass1 &= (a[i] == (unsigned char)(i + 1));
        }
    }
    
    print_result("allocate 512 bytes and verify content", pass1);

    unsigned char *b = (unsigned char*)custom_malloc(64);
    int pass2 = b != NULL && b != (void*)a;
    if (pass2) {
        for (size_t i = 0; i < 64; ++i) {
            b[i] = (unsigned char)(0xA0 + i);
        }
        for (size_t i = 0; i < 64; ++i) {
            pass2 &= (b[i] == (unsigned char)(0xA0 + i));
        }
    }
    print_result("allocate 64 bytes and verify content", pass2);

    custom_free(a);
    unsigned char *c = (unsigned char*)custom_malloc(16);
    int pass3 = (c == a);
    if (pass3) {
        for (size_t i = 0; i < 16; ++i) {
            c[i] = (unsigned char)(0xF0 + i);
        }
        for (size_t i = 0; i < 16; ++i) {
            pass3 &= (c[i] == (unsigned char)(0xF0 + i));
        }
    }
    print_result("reuse freed block and verify content", pass3);

    custom_free(NULL);
    print_result("free(NULL) is no-op", 1);

    void *d = custom_malloc(0);
    print_result("allocate 0 bytes returns NULL", d == NULL);

    // Cleanup remaining blocks for completeness
    custom_free(b);
    custom_free(c);

    printf("exit\n");

    return 0;
}