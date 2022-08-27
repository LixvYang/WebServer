#ifndef THREADPOOL_H_
#define THREADPOOL_H_

#include <cstdio>
#include <cstddef>
#include <pthread.h>
#include <list>
#include "locker.h"
#include <exception>

template <class T>
class threadpool
{
public:
  threadpool(int thread_number = 8, int max_requests = 10000);
  ~threadpool();
  bool append(T *request);

private:
  static void *worker(void *arg);
  void run();

private:
  //  线程数量
  int m_thread_number;
  // 线程池数组，大小为m_thread_number
  pthread_t *m_threads;
  // 最多连接数
  int m_max_requests;
  // 请求队列
  std::list<T *> m_workqueue;
  // 互斥锁
  locker m_queuelocker;
  // 信号量来判断是否有任务需要处理
  sem m_queuestat;
  //  是否结束线程
  bool m_stop;
};

template <class T>
threadpool<T>::threadpool(int thread_number, int max_requests) : m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(NULL)
{
  if ((thread_number <= 0) || (max_requests <= 0))
  {
    throw std::exception();
  }
  m_threads = new pthread_t[m_thread_number];
  if (!m_threads)
  {
    throw std::exception();
  }
  // create thread_number thread
  for (int i = 0; i < thread_number; ++i)
  {
    printf("create the %dth thread\n", i);

    if (pthread_create(m_threads + i, NULL, worker, NULL) != 0)
    {
      delete[] m_threads;
      throw std::exception();
    }
    if (pthread_detach(m_threads[i]))
    {
      delete[] m_threads;
      throw std::exception();
    }
  }
}

template <class T>
threadpool<T>::~threadpool()
{
  delete[] m_threads;
  m_stop = true;
}

template <class T>
bool threadpool<T>::append(T *request)
{
  m_queuelocker.lock();
  if (m_workqueue.size() > m_max_requests)
  {
    m_queuelocker.unlock();
    return false;
  }
  m_workqueue.push_back(request);
  m_queuelocker.unlock();
  m_queuestat.post();
  return true;
}

template <class T>
void *threadpool<T>::worker(void *arg)
{
  threadpool *pool = (threadpool *)arg;
  pool->run();
  return pool;
}

template <class T>
void threadpool<T>::run()
{
  while (!m_stop)
  {
    m_queuestat.wait();
    m_queuelocker.lock();
    if (m_workqueue.empty())
    {
      m_queuelocker.unlock();
      continue;
    }
    T *request = m_workqueue.front();
    m_workqueue.pop_front();
    m_queuelocker.unlock();
    if (!request)
    {
      continue;
    }
    request->process();
  }
}

#endif // !THREADPOOL_H_