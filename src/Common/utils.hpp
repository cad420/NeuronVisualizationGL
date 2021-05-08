//
// Created by wyz on 2021/4/7.
//

#ifndef NEURONVISUALIZATIONGL_UTILS_H
#define NEURONVISUALIZATIONGL_UTILS_H

#include <chrono>
#include <iostream>
#include <list>
#include <condition_variable>
#include <mutex>
#include <future>
#include <functional>
#include <atomic>
#include <queue>
#include <type_traits>
#include <array>

#include <glm/glm.hpp>

#define FLOAT_ZERO 0.001f

#define START_CPU_TIMER \
{auto _start=std::chrono::steady_clock::now();

#define END_CPU_TIMER \
auto _end=std::chrono::steady_clock::now();\
auto _t=std::chrono::duration_cast<std::chrono::milliseconds>(_end-_start);\
std::cout<<"CPU cost time : "<<_t.count()<<"ms"<<std::endl;}


#define START_CUDA_DRIVER_TIMER \
CUevent start,stop;\
float elapsed_time;\
cuEventCreate(&start,CU_EVENT_DEFAULT);\
cuEventCreate(&stop,CU_EVENT_DEFAULT);\
cuEventRecord(start,0);

#define STOP_CUDA_DRIVER_TIMER \
cuEventRecord(stop,0);\
cuEventSynchronize(stop);\
cuEventElapsedTime(&elapsed_time,start,stop);\
cuEventDestroy(start);\
cuEventDestroy(stop);\
std::cout<<"GPU cost time: "<<elapsed_time<<"ms"<<std::endl;


#define START_CUDA_RUNTIME_TIMER \
{cudaEvent_t     start, stop;\
float   elapsedTime;\
(cudaEventCreate(&start)); \
(cudaEventCreate(&stop));\
(cudaEventRecord(start, 0));

#define STOP_CUDA_RUNTIME_TIMER \
(cudaEventRecord(stop, 0));\
(cudaEventSynchronize(stop));\
(cudaEventElapsedTime(&elapsedTime, start, stop)); \
printf("\tGPU cost time used: %.f ms\n", elapsedTime);\
(cudaEventDestroy(start));\
(cudaEventDestroy(stop));}

template <class T>
inline void print(T t)
{
    std::cout<<t<<std::endl;
}

template <class T,class ...Args>
inline void print(T t,Args... args)
{
    std::cout<<t<<" ";
    print(args...);
}

template<class T,size_t N>
void print_array(const std::array<T,N>& arr)
{
    std::cout<<" ( ";
    for(size_t i=0;i<N;i++){
        std::cout<<arr[i]<<" ";
    }
    std::cout<<") ";
}

template <int N,class T>
void print_vec(const glm::vec<N,T,glm::qualifier::defaultp>& vec){
    std::cout<<" ( ";
    for(int i=0;i<N;i++){
        std::cout<<vec[i]<<" ";
    }
    std::cout<<") "<<std::endl;
}

template<typename T>
class ConcurrentQueue
{
public:

    ConcurrentQueue() {}
    ConcurrentQueue(size_t size) : maxSize(size) {}
    ConcurrentQueue(const ConcurrentQueue&) = delete;
    ConcurrentQueue& operator=(const ConcurrentQueue&) = delete;

    void setSize(size_t s) {
        maxSize = s;
    }

    void push_back(const T& value) {
        // Do not use a std::lock_guard here. We will need to explicitly
        // unlock before notify_one as the other waiting thread will
        // automatically try to acquire mutex once it wakes up
        // (which will happen on notify_one)
        std::unique_lock<std::mutex> lock(m_mutex);
        auto wasEmpty = m_List.empty();

        while (full()) {
            m_cond.wait(lock);
        }

        m_List.push_back(value);
        if (wasEmpty && !m_List.empty()) {
            lock.unlock();
            m_cond.notify_one();
        }
    }

    T pop_front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }
        auto wasFull = full();
        T data = std::move(m_List.front());
        m_List.pop_front();

        if (wasFull && !full()) {
            lock.unlock();
            m_cond.notify_one();
        }

        return data;
    }

    T front() {
        std::unique_lock<std::mutex> lock(m_mutex);

        while (m_List.empty()) {
            m_cond.wait(lock);
        }

        return m_List.front();
    }

    size_t size() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.size();
    }

    bool empty() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_List.empty();
    }
    void clear() {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_List.clear();
    }

private:
    bool full() {
        if (m_List.size() == maxSize)
            return true;
        return false;
    }

