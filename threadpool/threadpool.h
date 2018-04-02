#ifndef _THREADPOOL_
#define _THREADPOOL_
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
typedef struct task
{
  void *(*func)(void *);
  void *arg;
  struct task *next;
} thread_task;
typedef struct 
{
  /* queue_lock ans queue_ready are used for 
  thread-safelya accesing varibles about task queue*/
  pthread_mutex_t queue_lock; 
  pthread_cond_t queue_ready;
  thread_task *queue_head;
  /* created tids by main thread */
  pthread_t *tids;
  /* 1 indicates user want to destroy the thread pool */
  int shutdown;
  int max_thread_num;
  int cur_queue_size;
}thread_pool;

int pool_init(thread_pool *pool, int max_thread_num);
int pool_add_task(thread_pool *pool, void *(*func)(void *), void *arg);
int pool_destroy(thread_pool *pool);
void *thread_main(void *arg);

/******************************************************/
/* Description: initialize a thread pool.
/* Parameters: 
/* pool is a thread pool to be initialized.
/* max_thread_num is the number of threads in thread poo
/* -l.
/* Return Value: On success, 0 is returned. */
/******************************************************/
int pool_init(thread_pool *pool, int max_thread_num)
{
    pthread_mutex_init(&(pool->queue_lock), NULL);
    pthread_cond_init(&(pool->queue_ready), NULL);

    pool->queue_head = NULL;
    pool->shutdown = 0;
    pool->max_thread_num = max_thread_num;
    pool->cur_queue_size = 0;
    pool->tids = (pthread_t*)malloc(sizeof(pthread_t)*max_thread_num);
    
    for(int i=0; i<max_thread_num; ++i)
        pthread_create(&(pool->tids[i]), NULL, &thread_main, (void*) pool);
    return 0;
}

/******************************************************/
/* Description: function executed by initialized thread.
/* Parameters: 
/* void_pool is a thread pool to be initialized. */
/******************************************************/
void *thread_main(void *void_pool)
{
    thread_pool *pool = (thread_pool*)void_pool;
    for(;;)
    {
        pthread_mutex_lock(&(pool->queue_lock));
        while(pool->cur_queue_size == 0 && !pool->shutdown)
        {
            pthread_cond_wait(&(pool->queue_ready), &(pool->queue_lock));
        }
        if(pool->shutdown)
        {
            pthread_mutex_unlock(&(pool->queue_lock));
            pthread_exit(NULL);
        }
        pool->cur_queue_size--;
        thread_task *task = pool->queue_head;
        pool->queue_head = task->next;
        pthread_mutex_unlock(&(pool->queue_lock));
        (*(task->func))(task->arg);
        free(task);
     }
     pthread_exit(NULL);
}

/******************************************************/
/* Description: add a task into the task queue.
/* Parameters: 
/* pool is a thread pool to be initialized.
/* function is the task to be process.
/* arg is the arguments of function.
/* Return Value: On success, 0 is returned.  */
/******************************************************/
int pool_add_task(thread_pool *pool, void *(*func)(void *), void *arg)
{
    thread_task* task = (thread_task*)malloc(sizeof(thread_task));
    task->func = func;
    task->arg = arg;
    task->next = NULL;
    pthread_mutex_lock(&(pool->queue_lock));
    thread_task *cur = pool->queue_head;
    if(cur != NULL)
    {
        while(cur->next != NULL)
            cur=cur->next;
        cur->next = task;
    }
    else 
        pool->queue_head = task;
    ++pool->cur_queue_size;
    pthread_mutex_unlock(&(pool->queue_lock));
    pthread_cond_signal(&(pool->queue_ready));
    return 0;
}

/******************************************************/
/* Description: initialize a thread pool.
/* Parameters: 
/* pool is a thread pool to be initialized.
/* Return Value: On success, 0 is returned. */
/******************************************************/
int pool_destroy(thread_pool *pool)
{
    if(pool->shutdown) return -1;
    pool->shutdown = 1;
    pthread_cond_broadcast(&(pool->queue_ready));
    for(int i=0; i<pool->max_thread_num; ++i)
        pthread_join(pool->tids[i], NULL);
    free(pool->tids);

    thread_task *cur = NULL;
    while(pool->queue_head != NULL)
    {
        cur = pool->queue_head;
        pool->queue_head = cur->next;
        free(cur);
    }
    pthread_mutex_destroy(&(pool->queue_lock));
    pthread_cond_destroy(&(pool->queue_ready));
    return 0;
}


#endif
