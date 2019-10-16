#include <unistd.h>
#include <assert.h>
#include <string.h>

#define internal static

internal void *global_block_head = 0;

typedef struct {
    size_t size;
    void *next;
    int free;
    int magic;    
    char data[0];
} BlockHdr;

internal BlockHdr *find_free_block(BlockHdr **last, size_t size) {
    BlockHdr *current = (BlockHdr *)global_block_head;
    while (current && !(current->free && current->size >= size)){
        *last = current;
        current = current->next;
    }
    return current;
}

internal BlockHdr *request_space(BlockHdr *last, size_t size) {
    BlockHdr *block = (BlockHdr *)sbrk(0);
    void *requested = sbrk(size + sizeof(BlockHdr));
    assert(requested == (void *)block);
    if (requested == (void *)-1) {
        return 0;
    }
    if (last) {
        last->next = block;
    }
    block->size = size;
    block->free = 0;
    block->next = 0;
    block->magic = 0x12345678;
    return block;
}

void split_block(BlockHdr *block, size_t size) {
    BlockHdr *new_block = block->data + size;
    new_block->size = block->size - size - sizeof(BlockHdr);
    new_block->magic = 0x12345678;
    new_block->next = block->next;
    new_block->free = 1;
    block->size = size;
    block->next = new_block;
}

void *allocate(size_t size) {
    BlockHdr *block;
    if (size <= 0) return 0;
    if (!global_block_head) {
        block = request_space(0, size);
        if (!block) return 0;
        global_block_head = block;
    } else {
        BlockHdr *last = global_block_head;
        block = find_free_block(&last, size);
        if (!block) {
            block = request_space(last, size);
            if (!block) return 0;
        } else {
            if (block->size > size) {
                split_block(block, size);
            }
            block->free = 0;
            block->magic = 0x77777777;
        }
    }
    return block->data;
}

BlockHdr *get_block_header(void *ptr) {
    return (BlockHdr *)ptr - 1;
}

void deallocate(void *ptr) {
    if (!ptr) return;

    BlockHdr *block = get_block_header(ptr);
    assert(block->size > 0);
    assert(block->free == 0);
    assert(block->magic == 0x12345678 || block->magic == 0x77777777);
    block->free = 1;
    block->magic = 0x55555555;
}

void *reallocate(void *ptr, size_t size) {
    if (!ptr) {
        return allocate(size);
    }
    BlockHdr *block = get_block_header(ptr);
    if (block->size >= size) {
        return ptr;
    }

    void *new_ptr = allocate(size);
    if (!new_ptr) return 0;
    memcpy(new_ptr, ptr, block->size);
    deallocate(ptr);
    return new_ptr;
}

void *callocate(size_t nelem, size_t elsize) {
    size_t size = nelem * elsize;
    void *ptr = allocate(size);
    memset(ptr, 0, size);
    return ptr;
}

void allocate_test() {
    int n = 10;
    int n_bytes = n * sizeof(int);
    int *arr1 = allocate(n_bytes);
    for (int i = 0; i < n; ++i) {
        arr1[i] = i + 1;
    }
    int sum = 0;
    for (int i = 0; i < n; ++i) {
        sum += arr1[i];
    }
    int *arr2 = allocate(n_bytes * 2);
    for (int i = 0; i < 2*n; ++i) {
        arr2[i] = i + 1;
    }
    int sum2 = 0;
    for (int i = 0; i < 2*n; ++i) {
        sum2 += arr2[i];
    }
    assert(sum == (n*(n+1)/2));
    assert(sum2 == (n*(2*n+1)));
    deallocate(arr1);
    int *arr3 = allocate(n_bytes);
    assert(arr3 == arr1);
}

int main() {
    allocate_test();
    return 0;
}