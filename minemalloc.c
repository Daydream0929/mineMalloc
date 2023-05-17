#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>

typedef char ALIGN[16];

union header {
    struct {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub;
};

typedef union header header_t;

header_t *head = NULL, *tail = NULL;
pthread_mutex_t gloabl_malloc_lock;

// from the head to find a enough memory to fit the size
header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while (curr) {
        if (curr->s.is_free && curr->s.size >= size) 
            return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void *malloc(size_t size)
{
    size_t total_size;
    void *block;
    header_t *header;
    
    // if size == 0, we don't need to malloc anything
    if (!size) 
        return NULL;

    // lock the global_malloc_lock;
    pthread_mutex_lock(&gloabl_malloc_lock);

    header = get_free_block(size);
    if (header) { // if there has a enough memory
        header->s.is_free = 0;
        pthread_mutex_unlock(&gloabl_malloc_lock);
        return (void*)(header + 1);
    }

    // else there has no enough memory
    total_size = sizeof(header_t) + size;

    /* 
    Calling sbrk(0) gives the current address of program break.
    Calling sbrk(x) with a positive value increments brk by x bytes, as a result allocating memory.
    Calling sbrk(-x) with a negative value decrements brk by x bytes, as a result releasing memory.
    On failure, sbrk() returns (void*) -1.
    */
    block = sbrk(total_size); 
    if (block == (void*) -1) { // if sbrk() failure
        pthread_mutex_unlock(&gloabl_malloc_lock);
        return NULL;
    }

    // else if sbrk() success ------> new a header
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    header->s.next = NULL;

    if (!head)
        head = header;
    if (tail) 
        tail->s.next = header;
    tail = header;
    pthread_mutex_unlock(&gloabl_malloc_lock);
    return (void*)(header + 1);
}

/*
free() has to first deterimine if the block-to-be-freed is at the end of the heap. 
If it is, we can release it to the OS. Otherwise, all we do is mark it ‘free’, hoping to reuse it later.
*/
void free(void *block)
{
    header_t *header, *tmp;
    void *programbreak;

    if (!block)     
        return;
    
    pthread_mutex_lock(&gloabl_malloc_lock);
    header = (header_t*)block - 1;

    programbreak = sbrk(0);

    // at the end of the heap
    if ((char*)block + header->s.size == programbreak) {
        if (head == tail) {
            head = tail = NULL;
        } else {
            tmp = head;
            while (tmp) {
                if (tmp->s.next == tail) {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&gloabl_malloc_lock);
        return;
    }

    // not at the end of the heap
    header->s.is_free = 1;
    pthread_mutex_unlock(&gloabl_malloc_lock);
}

/*
The calloc(num, nsize) function allocates memory for an array of num elements of nsize bytes each and returns a pointer to the allocated memory.
Additionally, the memory is all set to zeroes.
*/
void *calloc(size_t num, size_t nsize)
{
    size_t size;
    void *block;
    if (!num || !nsize)
        return NULL;
    size = num * nsize;
    // check mul overflow
    if (num != size / nsize)
        return NULL;
    memset(block, 0, size);
    return block;
}

/*
realloc() changes the size of the given memory block to the size given.
*/
void *realloc(void *block, size_t size)
{
    header_t *header;
    void *ret;
    if (!block || !size)
        return malloc(size);
    header = (header_t*)block - 1;
    if (header->s.size >= size)
        return block;
    ret = malloc(size);
    if (ret) {
        memcmp(ret, block, header->s.size);
        free(block);
    }
    return ret;
}

void print_mem_list()
{
	header_t *curr = head;
	printf("head = %p, tail = %p \n", (void*)head, (void*)tail);
	while(curr) {
		printf("addr = %p, size = %zu, is_free=%u, next=%p\n",
			(void*)curr, curr->s.size, curr->s.is_free, (void*)curr->s.next);
		curr = curr->s.next;
	}
}

int main(int argc, char* argv[]) 
{
    print_mem_list();
    return 0;
}