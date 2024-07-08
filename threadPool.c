#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include "threadPool.h"
#include <string.h>
// 任务结构体
#define NUMBER 10
typedef struct Task
{
    void (*function)(void* arg); //任务
    void* arg;
}Task;

// 线程池结构体
struct ThreadPool
{
    // 任务队列
    Task* taskQ;        //任务队列
    int queueCapacity;  // 容量
    int queueSize;      // 当前任务个数
    int queueFront;     // 队头 -> 取数据
    int queueRear;      // 队尾 -> 放数据

    pthread_t managerID;    // 管理者线程ID
    pthread_t *threadIDs;   // 工作的线程ID
    int minNum;             // 最小线程数量
    int maxNum;             // 最大线程数量
    int busyNum;            // 忙的线程的个数
    int liveNum;            // 存活的线程的个数
    int exitNum;            // 要销毁的线程个数
    pthread_mutex_t mutexPool;  // 锁整个的线程池
    pthread_mutex_t mutexBusy;  // 锁busyNum变量
    pthread_cond_t notFull;     // 任务队列是不是满了
    pthread_cond_t notEmpty;    // 任务队列是不是空了

    int shutdown;           // 是不是要销毁线程池, 销毁为1, 不销毁为0
};
ThreadPool *threadPoolCreate(int min, int max, int queueSize){
    ThreadPool *pool = (ThreadPool*) malloc(sizeof(ThreadPool));
    do {
        if(pool == NULL){
            printf("malloc ThreadPool fail...\n");
            break;
        }
        pool->threadIDs = (pthread_t*) malloc(sizeof(pthread_t) * max);
        if(pool->threadIDs == NULL){
            printf("malloc threadIDs fail...\n");
            break;
        }
        memset(pool->threadIDs, 0, sizeof(pthread_t) * max);
        pool->minNum = min;
        pool->maxNum = max;
        pool->busyNum = 0;
        pool->liveNum = min;    // 和最小个数相等
        pool->exitNum = 0;

        if(pthread_mutex_init(&pool->mutexPool,NULL) != 0 ||
           pthread_mutex_init(&pool->mutexBusy,NULL) != 0 ||
           pthread_cond_init(&pool->notEmpty,NULL) != 0||
           pthread_cond_init(&pool->notFull,NULL) != 0){
            printf("mutex or cond init fail...\n");
            break;
        }
        //初始化任务队列
        pool->taskQ = (Task*) malloc(sizeof(Task) * queueSize);
        pool->queueCapacity = queueSize;
        pool->queueSize = 0;
        pool->queueFront = 0;
        pool->queueRear = 0;

        pool->shutdown = 0;
        //管理者线程创建
        pthread_create(&pool->managerID, NULL, manager, pool);
        for(int i = 0;i < min; ++i){
            pthread_create(&pool->threadIDs[i], NULL, worker, pool);
        }
        return pool;
    }while(0);
    //释放
    if(pool && pool->threadIDs) free(pool->threadIDs);
    if(pool && pool->taskQ) free(pool->taskQ);
    if(pool) free(pool);
    return NULL;
}

int threadPoolDestroy(ThreadPool *pool) {
    if(pool == NULL){
        return -1;
    }

    pool->shutdown = 1;
    //回收管理者线程
    pthread_join(pool->managerID,NULL);
    //阻塞回收
    for (int i = 0; i < pool->liveNum; ++i) {
        pthread_cond_signal(&pool->notEmpty);
    }
    if(pool->taskQ){
        free(pool->taskQ);
    }
    if(pool->threadIDs){
        free(pool->threadIDs);
    }

    pthread_mutex_destroy(&pool->mutexPool);
    pthread_mutex_destroy(&pool->mutexBusy);
    pthread_cond_destroy(&pool->notEmpty);
    pthread_cond_destroy(&pool->notFull);
    free(pool);
    pool = NULL;
    return 0;
}

void threadPoolAdd(ThreadPool *pool, void (*func)(void *), void *arg) {
    pthread_mutex_lock(&pool->mutexPool);
    while (pool->queueSize == pool->queueCapacity && !pool->shutdown) {
        pthread_cond_wait(&pool->notFull, &pool->mutexPool);
    }
    if(pool->shutdown){
        pthread_mutex_unlock(&pool->mutexPool);
        return;
    }
    pool->taskQ[pool->queueRear].function = func;
    pool->taskQ[pool->queueRear].arg = arg;
    pool->queueRear = (pool->queueRear+1) % pool->queueCapacity;
    pool->queueSize++;

    pthread_cond_signal(&pool->notEmpty);

    pthread_mutex_unlock(&pool->mutexPool);
}