private:
    std::list<T> m_List;
    std::mutex m_mutex;
    std::condition_variable m_cond;
    size_t maxSize;
};

template <typename T>
struct atomic_wrapper
{
    std::atomic<T> _a;

    atomic_wrapper()
            :_a()
    {}

    atomic_wrapper(const std::atomic<T> &a)
            :_a(a.load())
    {}

    atomic_wrapper(const atomic_wrapper &other)
            :_a(other._a.load())
    {}

    atomic_wrapper &operator=(const atomic_wrapper &other)
    {
        _a.store(other._a.load());
        return *this;
    }
};

template <typename T>
struct Helper;

template <typename T>
struct HelperImpl : Helper<decltype( &T::operator() )>
{
};

template <typename T>
struct Helper : HelperImpl<typename std::decay<T>::type>
{
};

template <typename Ret, typename Cls, typename... Args>
struct Helper<Ret ( Cls::* )( Args... )>
{
    using return_type = Ret;
    using argument_type = std::tuple<Args...>;
};

template <typename Ret, typename Cls, typename... Args>
struct Helper<Ret ( Cls::* )( Args... ) const>
{
    using return_type = Ret;
    using argument_type = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct Helper<R( Args... )>
{
    using return_type = R;
    using argument_type = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct Helper<R ( * )( Args... )>
{
    using return_type = R;
    using argument_type = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct Helper<R ( *const )( Args... )>
{
    using return_type = R;
    using argument_type = std::tuple<Args...>;
};

template <typename R, typename... Args>
struct Helper<R ( *volatile )( Args... )>
{
    using return_type = R;
    using argument_type = std::tuple<Args...>;
};

template <typename Ret, typename Args>
struct InferFunctionAux
{
};

template <typename Ret, typename... Args>
struct InferFunctionAux<Ret, std::tuple<Args...>>
{
using type = std::function<Ret( Args... )>;
};


template <typename F>
struct InvokeResultOf
{
    using type = typename Helper<F>::return_type;
};

template <typename F>
struct ArgumentTypeOf
{
    using type = typename Helper<F>::argument_type;
};

template <typename F>
struct InferFunction
{
    using type = typename InferFunctionAux<
            typename InvokeResultOf<F>::type,
            typename ArgumentTypeOf<F>::type>::type;
};



struct ThreadPool
{
    ThreadPool( size_t );
    ~ThreadPool();

    template <typename F, typename... Args>
    auto AppendTask( F &&f, Args &&... args );
    void Wait();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    std::mutex mut;
    std::atomic<size_t> idle;
    std::condition_variable cond;
    std::condition_variable waitCond;
    size_t nthreads;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool( size_t threads ) :
        idle( threads ),
        nthreads( threads ),
        stop( false )
{
    for ( size_t i = 0; i < threads; ++i )
        workers.emplace_back(
                [this] {
                    while ( true ) {
                        std::function<void()> task;
                        {
                            std::unique_lock<std::mutex> lock( this->mut );
                            this->cond.wait(
                                    lock, [this] { return this->stop || !this->tasks.empty(); } );
                            if ( this->stop && this->tasks.empty() ) {
                                return;
                            }
                            idle--;
                            task = std::move( this->tasks.front() );
                            this->tasks.pop();
                        }
                        task();
                        idle++;
                        {
                            std::lock_guard<std::mutex> lk( this->mut );
                            if ( idle.load() == this->nthreads && this->tasks.empty() ) {
                                waitCond.notify_all();
                            }
                        }
                    }
                } );
}

// add new work item to the pool
template <class F, class... Args>
auto ThreadPool::AppendTask( F && f, Args && ... args )
{
//#ifndef _HAS_CXX17
    using return_type = typename InvokeResultOf<F>::type;
//#else
//    using return_type=typename std::invoke_result<F,Args...>::type;
//#endif

    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind( std::forward<F>( f ), std::forward<Args>( args )... ) );
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock( mut );
        // don't allow enqueueing after stopping the pool
        if ( stop ) {
            throw std::runtime_error( "enqueue on stopped ThreadPool" );
        }
        tasks.emplace( [task]() { ( *task )(); } );
    }
    cond.notify_one();
    return res;
}

inline void ThreadPool::Wait()
{
    std::mutex m;
    std::unique_lock<std::mutex> l(m);
    waitCond.wait( l, [this]() { return this->idle.load() == nthreads && tasks.empty(); } );
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock( mut );
        stop = true;
    }
    cond.notify_all();
    for ( std::thread &worker : workers ) {
        worker.join();
    }
}
#endif //NEURONVISUALIZATIONGL_UTILS_H
