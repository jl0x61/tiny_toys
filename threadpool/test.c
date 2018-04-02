#include "threadpool.h"
#include <stdio.h>
#include <unistd.h>

void *f(void *arg)
{
    printf("tid is %lu, arg is %d\n", pthread_self(), *(int*)arg);
    sleep(1);
    return NULL;
}

int main()
{
    thread_pool *pool = (thread_pool*)malloc(sizeof(thread_pool));
    pool_init(pool, 3);
    int* arg = (int*)malloc(sizeof(int)*10);
    for(int i=0; i<10; ++i)
    {
        arg[i]=i;
        pool_add_task(pool, f, &arg[i]);
    }
    sleep(5);
    pool_destroy(pool);
    free(pool);
}
