#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <fcntl.h>
#include <assert.h>

/*
    Author:     goudan 
    Name  :     Easy_ThreadPool
    Time  :     Thu 14 Apr 2022 14:49:32 -4:00
*/

// task node
struct Task{
    void* (*callback_function)(void* arg);  // callback function
    void* arg;                              // arg
    struct Task* next;                      // next node
};

// thread pool
struct ThreadPool{
    int thread_number;                      // The number of this TreadPool
    int queue_max_num;                      // The Max number of Task Queue
    struct Task* head;                      // The Link Queue of Task
    struct Task* tail;

    pthread_t* pthread_ids;                 // All id of threads (Array)
    pthread_mutex_t mutex_lock;            // The lock of Manage Thread

    // the condition locks
    pthread_cond_t queue_empty;             // empty lock
    pthread_cond_t queue_not_empty;         // not empty lock
    pthread_cond_t queue_not_full;          // not full lock

    // the status of poll
	int queue_task_num;                     // The number of Task currently 
	int queue_close;                        // Whether the queue had been closeed
	int pool_close;                         // Whether the ThreadPool had been closed
};

struct ThreadPool* ThreadPool_init(int thread_num, int max_task_queue_num);
void* ThreadPool_Function(void *arg);
int ThreadPool_Add_Task(struct ThreadPool* poll,void* (*callback_function)(void* arg),void* arg);
int ThreadPool_destroy(struct ThreadPool* poll);

// the thread real use function
void* work(void* arg){
	char* p = (char*)arg;
	printf("Thread real Task function:%s\n",p);
	sleep(1);
}

// Test Process Start Here
int main(){
    struct ThreadPoll* pool = ThreadPool_init(10, 15);
    ThreadPool_Add_Task(pool, work, "Task 1");
    ThreadPool_Add_Task(pool, work, "Task 2");
    ThreadPool_Add_Task(pool, work, "Task 3");
    ThreadPool_Add_Task(pool, work, "Task 4");
    ThreadPool_Add_Task(pool, work, "Task 5");
    ThreadPool_Add_Task(pool, work, "Task 6");
    ThreadPool_Add_Task(pool, work, "Task 7");
    ThreadPool_Add_Task(pool, work, "Task 8");
    ThreadPool_Add_Task(pool, work, "Task 9");
    ThreadPool_Add_Task(pool, work, "Task 10");
    ThreadPool_Add_Task(pool, work, "Task 11");

    sleep(10);
	ThreadPool_destroy(pool);
	return 0;
}


// Initial the ThreadPoll Function
struct ThreadPool* ThreadPool_init(int thread_num, int max_task_queue_num){
    struct ThreadPool* pool = NULL;
    pool = malloc(sizeof(struct ThreadPool));
    if(pool == NULL) {
        printf("Create poll failed: malloc return NULL\n");
        return NULL;
    }
    // set initial value
    pool->pthread_ids       = malloc(sizeof(pthread_t)*thread_num);
    pool->thread_number     = thread_num;
    pool->queue_max_num     = max_task_queue_num;
    pool->queue_task_num    = 0;                // Current Task num set 0
    pool->head = pool->tail = NULL;             // Task Queue initial status

    // mutex lock
    pthread_mutex_init(&(pool->mutex_lock), NULL);
    // condition lock
    pthread_cond_init(&(pool->queue_empty), NULL);
    pthread_cond_init(&(pool->queue_not_empty), NULL);
    pthread_cond_init(&(pool->queue_not_full), NULL);

    // the status of poll initalized
    pool->pool_close = pool->queue_close = 0;
    
    // the thread id queue initalized
    for(int i=0;i<pool->thread_number;i++){
        // create every thread and transfor the poll class to the function
        pthread_create(&(pool->pthread_ids[i]), NULL, ThreadPool_Function, (void*)pool);
    }
    return pool;
}

// ThreadPoll_Thread Callback Fucntion
void* ThreadPool_Function(void *arg){
    // arg(poll) save to use check the status of pool
    struct ThreadPool* pool = (struct ThreadPool*) arg;
    struct Task* pTask = NULL;
    // thread function
    while(1){
        // MutexLock lock
        pthread_mutex_lock(&(pool->mutex_lock));
        // check the status of pool
        while((pool->queue_task_num == 0) && !pool->pool_close){
            // while the queue is empty the thread wait
            pthread_cond_wait(&(pool->queue_not_empty), &(pool->mutex_lock));
        }
        // The ThreadPoll closed , thread exit
        if(pool->pool_close){
            pthread_mutex_unlock(&(pool->mutex_lock));
			pthread_exit(NULL);
        }
        // Process Task  current leave queue
        pool->queue_task_num--;
        // Get Task Queue head 
        pTask = pool->head;
        if(pool->queue_task_num == 0){
            pool->head = pool->tail = NULL;
        }else{
            pool->head = pTask->next;
        }
        // When the size of queue of Task is zero post the queue_empty cond siganl lock
        if(pool->queue_task_num == 0){
            pthread_cond_signal(&(pool->queue_empty));
        }
        // When the size of queue of Task is less than queue_max_num post the queue_not_full cond siganl lock
        if(pool->queue_task_num == pool->queue_max_num){
            pthread_cond_broadcast(&(pool->queue_not_full));
        }
        pthread_mutex_unlock(&(pool->mutex_lock));

        // real work -> to process Task callback function
        (*(pTask->callback_function) )(pTask->arg);
		free(pTask);
		pTask=NULL;
    }
}


// Add the Task to ThreadPool's Task Queue
int ThreadPool_Add_Task(struct ThreadPool* pool,void* (*callback_function)(void* arg),void* arg){
    // Check args
    assert(pool!=NULL);
    assert(callback_function!=NULL);
    assert(arg!=NULL);
    
    // Check ThreadPool status
    pthread_mutex_lock(&(pool->mutex_lock));
    while((pool->queue_max_num == pool->queue_task_num)&&!(pool->pool_close||pool->queue_close)){
        pthread_cond_wait(&(pool->queue_not_full), &(pool->mutex_lock));
    }
    if(pool->pool_close||pool->queue_close){
        pthread_mutex_unlock(&(pool->mutex_lock));
        return -1;
    }

    // Add Task to ThreadPool's Task Queue
    struct Task* task = NULL;
    task = malloc(sizeof(struct Task));
    if(task == NULL){
        pthread_mutex_unlock(&(pool->mutex_lock));
        printf("Create Task failed: malloc failed");
        return -1;
    }
    task->callback_function = callback_function;
    task->arg = arg;
    task->next = NULL;

    if(pool->head==NULL){
        pool->head = pool->tail = task;
        pthread_cond_broadcast(&(pool->queue_not_empty));   //tell the mutelock the queue is not empty
    }else{
        pool->tail->next = task;
        pool->tail = task;
    }
    pool->queue_task_num ++;
    pthread_mutex_unlock(&(pool->mutex_lock));
	return 0;
}

// Destory the ThreadPoll
int ThreadPool_destroy(struct ThreadPool* poll){

}