int threadPoolBusyNum(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutexBusy);
    int busyNum = pool->busyNum;
    pthread_mutex_unlock(&pool->mutexBusy);
    return busyNum;
}

int threadPoolAliveNum(ThreadPool *pool) {
    pthread_mutex_lock(&pool->mutexPool);
    int liveNum = pool->liveNum;
    pthread_mutex_unlock(&pool->mutexPool);
    return liveNum;
}


void *worker(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    while(1){

        pthread_mutex_lock(&pool->mutexPool);
        while(pool->queueSize == 0 && !pool->shutdown){
            //阻塞工作线程
            pthread_cond_wait(&pool->notEmpty,&pool->mutexPool);

            //判断是否销毁线程
            if(pool->exitNum > 0){
                pool->exitNum--;
                if(pool->liveNum > pool->minNum){
                    pool->liveNum--;
                    pthread_mutex_unlock(&pool->mutexPool);
                    threadExit(pool);
                }

            }
        }
        //线程池是否被关闭
        if(pool->shutdown){
            pthread_mutex_unlock(&pool->mutexPool);
            threadExit(pool);
        }

        //取出一个任务
        Task task;
        task.function = pool->taskQ[pool->queueFront].function;
        task.arg = pool->taskQ[pool->queueFront].arg;
        //移动队列头
        pool->queueFront = (pool->queueFront+1)%pool->queueCapacity;
        pool->queueSize--;

        pthread_cond_signal(&pool->notFull);
        pthread_mutex_unlock(&pool->mutexPool);

        printf("thread %llu started...\n",pthread_self());
        //忙线程+1
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum++;
        pthread_mutex_unlock(&pool->mutexBusy);

        task.function(task.arg);
        free(task.arg);
        task.arg = NULL;

        printf("thread %llu finished...\n",pthread_self());
        pthread_mutex_lock(&pool->mutexBusy);
        pool->busyNum--;
        pthread_mutex_unlock(&pool->mutexBusy);
    }
    return NULL;
}

void *manager(void *arg) {
    ThreadPool *pool = (ThreadPool*)arg;
    while (!pool->shutdown){
        //1s检测一次
        sleep(1);

        //线程池中任务的数量
        //当前线程的数量
        pthread_mutex_lock(&pool->mutexPool);
        int queueSize = pool->queueSize;
        int liveNum = pool->liveNum;
        pthread_mutex_unlock(&pool->mutexPool);
        //忙线程的数量
        pthread_mutex_lock(&pool->mutexBusy);
        int busyNum = pool->busyNum;
        pthread_mutex_unlock(&pool->mutexBusy);

        //添加线程
        // 任务的个数>存活的线程个数 && 存活的线程数<最大线程数
        if (queueSize > liveNum - busyNum && liveNum < pool->maxNum)
        {
            pthread_mutex_lock(&pool->mutexPool);
            int count = 0;
            for (int i = 0; i < pool->maxNum && count < NUMBER
                            && pool->liveNum < pool->maxNum; ++i) {
                if(pool->threadIDs[i] == 0){

                    pthread_create(&pool->threadIDs[i],NULL,worker,pool);
                    count++;
                    pool->liveNum++;
                }
            }
            pthread_mutex_unlock(&pool->mutexPool);
        }
        //销毁线程
        if(busyNum * 2 < liveNum && liveNum > pool->minNum){
            pthread_mutex_lock(&pool->mutexPool);
            pool->exitNum = NUMBER;
            pthread_mutex_unlock(&pool->mutexPool);
            //唤醒工作线程自杀
            for (int i = 0; i < NUMBER; ++i)
            {
                pthread_cond_signal(&pool->notEmpty);
            }
        }
    }
    return NULL;
}

void threadExit(ThreadPool *pool) {
    pthread_t tid = pthread_self();
    for (int i = 0; i < pool->maxNum; ++i) {
        if (pool->threadIDs[i] == tid)
        {
            pool->threadIDs[i] = 0;
            printf("threadExit() called, %ld exiting...\n", tid);
            break;
        }
    }
    pthread_exit(NULL);
}
void taskFunc(void* arg)
{
    int num = *(int*)arg;
    printf("thread %ld is working, number = %d\n",
           pthread_self(), num);
    sleep(1);
}

