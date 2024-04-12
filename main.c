#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

typedef char ALIGN[16];

union header
{
    struct
    {
        size_t size;
        unsigned is_free;
        struct header_t *next;
    } s;
    ALIGN stub;
};

typedef union header header_t;

header_t *head, *tail;

pthread_mutex_t global_malloc_lock;

header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while (curr)
    {
        if (curr->s.is_free && curr->s.size >= size)
            return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void *rmalloc(size_t size)
{
    size_t total_size;
    void *block;
    if (!size)
        return NULL;

    pthread_mutex_lock(&global_malloc_lock);
    header_t *new_mem = get_free_block(size);
    if (new_mem)
    {
        new_mem->s.is_free = 0;
        pthread_mutex_unlock(&global_malloc_lock);
        return (void *)(new_mem + 1);
    }

    total_size = sizeof(header_t) + size;

    block = sbrk(total_size);
    if (block == (void *)-1)
    {
        pthread_mutex_unlock(&global_malloc_lock);
        return NULL;
    }
    new_mem = block;
    new_mem->s.size = size;
    new_mem->s.is_free = 0;
    new_mem->s.next = NULL;
    if (!head)
        head = new_mem;
    if (tail)
        tail->s.next = new_mem;
    tail = new_mem;
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(new_mem + 1);
}

void rfree(void *block)
{
    header_t *header, *tmp;
    void *pgbreak;
    if (!block)
        return;
    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t *)block - 1;

    pgbreak = sbrk(0);
    if ((char *)block + header->s.size == pgbreak)
    {
        if (head == tail)
        {
            head = tail = NULL;
        }
        else
        {
            tmp = head;
            while (tmp)
            {
                if (tmp->s.next == tail)
                {
                    tmp->s.next = NULL;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

void *rcalloc(size_t num, size_t nsize)
{
    size_t size;
    void *block;
    if (!num || !nsize)
        return NULL;
    size = num * nsize;
    if (nsize != size / num)
        return NULL;
    block = rmalloc(size);
    if (!block)
        return NULL;
    memset(block, 0, size);
    return block;
}

void *rrealloc(void *block, size_t size)
{
    header_t *header;
    void *ret;
    if (!block || !size)
    {
        return rmalloc(size);
    }
    header = (header_t *)block - 1;
    if (header->s.size >= size)
    {
        return block;
    }
    ret = rmalloc(size);
    if (ret)
    {
        memcpy(ret, block, header->s.size);
        rfree(block);
    }
    return ret;
}

int main()
{

    void *b = rmalloc(1024);
    printf("Memory address pointed to by ptr: %p\n", b);
    rfree(b);
    printf("Memory address pointed to by ptr: %p\n", b);

    return 0;
}
