#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include"locker.h"
//一般线程池中有任务队列，线程池，线程执行函数，任务添加函数
//当外部有任务到来时，会执行任务添加函数添加任务；在构造函数中创建线程时，会指定线程的执行函数，让这个执行函数去争抢任务队列中的任务，并执行任务的函数
//T为任务类
template<typename T>
class threadpool
{
private:
    std::list<T*> m_workequeue; //任务队列
    pthread_t* m_threads; //线程池
    int m_thread_number;
    int m_max_requests;
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;

private:
    static void* worker(void* arg);
    void run();

public:
    //therad_number为线程数量，max_requests为请求队列中最多允许等待的数量
    threadpool(int thread_number = 8 , int max_requests = 10000);
    ~threadpool();
    bool append(T* request); //往任务队列中添加任务
};


template<typename T>
threadpool<T>::threadpool(int thread_number,int max_requests):m_thread_number(thread_number),m_max_requests(max_requests),m_threads(NULL),m_stop(false)
{
    if((thread_number<=0) || (max_requests<=0)){
        throw std::exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }
    //创建线程
    for(int i=0;i<thread_number;++i){
        printf("create the %dth thread\n",i+1);
        pthread_create(m_threads+i,NULL,worker,this);
        pthread_detach(m_threads[i]);
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request){
    m_queuelocker.lock();
    if(m_workequeue.size()>=m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workequeue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workequeue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workequeue.front();
        m_workequeue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}

#endif