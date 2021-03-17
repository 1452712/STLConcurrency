//--------------------------------------------------------------------------
// A practicing of Cpp Concurency in Action (v.2)
//--------------------------------------------------------------------------
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <future>
#include <list>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

// 3.9 std::lock 
class BigObj;
void Swap(BigObj& lhs, BigObj& rhs);
class SwapClass
{
private:
    BigObj mObj;
    std::mutex mMtx;
public:
    SwapClass(BigObj const& data) : mObj(data) {};
    friend void Swap(SwapClass& lhs, SwapClass& rhs)
    {
        if(&lhs == &rhs)
            return;
        // use lock_guard for less mem & performance
        // useless necessary like the following situation
        // 1 particular situation: condition_variable
        std::unique_lock<std::mutex> lock_1(mutex_1, std::defer_lock);
        std::unique_lock<std::mutex> lock_2(mutex_2, std::defer_lock);
        std::lock(lock_1, lock_2);
        // // Equivalent code 
        // std::lock(mutex_1, mutex_2);
        // std::lock_guard<std::mutex> lock_1(mutex_1, std::adopt_lock);
        // std::lock_guard<std::mutex> lock_2(mutex_2, std::adopt_lock);
        // // Superior solution available since C++17
        // std::scoped_lock lock(mutex_1, mutex_2);
        Swap(lhs.mObj, rhs.mObj);
    }
};

// 3.12 std::call_once
class A
{
private:
    connection_info connection_details;
    connection_handle connection;
    std::once_flag connection_init_flag;

    void open_connection()
    {
        connection = connection_manager.open(connection_details);
    }

public:
    A(connection_info const& connection_details_):
    connection_details(connection_details_)
    {}

    void send_data(data_packet const& data)
    {
        std::call_once(connection_init_flag, &X::open_connection, this);
        connection.send_data(data);
    }

    data_packet receive_data()
    {
        std::call_once(connection_init_flag, &X::open_connection, this);
        retrun connection.receive_data();
    }  
};

// 4.1 EVENT 
// Listing 4.5 threadsafe_queue
// Try to limit lock account (1 per thread)
template<typename T>
class threadsafe_queue
{
private:
    mutable std::mutex mut; // MUTABLE!
    std::queue<T> data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue()
    {}

    threadsafe_queue(threadsafe_queue const& from)
    {
        std::lock_guard<std::mutex> lk(from.mut);
        data_queue = from.data_queue;
    }

    void push(T new_value)
    {
        std::lock_guard<std::mutex> lk(mut);
        data_queue.push(new_value);
        data_cond.notify_one();
    }

    void wait_and_pop(T& value)
    {
        std::unique_lock<std::mutex> lk(mut);
        data_cond.wait(lk, [this]{return !data_queue.empty();});
        value = data_queue.front();
        data_queue.pop();
    }

    std::shared_ptr<T> wait_and_pop()
    {
        std::unique_lock<std::mutex> lk(mut):
        data_cond.wait(lk, [this]{return !data_queue.empty();});
        std::shared_ptr<T> res(std::make_shared<T>(data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool try_pop(T& value)
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return false;
        value = data_queue.front();
        data_queue.pop();
        return true;
    }

    std::shared_ptr<T> try_pop()
    {
        std::lock_guard<std::mutex> lk(mut);
        if(data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(std::make_shared<T>data_queue.front()));
        data_queue.pop();
        return res;
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut);
        return data_queue.empty();
    }
};

// 4.2 ONE-OFF EVENTS
// Listing 4.9 std::packaged_task, passing tasks between threads
std::mutex m;
std::deque<std::packaged_task<void()>> tasks;
bool GUIShutdownMessageReceived();
void GetAndProcessGUIMessage();
void GUIThread()
{
    while(!GUIShutdownMessageReceived())
    {
        GetAndProcessGUIMessage();
        std::packaged_task<void()> task;
        {
            std::lock_guard<std::mutex> lk(m);
            if (tasks.empty())
                continue;
            task = std::move(tasks.front());
            tasks.pop_front();
        }
        task();
    }
}

std::thread GUIBgThread(GUIThread);
// Get return value of an asynchronous task
// The most simple way is to use std::async
// Func: a function / callable object
template<typename Func>
std::future<void> PostTaskForGUIThread(Func f)
{
    // associate a task with a future
    std::packaged_task<void()> task(f);
    // unique future; std::shared_future can be used for parallel execution
    std::future<void> res = task.get_future();
    std::lock_guard<Std::mutex> lk(m);
    tasks.push_back(std::move(task));
    return res;
}

// Listing 4.10 std::promise, no simple callable / multiple result source
void ProcessConnections(ConnectionSet& connections)
{
    while (!Done(connections))
    {
        for(ConnectionIterator connection = connections.begin(), end = connections.end();
            connection != end; ++connection)
            {
                if(connection->HasIncomingData())
                {
                    DataPacket data = connection->Incoming();
                    std::promise<PayloadType> &p =
                        connection->get_promise(data.Id);
                    try
                    {
                        p.set_value(data.Payload);
                    }
                    catch(...)
                    {
                        p.set_exception(std::current_exception());
                    }
                }

                if(connection->HasOutgoingData())
                {
                    OutgoingPacket data = 
                        connection->TopOfOutgoingQueue();
                    connection->Send(data.Payload);
                    data.Promise.set_value(true);
                    // An alternative way to store exception without throw
                    data.Promise.set_exception(std::make_exception_ptr(
                        std::logic_error("foo ")));
                }
            }
    }
}

// 4.3 TIME LIMIT
// Listing 4.11 condition_variable.wait_until, timeout
// (refer to table 4.1 for more timeout methods)
std::condition_variable cv;
bool isDone;
std::mutex m;
bool WaitLoop()
{
    auto const timeout = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(500);
    std::unique_lock<std::mutex> lk(m);
    while(!isDone)
    {
        // wait_until/sleep_until a time_point
        // wait_for/sleep_for a duration
        if(cv.wait_until(lk, timeout) == std::cv_status::timeout)
            break;
    }
    return isDone;
}

// 4.4 FP (Functional Programming, not related to external state,
// thus without any explicit access to shared data)
// Compared with CSP (Communicating Sequential Proceses, thread is
// a state machine; no shared data, all through message queues)
// Listing 4.13 FP Quicksort in parallel
template<typename T>
std::list<T> parallel_quick_sort<std::list<T> input)
{
    if(input.empty())
        return input;

    std::list<T> result;
    result.splice(result.begin(), input, input.begin());
    T const& pivot = *result.begin();
    auto divide_point = std::partition(input.begin(), input.end(),
        [&](T const& t){return t < pivot;});
    // sort the lower portion in another thread
    std::list<T> lower_part;
    lower_part.splice(lower_part.end(), input, input.begin(), divide_point);
    std::future<std::list<T> > new_lower(
        std::async(&parallel_quick_sort<T>, std::move(lower_part))
    );
    // sort the upper portion in current thread
    std::list<T> new_higher(
        parallel_quick_sort(std::move(input))
    );
    result.splice(result.end(), new_higher);
    result.splice(result.begin(), new_lower.get());
    return result;
}

// // Appendix A: an alternative of std::async
// template <typename F, typename A>
// std::future<std::result_of<F(A &&)>::type> spawn_task(F &&f, A &&a)
// {
//     typedef std::result_of<F(A &&)>::type result_type;
//     std::packaged_task<result_type(A &&)> task(std::move(f));
//     std::future<result_type> res(task.get_future());
//     std::thread t(std::move(task), std::move(a));
//     t.detach();
//     return res;
// }

// // Appendix B: quick_sort optimization
// template <typename It>
// void qsort(It first, It last)
// {
//     while (last - first > THRESHOLD)
//     {
//         auto cut = partition_pivot(first, last);
//         qsort(cut, last);
//         last = cut;
//     }
//     // optimistic linear insertion sort
//     small_sort(first, last);
// }

// C++20 supports executing a future in a separate thread with coroutines
// https://www.modernescpp.com/index.php/executing-a-future-in-a-separate-thread-with-coroutines
// require: 
// #include <coroutine>
template<typename T>
struct my_future {
    struct promise_type;
    using handle_type = std::coroutine_handle<promise_type>;
    handle_type coro;

    my_future(handle_type h) : coro(h) {}
    ~my_future() {
        if(coro) coro.destroy();
    }

    T get() {
        std::cout <<"    my_future::get: "
            << "std::this_thread::get_id(): "
            << std::this_thread::get_id() << std::endl;
        
        // resumes coroutine on another thread
        std::thread t([this] { coro.resume(); });
        t.join();

        // Finish resuming before return result.
        // Thus, calling t.join() after return is undefined.
        // e.g. using jthread without scope will return arbitrary value.
        return coro.promise().result;
    }

    // alternative: specialize coroutine_traits
    struct promise_type {
        T result;

        promise_type() {
            std::cout << "        promise_type::promise_type: "
                      << "std::this_thread::get_id(): "
                      << std::this_thread::get_id() << std::endl;
        }

        ~promise_type() {
            std::cout << "        promise_type::~promise_type: "
                      << "std::this_thread::get_id(): "
                      << std::this_thread::get_id() << std::endl;
        }

        auto get_return_object() {
            return my_future{handle_type::from_promise(*this)};
        }

        // Tips:
        // The promise needs a return_void member function if
        //     the coroutine has no co_return statement.
        //     the coroutine has a co_return statement without argument.
        //     the coroutine has a co_return expression statement where expression has type void.
        // The promise needs a return_value member function if 
        //     it returns co_return expression statement where expression must not have the type void
        void return_value(T v) {
            std::cout << "        promise_type::return_value: "
                      << "std::this_thread::get_id(): "
                      << std::this_thread::get_id() << std::endl;
            std::cout << v << std::endl;
            result = v;
        }

        void return_void() {}

        std::suspend_always initial_suspend() {
            return {};
        }

        std::suspend_always final_suspend() noexcept {
            std::cout << "        promise_type::final_suspend: "
                      << "std::this_thread::get_id(): "
                      << std::this_thread::get_id() << std::endl;
            return {};
        }

        void unhandled_exception() {
            std::exit(1);
        }
    };
};
my_future<int> test_create_future() {
    co_return 2021;
}
int test() {
    std::cout << std::endl;
    std::cout << "test: "
        << "std::this_thread::get_id()"
        << std::this_thread::get_id() << std::endl;
    
    auto fut = test_create_future();
    auto res = fut.get();

    std::cout << "res: " << res << std::endl;
    return 0;
}

// Listing 4.22 std::async to gather all results
std::future<FinalResult> ProcessData(std::vector<MyData>& vec)
{
    size_t const CHUNK_SIZE = blabla;
    std::vector<std::future<ChunkResult> > results;
    for (auto begin = vec.begin(), end = vec.end(); begin != end;)
    {
        size_t const remainingSize = end - begin;
        size_t const thisChunkSize = std::min(remainingSize, CHUNK_SIZE);
        auto ProcessChunk = [](){/*blabla*/};
        results.push_back(
            std::async(ProcessChunk, begin, begin + thisChunkSize))
        );
        begin += thisChunkSize;
    }

    return std::async([allRes = std::move(results)]() {
        std::vector<ChunkResult> v;
        v.reserve(allRes.size());
        for(auto& f : allRes)
            v.push_back(f.get());
        return GatherResults(v);
    });
}

// Listing 5.1 std::atomic_flag for spinlock
class spinlock_mutex
{
    std::atomic_flag flag;

public:
    spinlock_mutex() : flag(ATOMIC_FLAG_INIT) {}

    void lock()
    {
        // busy-wait, cannot query without modifying
        while (flag.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock()
    {
        flag.clear(std::memory_order_release);
    }
};

// // Try to create an atomic shared_ptr
// template<typename T>
// class atomic_shared_ptr
// {
// public:
//     void initialize()
//     {
//         if (std::atomic_if_lock_free())
//         {
//             std::shared_ptr<T> local(new T);
//             std::atomic_store(&p, local);
//         }
//         else
//         {
//             std::unique_lock lk(mtx);
//             // ...
//         }
        
//     }

//     std::shared_ptr<T> load()
//     {
//         return std::atomic_load(&p);
//     }

//     void store(const T& input)
//     {
//         std::shared_ptr<T> inputPtr(std::make_shared(input));
//         std::atomic_store(&p, inputPtr);
//     }
// private:
//     std::shared_ptr<T> p;
//     std::recursive_mutex mtx;
// }

// Memory Orders
// Listing 5.4 Sequential Consistent
std::atomic<bool> x, y;
std::atomic<int> z;
void write_x() { x.store(true, std::memory_order_seq_cst);}
void write_y() { y.store(true, std::memory_order_seq_cst);}
void read_x_then_y(){
    while(!x.load(std::memory_order_seq_cst))
        ;
    if(y.load(std::memory_order_seq_cst))
        ++z;
}
void read_y_then_x(){
    while(!y.load(std::memory_order_seq_cst))
        ;
    if(x.load(std::memory_order_seq_cst))
        ++z;
}
int main(){
    x = y = false;
    z = 0;
    
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);

    a.join();
    b.join();
    c.join();
    d.join();
    // z == 1: x[y].load true & y[x].load false (i.e. a seq);
    // z == 2: x.load true & y.load true;
    assert(z.load() != 0);
}

// Listing 5.5 Relaxed
std::atomic<bool> x, y;
std::atomic<int> z;
void write_x_then_y() {
    x.store(true, std::memory_order_relaxed);
    y.store(true, std::memory_order_relaxed);
}
void read_y_then_x() {
    while(!y.load(std::memory_order_relaxed))
        ;
    if(x.load(std::memory_order_relaxed))
        z++;
}
int main() {
    x = y = false;
    z = 0;
    std::thread a(write_x_then_y);
    std::thread b(read_y_then_x);
    a.join();
    b.join();
    // z == 0: loads see stores out of order
    assert(z.load()!=0);

}

// Listing 5.7 Acquire-Release
std::atomic<bool> x, y;
std::atomic<int> z;
void write_x() { x.store(true, std::memory_order_release);}
void write_y() { y.store(true, std::memory_order_release);}
void read_x_then_y() {
    while (!x.load(std::memory_order_acquire))
        ;
    if (y.load(std::memory_order_acquire))
        ++z;
}
void read_y_then_x() {
    while (!y.load(std::memory_order_acquire))
        ;
    if (x.load(std::memory_order_acquire))
        ++z;
}
int main() {
    x = y = false;
    z = 0;

    // stores happen in different threads,
    // no happens-before relationship.
    std::thread a(write_x);
    std::thread b(write_y);
    std::thread c(read_x_then_y);
    std::thread d(read_y_then_x);

    a.join();
    b.join();
    c.join();
    d.join();
    // assert can fire (i.e. z might be 0)
    assert(z.load() != 0);
}

// Listing 5.8 Acquire-Release to Impose ordering on Relaxed
// Synchronization require acquire & release paired up.
std::atomic<bool> x, y;
std::atomic<int> z;
void write_x_then_y() {
    x.store(true, std::memory_order_relaxed);
    // store will synchronize with load
    y.store(true, std::memory_order_release);
}
void read_y_then_x() {
    // spin, waiting for y to be set to true
    while (!y.load(std::memory_order_acquire))
        ;
    if(x.load(std::memory_order_relaxed))
        ++z;
}
int main() {
    x = y = false;
    z = 0;
    std::thread a(write_x_then_y);
    std::thread b(read_y_then_x);
    a.join();
    b.join();
    // z must be 1
    assert(z.load() != 0);
}

// Listing 5.9 Acquire-Release for Transitive synchronization
std::atomic<int> data[5];
std::atomic<bool> sync1(false), sync2(false);
// alternative
// std::atomic<int> sync(0);
void thread_1() {
    data[0].store(43, std::memory_order_relaxed);
    data[1].store(97, std::memory_order_relaxed);
    data[2].store(17, std::memory_order_relaxed);
    data[3].store(-141, std::memory_order_relaxed);
    data[4].store(2003, std::memory_order_relaxed);
    // set sync1 (i.e. unlock)
    sync1.store(true, std::memory_order_release);
    // alternative
    // sync.store(1, std::memory_order_release);
}
void thread_2() {
    // loop until sync1 is set (i.e. lock)
    while(!sync1.load(std::memory_order_acquire)))
        ;
    // set sync2
    sync2.store(true, std::memory_order_release);

    // alternative
    // int expected = 1;
    // while (!sync.compare_exchange_strong(expected, 2, 
    //     std::memory_order_acq_rel))
    //     expected = 1;
}
void thread_3() {
    // loop until sync2 is set
    while (!sync2.load(std::memory_order_acquire))
        ;
    // alternative
    // while(sync.load(std::memory_order_acquire) < 2)
    //     ;

    // must no assert
    assert(data[0].load(std::memory_order_relaxed) == 42);
    assert(data[1].load(std::memory_order_relaxed) == 97);
    assert(data[2].load(std::memory_order_relaxed) == 17);
    assert(data[3].load(std::memory_order_relaxed) == -141);
    assert(data[4].load(std::memory_order_relaxed) == 2003);
}

// Listing 5.11 Release Sequence
std::vector<int> queue_data;
std::atomic<int> count;
void populate_queue() {
    unsigned const number_of_items = 20;
    queue_data.clear();
    for (unsigned i = 0; i < number_of_items; ++i) {
        queue_data.push_back(i);
    }
    // the initial store
    count.store(number_of_items, std::memory_order_release);
}
void consume_queue_items() {
    while(true) {
        int item_index;
        // RMW (read-modify-write) op
        if((item_index = count.fetch_sub(1, std::memory_order_acquire)) <= 0) {
            wait_for_more_items();
            continue;
        }
        // reading queue_data is safe
        process(queue_data[item_index - 1]);
    }
}
int main() {
    std::thread a(populate_queue);
    // no synchronize-with between b & c
    std::thread b(consume_queue_items);
    std::thread c(consume_queue_items);
    a.join();
    b.join();
    c.join();
}

// Listing 5.13 Fence to enforce ordering on Non-atomic
bool x = false; // a plain non-atomic var
std::atomic<bool> y;
std::atomic<int> z;
void write_x_then_y() {
    // store to x before fence
    x = true;
    std::atomic_thread_fence(std::memory_order_release);
    // alternative: std::atomic_store(x, true, std::memory_order_release);

    // store to y after fence
    y.store(true, std::memory_order_relaxed);
}
void read_y_then_x() {
    // wait until store
    while(!y.load(std::memory_order_relaxed));
    std::atomic_thread_fence(std::memory_order_acquire);
    // read value stored
    if (x) ++z;
    // alternative: if(std::atomic_load(x, std::memory_order_acquire) && x) ++z;
}
int main() {
    x = false;
    y = false;
    z = 0;
    std::thread a(write_x_then_y);
    std::thread b(read_y_then_x);
    a.join();
    b.join();
    // must no assert
    assert(z.load() != 0);
}

// Listing 6.1 Thread-safe Stack [Lock-based]
struct empty_stack: std::exception {
    const char* what() const throw();
};
template<typename T>
class threadsafe_stack {
private:
    std::stack<T> data;
    mutable std::mutex m;
public:
    threadsafe_stack() {}
    threadsafe_stack(const threadsafe_stack& other) {
        std::lock_guar<std::mutex> lock(other.m);
        data = other.data;
    }
    threadsafe_stack& operator = (const threadsafe_stack&) = delete;

    void push(T new_value) {
        std::lock_guard<std::mutex> lk(m);
        // std::stack<> guarantees safety for push() exception,
        // if either copying/moving fails or 
        // not enough memory for allocating extended data structures.
        data.push(std::move(new_value));
    }
    std::shared_ptr<T> pop() {
        std::lock_guard<std::mutex> lk(m);
        // avoid race condition
        if(data.empty())
            throw empty_stack();
        // If creation of res fails, CRT & STL ensure no mem leaks,
        // and new obj destroyed. No modification of the data.
        // The following data.pop() is guaranteed not to throw.
        // So this is exception-safe.
        std::shared_ptr<T> const res(
            // separate top() and pop() to avoid race condition
            std::make_shared<T>(std::move(data.top()))
        );
        data.pop();
        return res;
    }
    void pop(T& value) {
        std::lock_guard<std::mutex> lk(m);
        if(data.empty())
            throw empty_stack();
        // Throw due to copy/move assignment
        // rather than construction.
        // Also exception-safe.
        value = std::move(data.top());
        data.pop();
    }
    bool empty() const {
        // No modify. Exception-safe.
        std::lock_guard<std::mutex> lk(m);
        return data.empty();
    }
};

// Listing 6.2 Thread-safe Queue [Lock-based]
template<typename T>
class threadsafe_queue {
private:
    mutable std::mutex m;
    std::queue<T> data_queue;
    // alternative: std::queue<std::shared_ptr<T> > data_queue;
    std::condition_variable data_cond;
public:
    threadsafe_queue() {}
    
    void push(T value) {
        std::lock_guard<std::mutex> lk(m);
        data_queue.push(std::move(value));
        // alternative:
        // Because the allocation is outside the lock,
        // it reduces time the mutex is held.
        // std::shared_ptr<T> data(std::make_shared<T>(std::move(value)));
        // std::lock_guard<std::mutex> lk(m);
        // data_queue.push(data);

        // awake one thread
        data_cond.notify_one();
    }

    void wait_and_pop(T& value) {
        std::unique_lock<std::mutex> lk(m);
        // avoid busy-waiting / race conditions / deadlock
        data_cond.wait(lk, [this] {return !data_queue.empty();});
        try {
            value = std::move(data_queue.front());
            data_queue.pop();
        }
        catch(std::exception ...){
            // notify another thread to retrieve
            // alternative: store shared_ptr in queue, because
            //     copying shared_ptr out of queue cannot throw.
            data_cond.notify_one();
        }
        // alternative
        // value = std::move(*data.front());
        // data_queue.pop();
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_lock<std::mutex> lk(m);
        data_cond.wait(lk, [this] { return !data_queue.empty(); });
        std::shared_ptr<T> res(
            std::make_shared<T>(std::move(data_queue.front())
        );
        // alternative: std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }

    bool try_pop(T& value) {
        std::lock_guard<std::mutex> lk(m);
        if (data_queue.empty())
            return false;
        value = std::move(data_queue.front());
        // alternative: value = std::move(*data_queue.front());
        data_queue.pop();
        return true;
    }
    std::shared_ptr<T> try_pop() {
        std::lock_guard<std::mutex> lk(m);
        if (data_queue.empty())
            return std::shared_ptr<T>();
        std::shared_ptr<T> res(
            std::make_shared<T>(std::move(data_queue.front())
        );
        // alternative: std::shared_ptr<T> res = data_queue.front();
        data_queue.pop();
        return res;
    }

    bool empty() const {
        std::lock_guard<std::mutex> lk(m);
        return data_queue.empty();
    }
};

// Listing 6.6 Thread-safe Linked-List [Lock-based]
template<typename T>
class threadsafe_list {
private:
    struct node {
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
    };
    // unbounded
    std::unique_ptr<node> head;
    node* tail;
    mutable std::mutex head_mutex;
    mutable std::mutex tail_mutex;
    std::condition_variable data_cond;

    node* get_tail() {
        std::lock_guard<std::mutex> tail_lock(tail_mutex);
        return tail;
    }
    std::unique_ptr<node> pop_head() {
        std::unique_ptr<node> old_head = std::move(head);
        head = std::move(old_head->next);
        return old_head;
    }
    std::unique_lock<std::mutex> wait_for_data() {
        // ensure no data race reading data from head
        std::unique_lock<std::mutex> head_lock(head_mutex);

        // must inside head_lock, otherwise,
        // get_tail() may get outdated/invalid node
        data_cond.wait(head_lock, [&]{ return head.get() != get_tail();});
        return std::move(head_lock);
    }

    std::unique_ptr<node> wait_pop_head() {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        return pop_head();
    }
    std::unique_ptr<node> wait_pop_head(T& value) {
        std::unique_lock<std::mutex> head_lock(wait_for_data());
        value = std::move(*head->data);
        return pop_head();
    }
    std::unique_ptr<node> try_pop_head() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return head.get() == get_tail() ? 
            std::unique_ptr<node>() : pop_head();
    }
    std::unique_ptr<node> try_pop_head(T& value) {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        if (head.get() == get_tail())
            return std::unique_ptr<node>();
        value = std::move(*head->data);
        return pop_head();
    }
public:
    threadsafe_list(): head(new node), tail(head.get()) {}
    threadsafe_list(const threadsafe_list&) = delete;
    threadsafe_list& operator=(const threadsafe_list&) = delete;

    // exception-safe
    std::shared_ptr<T> try_pop() {
        // make destorying old_head out of lock.
        std::unique_ptr<node> old_head = try_pop_head();
        return old_head ? old_head->data : std::shared_ptr<T>();
    }
    bool try_pop(T &value) {
        std::unique_ptr<node> const old_head = try_pop_head();
        return old_head;
    }
    std::shared_ptr<T> wait_and_pop() {
        std::unique_ptr<node> const old_head = wait_pop_head();
        return old_head->data;
    }
    void wait_and_pop(T &value) {
        std::unique_ptr<node> const old_head = wait_pop_head(value);
    }
    // exception-safe
    void push(T new_value) {
        std::shared_ptr<T> new_data(
            std::make_shared<T>(std::move(new_value))
        );
        std::unique_ptr<node> p(new node);
        {
            std::lock_guard<std::mutex> tail_lock(tail_mutex);
            tail->data = new_data;
            node* const new_tail = p.get();
            tail->next = std::move(p);
            tail = new_tail;
        }
        data_cond.notify_one();
    }

    bool empty() {
        std::lock_guard<std::mutex> head_lock(head_mutex);
        return (head.get() == get_tail());
    }
};

// Listing 6.11 Thread-safe Hash Table [Lock-based]
template<typename Key, typename Value, typename Hash = std::hash<Key> >
class threadsafe_lookup_table {
private:
    class bucket_type {
    private:
        typedef std::pair<Key, Value> bucket_value;
        typedef std::list<bucket_value> bucket_data;
        typedef typename bucket_data::iterator bucket_iterator;
        // each bucket contains one list of key/value pairs
        bucket_data data;
        mutable std::shared_mutex mutex;

        // determine whether the entry is in the bucket
        bucket_iterator find_entry_for(Key const & key) const {
            return std::find_if(data.begin(). data.end(),
                [&](bucket_value const& item)
                { return item.first == key;});
        }
    public:
        Value value_for(Key const& key, Value const& default_value) const {
            // read-only (shared ownership)
            std::shared_lock<std::shared_mutex> lock(mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            return (found_entry == data.end()) ?
                default_value : found_entry -> second;
        }
        void add_or_update_mapping(Key const& key, Value const& value) {
            // read-write (unique ownership)
            std::unique_lock<std::shared_mutex> lock(mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry == data.end()) {
                data.push_back(bucket_value(key, value));
            }
            else {
                    found_entry->second = value;
            }
        }
        void remove_mapping(Key const& key) {
            // read-write
            std::unique_lock<std::shared_mutex> lock(mutex);
            bucket_iterator const found_entry = find_entry_for(key);
            if (found_entry != data.end()) {
                data.erase(found_entry);
            }
        }
    };
    // vector allows specifing bucket amount
    std::vector<std::unique_ptr<bucket_type> > buckets;
    Hash hasher;
    // Fixed-size buckets, no need to lock
    bucket_type& get_bucket(Key const& key) const {
        std::size_t const bucket_index = hasher(key) % buckets.size();
        return *buckets[bucket_index];
    }
public:
    typedef Key key_type;
    typedef Value mapped_type;
    typedef Hash hash_type;
    // default number: an arbitrary prime number
    threadsafe_lookup_table (unsigned num_buckets = 19, Hash const& hasher_=Hash()) 
    : buckets(num_buckets), hasher(hasher_) {
        for (unsigned i = 0; i < num_buckets; ++i) {
            buckets[i].reset(new bucket_type);
        }
    }
    threadsafe_lookup_table(threadsafe_lookup_table const&) = delete;
    threadsafe_lookup_table& operator=(threadsafe_lookup_table const&) = delete;

    Value value_for(Key const& key, Value const& default_value = Value()) const {
        // get_bucket can be called without lock
        return get_bucket(key).value_for(key, default_value);
    }
    void add_or_update_mapping(Key const& key, Value const& value) {
        get_bucket(key).add_or_update_mapping(key, value);
    }
    void remove_mapping(Key const& key) {
        get_bucket(key).remove_mapping(key);
    }

    // retrieve a snapshot of current table
    std::map<Key, Value> get_map() const {
        std::vector<std::unique_lock<std::shared_mutex> > locks;
        for (unsigned i = -; i < buckets.size(); ++i) {
            locks.push_back(std::unique_lock<std::shared_mutex>(buckets[i].mutex));
        }
        std::map<Key, Value> res;
        for(unsigned i = 0; i < buckets.size(); ++i) {
            auto const & bkt_data = buckets[i].data;
            for(bucket_iterator iter = bkt_data.begin(); iter != bkt_data.end(); ++iter) {
                res.insert(*iter);
            }
        }
        return res;
    }
};

// Listing 6.13 Thread-safe List with Iteration [Lock-based]
template<typename T>
class threadsafe_list {
    // entry of linked list
    struct node {
        std::mutex m;
        std::shared_ptr<T> data;
        std::unique_ptr<node> next;
        // nullptr next for head
        node(): next() {}
        // alloc on heap
        node(T const& value) : data(Std::make_shared<T>(value)) {}
    };
    node head;
public:
    threadsafe_list() {}
    ~threadsafe_list() {
        remove_if([](node const&){ return true; });
    }
    threadsafe_list(threadsafe_list const &) = delete;
    threadsafe_list &operator=(threadsafe_list const &) = delete;

    void push_front(T const& value) {
        // construct new node
        std::unique_ptr<node> new_node(new node(value));
        // lock head to get correct next value
        std::lock_guard<std::mutex> lk(head.m);
        new_node->next = std::move(head.next);
        head.next = std::move(new_node);
    }

    template<typename Function>
    void for_each(Function callable) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        // hand-over-hand lock
        while (node* const next = current->next.get()) {
            // lock next mutex before releasing current
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            // update items, copy, ...
            callable(*next->data);
            current = next;
            lk = std::move(next_lk);
        }
    }

    // Predicate must return true (match) or false (no match)
    template<typename Predicate>
    std::shared_ptr<T> find_first_if(Predicate callable) {
        node* current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node* const next = current->next.get()) {
            std::unique_lock<std::mutex> next_lk(next->m);
            lk.unlock();
            if(callable(*next->data))
                return next->data;
            current = next;
            lk = std::move(next_lk);
        }
        return std::shared_ptr<T>();
    }

    template <typename Predicate>
    std::shared_ptr<T> remove_if(Predicate callable) {
        node *current = &head;
        std::unique_lock<std::mutex> lk(head.m);
        while (node *const next = current->next.get())
        {
            std::unique_lock<std::mutex> next_lk(next->m);
            if (callable(*next->data)) {
                // remove next node & update current node
                std::unique_ptr<node> old_next = std::move(current->next);
                current->next = std::move(next->next);
                next_lk.unlock();
            } else {
                // move to next node
                lk.unlock();
                current = next;
                 // ensure mutex on "current" node is hold
                 // so that destroying "old_next" node is safe.
                lk = std::move(next_lk);
            }
        }
    }
};

// Listing 7.2 Lock-free Stack using thread account
template<typename T>
class lock_free_stack {
private:
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node(T const& data_) : data(std::make_shared<T>(data_)) {}
    };
    std::atomic<node*> head;

    ///////////////////////////////////////////////////
    // deal with removed nodes
    // alternative: hazard pointer (Listing 7.6)
    std::atomic<unsigned> threads_in_pop;
    // the head pointer of node list to be released
    std::atomics<nodes*> to_be_deleted;
    static void delete_nodes(node* nodes) {
        while(nodes) {
            node* next = nodes->next;
            delete nodes;
            nodes = next;
        }
    }
    void try_reclaim(node* old_head) {
        if(threads_in_pop == 1) {
            // Claim list of to-be-deleted nodes
            node* nodes_to_delete = to_be_deleted.exchange(nullptr);
            if(!--threads_in_pop) {
                // current thread is the only thread in pop()
                delete_nodes(nodes_to_delete);
            } else if(nodes_to_delete) {
                chain_pending_nodes(nodes_to_delete);
            }
            // no need to append old_head
            delete old_head;
        } else {
            // multiple threads in pop(), 
            // need to ensure old_head is correct
            chain_pending_node(old_head);
            --threads_in_pop;
        }
    }
    // help functions
    void chain_pending_nodes(node* nodes) {
        node* last = nodes;
        while(node* const next = last->nex) {
            last = next;
        }
        chain_pending_node(nodes, last);
    }
    void chain_pending_nodes(node* first, node* last) {
        last->next = to_be_deleted;
        while(!to_be_deleted.compare_exchange_weak(last->next, first))
            ;
    }
    void chain_pending_node(node* n){
        chain_pending_nodes(n, n);
    }
public:
    void push(T const& data) {
        node* const new_node = new node(data);
        new_node->next = head.load();
        // ensure head pointer still has the same value when updating
        // otherwise, new_node->next will be updated to new head.
        // bool compare_exchange_weak(expected, desired){
        //     ret = false;
        //     if(data.load() == expected)
        //         ret = data.store(desired);
        //     expected = data.load();
        //     return ret;
        // }
        // bool compare_exchange_strong(expected, desired) {
        //     ret = false;
        //     if (data.load() == expected) {
        //         data.store(desired);
        //         ret = true;
        //     }
        //     expected = data.load();
        //     return ret;
        // }
        while(!head.compare_exchange_weak(new_node->next, new_node))
            ;
    }
    std::shared_ptr<T> pop() {
        ++threads_in_pop;
        node* old_head = head.load();
        // Check old_head is not nullptr before dereference
        while (old_head && !head.compare_exchange_weak(old_head, old_head->next))
            ;
        std::shared_ptr<T> res;
        if(old_head){
            // extract data rather than copying pointer
            res.swap(old_head->data);
        }
        // try to deleate node & decrease threads_in_pop
        try_reclaim(old_head);
        return res;
    }
};

// Listing 7.6 Lock-free Stack using hazard pointers
template<typename T>
class lock_free_stack {
private:
    //////////////////////////////////////////////////
    // help functions for hazard pointer
    unsigned const max_hazard_pointers = 100;
    struct hazard_pointer {
        std::atomic<std::thread::id> id;
        std::atomic<void*> pointer;
    };
    // owner/pointer table
    hazard_pointer hazard_pointers[max_hazard_pointers];
    class hp_owner {
        hazard_pointer* hp;
    public:
        hp_owner(hp_owner const&) = delete;
        hp_owner operator=(hp_owner const &) = delete;
        hp_owner() : hp(nullptr) {
            for(unsigned i = 0; i < max_hazard_pointers; ++i) {
                std::thread::id old_id;
                // try to claim ownership of a hazard pointer
                if(hazard_pointers[i].id.compare_exchange_strong(
                    old_id, std::this_thread::get_id()
                )) {
                    hp = &hazard_pointers[i];
                    break;
                }
            }if (!hp) {
                // Too many threads using hazard pointers
                // so that no available entry in the table.
                throw std::runtime_error("No hazard pointers available");
            }
        }
        std::atomic<void*>& get_pointer() {
            return hp->pointer;
        }
        ~hp_owner() {
            hp->pinter.store(nullptr);
            hp->id.store(std::thread::id());
        }
    };
    //////////////////////////////////////////////////
    // help functions for reclaim
    template<typename U>
    void do_delete(void* p) {
        // call the correct destructor
        delete static_cast<U*>(p);
    }
    struct data_to_reclaim {
        // general purpose, can handle any pointer type
        void* data;
        std::function<void(void*)> deleter;
        data_to_reclaim* next;
        template<typename U>
        data_to_reclaim(U* p) : data(p), 
            deleter(&do_delete<U>), next(0) {}
        ~data_to_reclaim() {
            deleter(data);
        }
    };
    std::atomic<data_to_reclaim*> nodes_to_reclaim;
    void add_to_reclaim_list(data_to_reclaim* node) {
        node->next = nodes_to_reclaim.load();
        while(!nodes_to_reclaim.compare_exchange_weak(node->next, node));
    }
    //////////////////////////////////////////////////
    // help functions for pop
    std::atomic<void*>& get_hazard_pointer_for_current_thread() {
        // each thread has its own hazard pointer 
        thread_local static hp_owner hazard;
        return hazard.get_pointer();
    }
    bool outstanding_hazard_pointers_for(void* p) {
        for(unsigned i = 0; i < max_hazard_pointers; ++i) {
            // unowned entries have nullptr
            if(hazard_pointers[i].pointer.load() == p) {
                return true;
            }
        }
        return false;
    }
    template<typename U>
    void reclaim_later(U* data) {
        add_to_reclaim_list(new data_to_reclaim(data));
    }
    void delete_nodes_with_no_hazards() {
        // claim entire list to be reclaimed. so that
        // other threads can keep on adding nodes to the list,
        // or try to reclaim.
        data_to_reclaim* current = nodes_to_reclaim.exchange(nullptr);
        while(current) {
            data_to_reclaim* const next = current->next;
            if(!outstanding_hazard_pointers_for(current->data)){
                delete current;
            } else {
                // still referenced, add it back
                add_to_reclaim_list(current);
            }
            current = next;
        }
    }
public:
    std::shared_ptr>T> pop() {
        std::atomic<void*>& hp = get_hazard_pointer_for_current_thread();
        node* old_head = head.load();
        do {
            node* temp;
            // loop until hazard pointer set to head
            do {
                temp = old_head;
                hp.store(old_head);
                old_head = head.load();
            } while(old_head != temp);
        } while (old_head && 
            // a spurious failure on xxx_weak will reset the hazard pointer
            !head.compare_exchange_strong(old_head, old_head->next));
        // clear hazard pointer once finished
        hp.store(nullptr);
        std::shared_ptr<T> res;
        if(old_head) {
            res.swap(old_head->data);
            // check if hazard pointer referenced by other threads
            if(outstanding_hazard_pointers_for(old_head)) {
                // put referenced pointer on a listed
                // to be reclaimed later
                reclaim_later(old_head);
            } else {
                delete old_head;
            }
            // scanning the whole list (max_hazard_pointers)
            // Too expensive!!!
            delete_nodes_with_no_hazards();
        }
        return res;
    }
};

// Listing 7.9 Lock-free Stack using reference count
template<typename T>
class lock_free_stack{
// this method is ok when
// std::atomic_is_lock_free(&some_shared_ptr) == true
// which is os dependent. (otherwise manually refcount)
private:
    struct node {
        std::shared_ptr<T> data;
        std::shared_ptr<node> next;
        // alternative: std::atomic_shared_ptr<node> next;
        // so that no need to use atomic_xxx functions
        node(T const& data_) : 
            data(std::make_shared<T>(data_)) {}
    };
    std::shared_ptr<node> head;
public:
    void push(T const& data) {
        std::shared_ptr<node> const new_node = 
            std::make_shared<node>(data);
        new_node->next = std::atomic_load(&head);
        while(!std::atomic_compare_exchange_weak(&head,
            &new_node->next, new_node))
            ;
    }
    std::shared_ptr<T> pop() {
        std::shared_ptr<node> old_head = std::atomic_load(&head);
        while(old_head && !std::atomic_compare_exchange_weak(&head,
            &old_head, std::atomic_load(&old_head0>next)))
            ;
        if(old_head) {
            std::atomic_store(&old_head->next, std::shared_ptr<node>());
            return old_head->data;
        }
        return std::shared_ptr<T>();
    }
    ~lock_free_stack() {
        while(pop());
    }
};

// Listing 7.14 Lock-free Queue [single-producer, single-consumer]
template<typename T>
class lock_free_queue {
private:
    struct node {
        std::shared_ptr<T> data;
        node* next;
        node() : next(nullptr) {}
    };
    std::atomic<node*> head;
    std::atomic<node*> tail;
    node* pop_head() {
        node* const old_head = head.load();
        if(old_head == tail.load())
            return nullptr;
        head.store(old_head->next);
        return old_head;
    }
public:
    lock_free_queue():
        head(new node()), tail(head.load()) {}
    lock_free_queue(const lock_free_queue &) = delete;
    lock_free_queue &operator=(const lock_free_queue &) = delete;
    ~lock_free_queue() {
        while(node* const old_head = head.load()) {
            head.store(old_head->next);
            delete old_head;
        }
    }

    // happens-before: push -> pop
    std::shared_ptr<T> pop() {
        node* old_head = pop_head();
        if(!old_head) {
            return std::shared_ptr<T>();
        }
        // sequence after pop head tail.load()
        std::shared_ptr<T> const res(old_head->data);
        delete old_head;
        return res;
    }
    void push(T new_value) {
        std::shared_ptr<T> new_data(std::make_shared<T>(new_value));
        node* p = new node();
        node* const old_tail = tail.load();
        // sequence before tail.store
        old_tail->data.swap(new data);
        old_tail->next = p;
        // sync with pop_head tail.load()
        tail.store(p);
    }
};

// Listing 7.16 Lock-free Queue using refcount
// [multiple-producer, multiple-consumer]
template<typename T>
class lock_free_queue {
private:
    struct node;
    struct counted_node_ptr {
        int external_count;
        node* ptr;
    };
    std::atomic<counted_node_ptr> head;
    std::atomic<counted_node_ptr> tail;
    // need to update together as a single entity
    struct node_counter {
        // keep total counter size 32 bits
        unsigned internal_count:30;
        // need only 2 bits because at most 2 counters
        unsigned external_counters:2;
    };
    struct node {
        std::atomic<T*> data;
        std::atomic<node_counter> count;
        counted_node_ptr next;
        node() {
            node_counter new_count;
            new_count.internal_count = 0;
            // referenced from tail & previous' next
            new_count.external_counters = 2;
            count.store(new_count);

            next.ptr = nullptr;
            next.external_count = 0;
        }
        void release_ref() {
            node_counter old_counter = 
                count.load(std::memory_order_relaxed);
            node_counter new_counter;
            do {
                new_counter = old_counter;
                --new_counter.internal_count;
                // ensure is decreased successfully
            } while(!count.compare_exchange_strong(
                old_counter, new_counter,
                std::memory_order_acquire, std:memory_order_relaxed));
            if(!new_counter.internal_count &&
                !new_counter.external_counters) {
                    delete this;
            }
        }
    };
    /////////////////////////////////////////////////////
    // help functions for dealing with reference count
    static void increase_external_count (
        std::atomic<counted_node_ptr>& counter,
        counted_node_ptr& old_counter) {
            counted_node_ptr new_counter;
            do{
                new_counter = old_counter;
                ++new_counter.external_count;
            } while(!counter.compare_exchange_strong(
                old_counter, new_counter,
                std::memory_order_acquire, std::memory_order_relaxed));
            old_count.external_count = new_counter.external_count;
    }
    static void free_external_counter(counted_node_ptr& old_node_ptr) {
        node* const ptr = old_node_ptr.ptr;
        int const count_increase = old_node_ptr.external_count-2;
        node_counter old_counter;
        do {
            new_counter = old_counter;
            --new_counter.external_counters;
            // released by release_ref()
            new_counter.internal_count += count_increase;
        } while(!ptr->count.compare_exchange_strong(
            old_counter, new_counter,
            std::memory_order_acquire, std::memory_order_relaxed))
        if(!new_counter.internal_count &&
            !new_counter.external_counters) {
                delete ptr;
        }
    }
public:
    void push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_next;
        new_next.ptr = new node();
        new_next.external_count = 1;

        counted_node_ptr old_tail = tail.load();
        for(;;) {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            //  busy wait, will block other threads -> Next listing
            if(old_tail.ptr->data.compare_exchange_strong(
                old_data, new_data.get()) {
                old_tail.ptr->next = new_next;
                old_tail = tail.exchange(new_next);
                free_external_counter(old_tail);
                new_data.release();
                break;
            }
            old_tail.ptr->release_ref();
        }
    }
    std::unique_ptr<T> pop() {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            // go with free_external_counter
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if(ptr == tail.load().ptr) {
                // empty queue
                ptr->release_ref();
                return std::unique_ptr<T>();
            }
            // compare external count & pointer
            if(head.compare_exchange_strong(old_head, ptr->next)) {
                T* const res = ptr->data.exchange(nullptr);
                // If both exref freed & inref dropped to 0,
                // the node itself can be deleted
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};

// Listing 7.21 Lock-free Queue using refcount without blocking
// [multiple-producer, multiple-consumer]
template<typename T>
class lock_free_queue {
private:
    struct node {
        std::atomic<T*> data;
        std::atomic<node_counter> count;
        // make next pointer atomic
        std::atomic<counted_node_ptr> next;
    };
    void set_new_tail(counted_node_ptr &old_tail,
        counted_node_ptr const & new_tail) {
        node *const current_tail_ptr = old_tail.ptr;
        while (!tail.compare_exchange_weak(old_tail, new_tail)
            // ensure not to replace ptr if
            // another thread hassuccessfully changed it
            && old_tail.ptr == current_tail_ptr)
            ;
        if(old_tail.ptr == current_tail_ptr)
            free_external_counter(old_tail);
        else
            current_tail_ptr->release_ref();
    }
public:
    void push(T new_value) {
        std::unique_ptr<T> new_data(new T(new_value));
        counted_node_ptr new_next;
        new_next.ptr = new node();
        new_next.external_count = 1;

        counted_node_ptr old_tail = tail.load();
        for(;;) {
            increase_external_count(tail, old_tail);
            T* old_data = nullptr;
            //  To solve the busy wait
            if(old_tail.ptr->data.compare_exchange_strong(
                old_data, new_data.get()) {
                counted_node_ptr old_next = {0};
                if (!old_tail.ptr->next.compare_exchange_strong(
                    old_next, new_next)) {
                    // has been updated by another thread
                    // not need node in new_next
                    delete new_next.ptr;
                    new_next = old_next;
                }
                set_new_tail(old_tail, new_next);
                new_data.release();
                break;
            } else {
                // blocked thread help to update next ptr
                counted_node_ptr old_next = {0};
                if (old_tail.ptr->next.compare_exchange_strong(
                    old_next, new_next)) {
                    old_next = new_next;
                    // allocate another new in anticipation of
                    // managing to really push.
                    // So an efficiency allocator is required.
                    new_next.ptr = new node();
                }
                // update tailing
                set_new_tail(old_tail, new_next);
            }
        }
    }
    std::unique_ptr<T> pop() {
        counted_node_ptr old_head = head.load(std::memory_order_relaxed);
        for (;;) {
            increase_external_count(head, old_head);
            node* const ptr = old_head.ptr;
            if(ptr == tail.load().ptr) {
                return std::unique_ptr<T>();
            }
            // load next ptr using default memory_order_seq_cst
            counted_node_ptr next = ptr->next.load();
            if(head.compare_exchange_strong(old_head, next)) {
                T* const res = ptr->data.exchange(nullptr);
                free_external_counter(old_head);
                return std::unique_ptr<T>(res);
            }
            ptr->release_ref();
        }
    }
};

// Listing 8.1 Recursive Parallelism
template<typename T>
struct sorter {
    struct chunk_to_sort {
        std::list<T> data;
        std::promise<std::list<T> > promise;
    };
    // group the stack of unsorted chunks
    thread_safe_stack<chunk_to_sort> chunks;
    std::vector<std::thread> threads;
    unsigned const max_thread_count;
    std::atomic<bool> end_of_data;
    sorter() :
        max_thread_count (std::thread::hardware_concurrency() -1),
        end_of_data(false)
    {}
    // tidy up threads
    ~sorter() {
        // signal all threads to terminate the loop
        end_of_data = true;
        for (unsigned i = 0; i < thread.size(), ++i) {
            // wait thread to finish
            threads[i].join();
        }
    }

    void try_sort_chunk() {
        boost::shared_ptr<chunk_to_sort> chunk = chunks.pop();
        if(chunk) {
            sort_chunk(chunk);
        }
    }
    // main algorithm
    std::list<T> do_sort(std::list<T>& chunk_data) {
        if(chunk_data.empty()) {
            return chunk_data;
        }
        std::list<T> result;
        result.splice(result.begin(), chunk_data, chunk_data.begin());
        T const& partition_val = *result.begin();
        // partition data
        typename std::list<T>::iterator divide_point =
            std::partition(chunk_data.begin(), chunk_data.end(),
            [&](T const& value){ return value < partition_val;} );
        chunk_to_sort new_lower_chunk;
        new_lower_chunk.data.splice(new_lower_chunk.data.end(),
            chunk_data, chunkdata.begin(), divide_point);
        
        std::future<std::list<T> > new_lower =
            new_lower_chunk.promise.get_future();
        // push divided data onto the stack
        chunks.push(std::move(new_lower_chunk));
        // spawn a new thread if have vacant processors
        if(threads.size() < max_thread_count) {
            thread.push_back(std::thread(&sorter<T>::sort_thread, this));
        }
        std::list<T> new_higher(do_sort(chunk_data));
        result.splice(result.end(), new_higher);
        
        // wait for lower part sorting done
        while(new_lower.wait_for<std::chrono::seconds(0)) !=
            std::future_Status::ready) {
            // help to process chunks from stack in current thread
            // while waiting
            try_sort_chunk();
        }
        result.splice(result.begin(), new_lower.get());
        return result;    
    }
    void sort_chunk(boost::shared_ptr<chunk_to_sort> const& chunk) {
        // store the result in the promise, 
        // ready to be picked up by the thread posted this chunk
        chunk->promise.set_value(do_sort(chunk->data));
    }
    void sort_thread() {
        while(!end_of_data) {
            try_sort_chunk();
            // yield to other threads so that can do more work
            std::this_thread::yield();
        }
    }
};

template<typename T>
std::list<T> parallel_quick_sort(std::list<T> input) {
    if(input.empty()) {
        return input;
    }
    sorter<T> s;
    // return sorted data & destroy sorter
    return s.do_sort(input);
}

// HOWTO: Design concurrent code.
// PERFORMANCE:
// Processor account -> hardware_concurrency();
// Data contention & cache ping-pong -> Less mutex (same mem location);
// False sharing cache line -> 
//     far apart data accessed by seperated threads
//     (hardware_destructive_interference_size());
// Data proximity & cache miss ->
//     put data accessed by the same thread close together in mem;
// Oversubscription & task switch -> 
//     less threads, other division, busy wait, etc.
// SCALABILITY:
// More cores, more improvement, Amdahl's law: N = 1 / (fs + (1 - fs) / N)
//     -> Less process time (responsiveness) / more processed data (hide latency)
// CORRECTNESS:
// Exception safety -> shared_ptr, packaged_task& future, jthread etc.

// Listing 8.8 Divide data recursively: std::for_each
template<typename Iterator, typename Func>
void parallel_for_each(Iterator first, Iterator last, Func f) {
    unsigned long const length = std::distance(first, last);
    if(!length)
        return;
    unsigned long const min_per_thread = 25;
    if(length < 2 * min_per_thread) {
        std::for_each(first, last, f);
    } else {
        Iterator const mid_point = first + length / 2;
        std::future<void> first_part =
            std::async(&parallel_for_each<Iterator, Func>,
                first, mid_point, f);
        parallel_for_each(mid_point, last, f);
        first_part.get();
    }
}

// Listing 8.9 Divide data before processing: std::for_each
template<typename Iterator, typename MatchType>
Iterator parallel_find(Iterator first, Iterator last, MatchType match) {
    struct find_element {
        void operator()(Iterator begin, Iterator end, MatchType match,
            std::promise<Iterator>* resutl, std::atomic<bool>* done_flag) {
            try {
                for(; (begin != end) && !done_flag->load(); ++begin) {
                    if(*begin == match) {
                        result->set_value(begin);
                        done_flag->store(true);
                        return;
                    }
                }
            } catch(...) {
                try {
                    result->set_exception(std::current_exception());
                    done_flag->store(true);
                } catch(...) {}
            }   
        }
    };
    unsigned long const length = std::distance(first, last);
    if(!length)
        return last;
    unsigned long const min_per_thread = 25;
    unsigned long const max_threads = (length + min_per_thread - 1) / min_per_thread;
    unsigned long const hardware_threads = std::thread::hardware_concurrency();
    unsigned long const num_threads = min(hardware_threads? hardware_threads : 2, max_threads);
    unsigned long const block_size = length / num_threads;

    std::promise<Iterator> result;
    std::atomic<bool> done_flag(false);
    std::vector<std::thread> threads(num_threads - 1);

    {
        join_threads joiner(threads);
        Iterator block_start = first;
        for(unsigned long i = 0; i < num_threads - 1; ++i) {
            Iterator block_end = block_start;
            std::advance(block_end, block_size);
            threads[i] = std::thread(find_element(),
                block_start, block_end, match,
                &result, &done_flag);
            block_start = block_end;
        }
        find_element()(block_start, last, match, &result, &done_flag);
    }

    if(!done_flag.load()) {
        return last;
    }
    return result.get_future().get();
}

// Listing 8.13 Divide tasks: parallel sum
struct barrier {
    std::atomic<unsigned> count;
    std::atomic<unsigned> spaces;
    std::atomic<unsigned> generation;
    barrier(unsigned count_) :
        count(count_), spaces(count_), generation(0) {}
    void wait() {
        unsigned const gen = generation.load();
        // available seats decrease to 0, reset
        if(!--spaces) {
            spaces = count.load();
            ++generation;
        } else {
            // spinlock to wait
            while(generation.load() == gen) {
                std::this_thread::yield();
            }
        }
    }
    void done_waiting() {
        --count;
        if(!--spaces) {
            spaces = count.load();
            ++generation;
        }
    }
};

template<typename Iterator>
void parallel_partial_sum(Iterator first, Iterator last) {
    typedef typename Iterator::value_type value_type;
    struct process_element {
        // barrier is optional. It is useful for SIMD.
        void operator()(Iterator first, Iterator last,
            std::vector<value_type>& buffer, unsigned i, barrier& b) {
                value_type& ith_element = *(first+i);
                bool update_source = false;
                // recursively add with stride O(logN)
                // make full use of processor cores, especially
                // when core is more than element
                for(unsigned step = 0, stride = 1;
                    stride <= i; ++step, stride *= 2) {
                    value_type const& source = (step % 2)? buffer[i] : ith_element;
                    value_type& dest = (step % 2)? ith_element : buffer[i];
                    value_type &addend = (step % 2)? buffer[i - stride] : *(first + i - stride);
                    // update end data to enable threads afterwords
                    dest = source + added;
                    update_source = !(step % 2);
                    // wait until addend is available
                    b.wait();
                }
                if(update_source) {
                    // update origin data
                    ith_element = buffer[i];
                }
                b.done_waiting();
            }
    };
    unsigned long const length = std::distance(first, last);
    if (length <= 1)
        return;
    std::vector<value_type> buffer(length);
    barrier b(length);
    std::vector<std::thread> threads(length - 1);
    join_threads joiner(threads);

    Iterator block_start = first;
    for (unsigned long i = 0; i < (length - 1); ++i) {
        threads[i] = std::thread(process_element(),
            first, last, std::ref(buffer), i , std::ref(b));
    }
    // store result in last element (i.e end() -1)
    process_element()(first, last, buffer, length - 1, b);
}

// Listing 9.2 Thread Pool [Waitable Tasks]
class function_wrapper {
    struct impl_base {
        virtual void call() = 0;
        virtual ~impl_base() {}
    };
    std::unique_ptr<impl_base> impl;
    template<typename F>
    struct impl_type : impl_base {
        F f;
        impl_type(F&& f_) : f(std::move(f_)) {}
        void call() {f();}
    };
public:
    // this is used for std::packaged_task,
    // which is not copyable
    template<typename F>
    function_wrapper(F&& f) :
        impl(new impl_type<F>(std::move(f))) {}
    void operator()() {impl->call();}
    function_wrapper() = default;
    function_wrapper(function_wrapper&& other):
        impl(std::move(other.impl)) {}
    function_wrapper& operator=(function_wrapper&& other) {
        impl = std::move(other.impl);
        return *this;
    }
    function_wrapper(const function_wrapper&) = delete;
    function_wrapper(function_wrapper&) = delete;
    function_wrapper& operator=(const function_wrapper&) = delete;
};
class thread_pool{
    std::atomic<bool> done;
    // thread safe queue from listing 6.x
    // use function_wrapper rather than std::function
    threadsafe_queue<function_wrapper> work_queue;
    std::vector<std::thread> threads;
    join_threads joiner;
    void worker_thread() {
        while(!done) {
            function_wrapper task;
            if(work_queue.try_pop(task)) {
                task();
            } else {
                std::this_thread::yield();
            }
        }
    }
public:
    thread_pool() : done(false), joiner(threads) {
        unsigned const thread_count = std::thread::hardware_concurrency();
        try {
            for(unsigned i = 0; i < thread_count; ++i) {
                threads.push_back(std::thread(
                    &thread_pool::worker_thread, this
                ));
            }
        } catch(...) {
            done = true;
            throw;
        }
    }
    ~thread_pool() {
        done = true;
    }

    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type> // hold the return value of task
        submit(FunctionType f) {
        // the return type of the supplied function f
        typedef typename std::result_of<FunctionType()>::type
            result_type;
        std::packaged_task<result_type()> task(std::move(f));
        std::future<result_type> res(task.get_future());
        work_queue.push(std::move(task));
        return res;
    }  
};
// Sample usage
// template<typename Iterator, typename T>
// T parallel_accumulate(Iterator first, Iterator last, T init) {
//     unsigned long const length = std::distance(first, last);
//     if(distance)
//         return init;
//     unsigned long const block_size = 25;
//     unsigned long const num_blocks = (length + block_size - 1) / block_size;
//
//     std::vector<future<T> > futures(num_blocks - 1);
//     thread_pool pool;
//     Iterator block_start = first;
//     for(unsigned long i = 0; i < (num_blocks - 1); ++i) {
//         Iterator block_end = block_start;
//         std::advance(block_end, block_size);
//         futures[i] = pool.submit([=]{
//             accumulate_block<Iterator, T>()(block_start, block_end);
//         });
//         block_start = block_end;
//     }
//     T last_result = accumulate_block<Iterator, T>()(block_start, last);
//     T result = init;
//     for(unsigned long i = 0; i < (num_blocks - 1); ++i) {
//         result += futures[i].get();
//     }
//     result += last_result;
//     return result;
// }

// Listing 9.6 Thread Pool [Thread-local Queue, Dependent Tasks]
class thread_pool {
    std::atomic<bool> done = false;
    std::vector<std::thread> threads;
    join_threads joiner;
    threadsafe_queue<function_wrapper> pool_work_queue;
    // only contained by threads in thread pool
    typedef std::queue<function_wrapper> local_queue_type;
    // initialize  local_work_queue before processing
    static thread_local std::unique_ptr<local_queue_type>
        local_work_queue;
    void worker_thread() {
        local_work_queue.reset(new local_queue_type);
        while(!done) {
            run_pending_task();
        }
    }
public:
    thread_pool() : joiner(threads) {
        unsigned const thread_count = 
            std::thread::hardware_concurrency();
        try {
            for(unsigned i = 0; i < thread_count; ++i) {
                threads.push_back(std::thread(
                    &thread_pool::worker_thread, this
                ));
            }
        } catch(...) {
            done = true;
            throw;
        }
    }
    ~thread_pool() {
        done = true;
    }
    // This requires manually distribute tasks in balance
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
        submit(FunctionType f) {
        typedef typename std::result_of<FunctionType()>::type result_type;
        std::packaged_task<result_type()> task(f);
        std::future<result_type> res(task.get_future());
        if(local_work_queue) {
            local_work_queue->push(std::move(task));
        } else {
            pool_work_queue.push(std::move(task));
        }
        return res;
    }
    void run_pending_task() {
        function_wrapper task;
        if(local_work_queue && !local_work_queue->empty()) {
            task = std::move(local_work_queue->front());
            local_work_queue->pop();
            task();
        } else if(pool_work_queue.try_pop(task)) {
            task();
        } else {
            std::this_thread::yield();
        }
    }
};

// Listing 9.8 Thread Pool [Work Stealing]
// Sample work-stealing queue
class work_stealing_queue {
private:
    typedef function_wrapper data_type;
    std::deque<data_type> _queue;
    mutable std::mutex _mutex;
public:
    work_stealing_queue() {}
    work_stealing_queue(const work_stealing_queue&) = delete;
    work_stealing_queue& operator=(const work_stealing_queue&) = delete;

    void push(data_type data){
        std::lock_guard<std::mutex> lock(_mutex);
        _queue.push_front(std::move(data));
    }
    bool empty() {
        std::lock_guard<std::mutex> lock(_mutex);
        return _queue.empty();
    }
    // LIFO, cache friendly
    bool try_pop(data_type& res) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_queue.empty())
            return false;
        res = std::move(_queue.front());
        _queue.pop_front();
        return true;   
    }
    bool try_steal(data_type& res) {
        std::lock_guard<std::mutex> lock(_mutex);
        if(_queue.empty())
            return false;
        res = std::move(_queue.back());
        _queue.pop_back();
        return true;
    }
};
class thread_pool {
    typedef function_wrapper task_type;
    std::atomic_bool done;
    threadsafe_queue<task_type> pool_work_queue;
    std::vector<std::unique_ptr<work_stealing_queue> > queues;

    std::vector<std::thread> threads
    join_threads joiner;
    static thread_local work_stealing_queue* local_work_queue;
    static thread_local unsigned my_index;
    void worker_thread(unsigned my_index_) : my_index(my_index_) {
        local_work_queue = queues[my_index].get();
        while(!done) {
            run_pending_task();
        }
    }
    bool pop_task_from_local_queue(task_type& task) {
        return local_work_queue && local_work_queue->try_pop(task);
    }
    bool pop_task_from_pool_queue(task_type& task) {
        return pool_work_queue.try_pop(task);
    }
    bool pop_task_from_other_thread_queue(task_type& task) {
        // Start from the work_queue in next queue.
        for(unsigned i = 0; i < queues.size(); ++i) {
            unsigned const index = (my_index + i + 1) % queues.size();
            if(queues[index]->try_steal(task))
                return true;
        }
        return false;
    }
public:
    thread_pool() : done(false), joiner(threads) {
        // TODO: dynamically resizing  to optimize CPU usage
        // (especially when threads are blocked waiting for I/O, mutex lock. etc.)
        unsigned const thread_count = std::thread::hardware_concurrency();
        try {
            for(unsigned i = 0; i < thread_count; ++i) {
                queues.push_back(std::unique_ptr<work_stealing_queue>(
                    new work_stealing_queue));
                threads.push_back(
                    std::thread(&thread_pool::worker_thread, this, i));
            }
        } catch (...) {
            done = true;
            throw;
        }
    }
    ~thread_pool() {
        done = true;
    }

    // Threads in thread pool can also submit generated task itself
    template<typename FunctionType>
    std::future<typename std::result_of<FunctionType()>::type>
        submit(FunctionType f) {
        typedef typename std::future<typename std::result_of<FunctionType()>::type>
            result_type;
        std::package_task<result_type()> task(f);
        std::future<result_type> res(task.get_future());
        if (local_work_queue) {
            local_work_queue->push(std::move(task));
        } else {
            pool_work_queue.push(std::move(task));
        }
        return res;
    }
    void run_pending_task() {
        task_type task;
        if(pop_task_from_local_queue(task) ||
           pop_task_from_pool_queue(task) ||
           pop_task_from_other_thread_queue(task)) {
            task();
        } else {
            std::this_thread::yield();
        }
    }
};

// Listing 9.11 Interruptable Thread: std::condition_variable
// Can be extended with std::condition_variable_any
class interrupt_flag {
    std::atomic<bool> flag;
    std::condition_variable* thread_cond;
    //std::condition_variable_any* thread_cond_any = nullptr;
    std::mutex set_clear_mutex;
public:
    interrupt_flag(): thread_cond(nullptr) {}
    void set() {
        flag.store(true, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        if(thread_cond) {
            thread_cond->notify_all();
        }
        // else if(thread_cond_any){
        //     thread_cond_any->notify_call();
        // }
    }
    bool is_set() const {
        return flag.load(std::memory_order_relaxed);
    }
    void set_condition_variable(std::condition_variable& cv) {
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = &cv;
    }
    void clear_condition_variable(){
        std::lock_guard<std::mutex> lk(set_clear_mutex);
        thread_cond = nullptr;
    }
    // template<typename Lockable>
    // void wait(std::condition_variable_any& cv, Lockable& lk) {
    //     struct custom_lock {
    //         interrupt_flag* self;
    //         Lockable& lk;
    //         custom_lock(interrupt_flag* self_,
    //             std::condition_variable_any& cond, Lockable& lk_) :
    //             self(self_), lk(lk_) {
    //             self->set_clear_mutex.lock();
    //             self->thread_cond_any = &cond;
    //         }
    //         void unlock() {
    //             lk.unlock();
    //             self->set_clear_mutex.unlock();
    //         }
    //         void lock() {
    //             std::lock(self->set_clear_mutex, lk);
    //         }
    //         ~custom_lock() {
    //             self->thread_cond_any = nullptr;
    //             selkf->set_clear_mutex.unlock();
    //         }
    //     };
    //     custom_lock cl(this, cv, lk);
    //     interruption_point();
    //     cv.wait(cl);
    //     interruption_point();
    // }

    struct clear_cv_on_destruct {
        ~clear_cv_on_destruct() {
            this_thread_interrupt_flag.clear_condition_variable();
        }
    };
};
class thread_interrupted : public std::exception {
    // blablabla
};

thread_local interrupt_flag this_thread_interrupt_flag;
class interruptible_thread {
    std::thread internal_thread;
    interrupt_flag* flag;
public:
    template<typename FunctionType>
    interruptible_thread(FunctionType f) {
        // pass by a reference to the local promise
        std::promise<interrupt_flag*> p;
        internal_thread = std::thread([f, &p]{
            p.set_value(&this_thread_interrupt_flag);
            // detect & handling interruption
            try
            {
                f();
            }
            catch (const thread_interrupted &e)
            {
                std::cerr << e.what() << '\n';
                handle_interruption();
            }
        });
        // wait for the future associated with the promise ready
        flag = p.get_future().get();
    }
    void interrupt() {
        if(flag) {
            flag->set();
        }
    }
};
void interruption_point() {
    if(this_thread_interrupt_flag.is_set()) {
        throw thread_interrupted();
    }
}

template<typename Predicate>
void interruptible_wait(std::condition_variable& cv,
    std::unique_lock<std::mutex>& lk, Predicate pred) {
    interruption_point();
    this_thread_interrupt_flag.set_condition_variable(cv);
    interrupt_flag::clear_cv_on_destruct guard;
    while(!this_thread_interrupt_flag.is_set() && !pred()) {
        cv.wait_for(lk, std::chrono::milliseconds(1));
    }
    interruption_point();
}
// template<typename Lockable>
// void interruptible_wait(std::condition_variable_any& cv, Lockable& lk) {
//     this_thread_interrupt_flag.wait(cv, lk);
// }

// Sample usage
// Listing 9.13 filesys background thread
std::mutex config_mutex;
std::vector<interruptible_thread> background_threads;
void background_thread(int disk_id) {
    while(true) {
        interruption_point();
        fs_change fsc = get_fs_changes(disk_id);
        if(fsc.has_changes()) {
            update_index(fsc);
        }
    }
}
void start_background_processing() {
    for (int i = 0, i < disks.size(), ++i){
        background_threads.push_back(
            interruptible_thread(background_thread, disks[i]));
    }
}
void main_thread() {
    start_background_processing();
    process_gui_until_exit();

    {
        std::unique_lock<std::mutex> lk(config_mutex);
        for(unsigned i = 0 ; i < background_threads.size(); ++i) {
            background_threads[i].interrupt();
        }
        // for better concurrency, signal all interruption first
        for(unsigned i = 0 ; i < background_threads.size(); ++i) {
            background_threads[i].join();
        }
    }
}

// simple once flag
std::atomic<bool> call_once_flag = false;
if (call_once_flag.load(std::memory_order_require))
    return;
call_once_flag.store(true, std::memory_order_release);

// Appendix C
// Message-Passing Framework
namespace messaging{
    // base class of message queue entries
    struct message_base {
        virtual ~message_base() {}
    };

    // each message type has a specialization
    template<typename Msg>
    struct wrapped_message : message_base {
        Msg contents;
        explicit wrapped_message(Msg const& contents_) :
            contents(contents_) {}
    };

    // message queue
    class message_queue {
        std::mutex m;
        std::condition_variable cv;
        // internal queue stores pointers to message_base
        std::queue<std::shared_ptr<message_base> > messages;
    public:
        template<typename T>
        void push(T const& msg) {
            std::lock_guard<std::mutex> lk(m);
            // wrap posted message and store pointer
            messages.push(std::make_shared<wrapped_message<T> >(msg));
            cv.notify_all()
        }
        std::shared_ptr<message_base> wait_and_pop() {
            std::unique_lock<std::mutex> lk(m);
            // block until queue isn't empty
            cv.wait(lk, [&]{return !messages.empty();});
            auto res = messages.front();
            messages.pop();
            return res;
        }
    };
    
    template<typename PreviousDispatcher, typename Msg, typename Func>
    class TemplateDispatcher {
        message_queue* msgs;
        PreviousDispatcher* prev;
        Func f;
        bool chained;
        TemplateDispatcher(TemplateDispatcher const&) = delete;
        TemplateDispatcher& operator=(TemplateDispatcher const&) = delete;

        template<typename Dispatcher, typename OtherMsg, typename OtherFunc>
        friend class TemplateDispatcher; // TemplateDispatcher are friends of each other
        void wait_and_dispatch(){
            while(true){
                auto msg = msgs->wait_and_pop();
                // If handle the message
                if(dispatch(msg))
                    break;
            }
        }
        bool dispatch(std::shared_ptr<message_base> const& msg) {
            // check the message type and call the function
            if(wrapped_message<Msg>* wrapper = 
                dynamic_cast<wrapped_message<Msg> *>(msg.get())) {
                f(wrapper->contents);
                return true;
            } else {
                // chain to the previous dispatcher
                return prev->diapatch(msg);
            }
        }
    public:
        TemplateDispatcher(TemplateDispatcher&& other):
            msgs(other.msgs), prev(other.prev), 
            f(std::move(other.f))c, hained(other.chained) {
            other.chained = true;
        }
        TemplateDispatcher(message_queue* msgs_, 
            PreviousDispatcher* prev_, Func&& f_) :
            msgs(msgs_), prev(prev_), 
            f(std::forward<Func>(f_)), chained(false) {
            prev_->chained = true;
        }
        ~TemplateDispatcher() noexcept(false) { // similar to dispatcher
            if(!chained) {
                wait_and_dispatch();
            }
        }

        template<typename OtherMsg, typename OtherFunc>
        TemplateDispatcher<TemplateDispatcher, OtherMsg, OtherFunc>
        handle(OtherFunc&& of) {
            // additional handlers can be chained
            return TemplateDispatcher<TemplateDispatcher, OtherMsg,
                OtherFunc>(msgs, this, std::forward<OtherFunc>(of));
        }

    };

    // message for closing the queue
    class close_queue{};
    class dispatcher {
        message_queue* msgs;
        bool chained;
        // dispatcher instances cannot be copied
        dispatcher(dispatcher const&) = delete;
        dispatcher& operator=(dispatcher const&) = delete;

        // allow TemplateDispatcher instances to access internals
        template<typename Dispatcher, typename Msg, typename Func>
        friend class TemplateDispatcher;
        void wait_and_dispatch() {
            // loop, wait for & dispatch messages
            while(true) {
                auto msg = msgs->wait_and_pop();
                dispatch(msg);
            }
        }
        bool dispatch(std::shared_ptr<message_base> const& msg) {
            // check for a close_queue message and throws
            if(dynamic_cast<wrapped_message<close_queue> *>(msg.get())) {
                throw close_queue();
            }
            // unhandled message
            return false;
        }
    public:
        // Dispatcher instances can be moved
        dispatcher(dispatcher&& other) : 
            msgs(other.msgs), chained(other.chained) {
            // the source shouldn't wait for messages
            other.chained = true;
        }
        explicit dispatcher(message_queue* msgs_) :
            msgs(msgs_), chained(true) {}
        // indicate exceptions might throws to
        // avoid terminating by close_queue exception
        ~dispatcher() noexcept(false) {
            if(!chained){
                wait_and_dispatch();
            }
        }
        
        template<typename Message, typename Func>
        TemplateDispatcher<dispatcher, Message, Func>
        handle(Func&& f) {
            // handle a specific type of message 
            // with a TemplateDispatcher
            return TemplateDispatcher<dispatcher, Message, Func>(
                msgs, this, std::forward<Func>(f)
            );
        }
    };

    class sender {
        // sender is a wrapper around the queue pointer
        message_queue* msgs;
    public:
        // default-constructed sender has no queue
        sender(): msgs(nullptr) {}
        // all construction from pointer to queue
        explicit sender(message_queue* msgs_): msgs(msgs_) {}

        template<typename Message>
        void send(Message const& msg) {
            if(msgs) {
                // sending pushes message on the queue
                msgs->push(msg);
            }
        }
    };

    class receiver {
        // receiver owns the queue
        message_queue msgs;
    public:
        // allow implicit conversion to a sender 
        // that references the queue.
        operator sender() {
            return sender(&msgs);
        }
        // wait for a queue creates a dispatcher
        dispatcher wait() {
            return dispatcher(&msgs);
        }
    };
}
// Sample Usage: ATM mechanism
// ATM messages
struct withdraw {
    std::string account;
    unsigned amount;
    mutable messaging::sender atm_queue;
    withdraw(std::string const& account_,
        unsigned amount_, messaging::sender atm_queue_) :
        account(account_), amount(amount_), atm_queue(atm_queue_) {}
};
struct withdraw_ok {};
struct withdraw_denied {};
struct cancel_withdrawal {
    std::string account;
    unsigned amount;
    cancel_withdrawal(std::string const& account_, unsigned amount_) :
        account(account_), amount(amount_) {}
};
struct withdrawal_processed {
    std::string account;
    unsigned amount;
    withdrawal_processed(std::string const& account_, unsigned amount_) :
        account(account_), amount(amount_) {}
};
struct card_inserted {
    std::string account;
    explicit card_inserted(std::string const &account_) : 
        account(account_) {}
};
struct digit_pressed {
    char digit;
    explicit digit_pressed(char digit_) : digit(digit_) {}
};
struct clear_last_pressed {};
struct eject_card {};
struct withdraw_pressed {
    unsigned amount;
    explicit withdraw_pressed(unsigned amount_):
    amount(amount_) {}
};
struct cancel_pressed {};
struct issue_money {
    unsigned amount;
    issue_money(unsigned amount_):
    amount(amount_) {}
};
struct verify_pin {
    std::string account;
    std::string pin;
    mutable messaging::sender atm_queue;
    verify_pin(std::string const& account_,std::string const& pin_,
    messaging::sender atm_queue_):
    account(account_),pin(pin_),atm_queue(atm_queue_) {}
};
struct pin_verified {};
struct pin_incorrect {};
struct display_enter_pin {};
struct display_enter_card {};
struct display_insufficient_funds {};
struct display_withdrawal_cancelled {};
struct display_pin_incorrect_message {};
struct display_withdrawal_options {};
struct get_balance {
    std::string account;
    mutable messaging::sender atm_queue;
    get_balance(std::string const& account_,messaging::sender atm_queue_):
    account(account_),atm_queue(atm_queue_) {}
};
struct balance {
    unsigned amount;
    explicit balance(unsigned amount_):
    amount(amount_) {}
};
struct display_balance {
    unsigned amount;
    explicit display_balance(unsigned amount_):
    amount(amount_) {}
};
struct balance_pressed {};
// ATM state machine
class atm {
    messaging::receiver incoming;
    messaging::sender bank;
    messaging::sender interface_hardware;
    void(atm::*state)();
    std::string account;
    unsigned withdrawal_amount;
    std::string pin;
    void process_withdrawal() {
        incoming.wait()
            .handle<withdraw_ok>([&](withdraw_ok const &msg) {
                interface_hardware.send(issue_money(withdrawal_amount));
                bank.send(withdrawal_processed(account, withdrawal_amount));
                state = &atm::done_processing;
            })
            .handle<withdraw_denied>([&](withdraw_denied const &msg) {
                interface_hardware.send(display_insufficient_funds());
                state = &atm::done_processing;
            })
            .handle<cancel_pressed>([&](cancel_pressed const &msg) {
                bank.send(cancel_withdrawal(account, withdrawal_amount));
                interface_hardware.send(display_withdrawal_cancelled());
                state = &atm::done_processing;
            });
    }
    void process_balance() {
        incoming.wait()
            .handle<balance>([&](balance const &msg) {
                interface_hardware.send(display_balance(msg.amount));
                state = &atm::wait_for_action;
            })
            .handle<cancel_pressed>([&](cancel_pressed const &msg) {
                state = &atm::done_processing;
            });
    }
    void wait_for_action() {
        interface_hardware.send(display_withdrawal_options());
        incoming.wait()
            .handle<withdraw_pressed>([&](withdraw_pressed const &msg) {
                withdrawal_amount = msg.amount;
                bank.send(withdraw(account, msg.amount, incoming));
                state = &atm::process_withdrawal;
            })
            .handle<balance_pressed>([&](balance_pressed const &msg) {
                bank.send(get_balance(account, incoming));
                state = &atm::process_balance;
            })
            .handle<cancel_pressed>([&](cancel_pressed const &msg) {
                state = &atm::done_processing;
            });
    }
    void verifying_pin() {
        incoming.wait()
            .handle<pin_verified>(
                [&](pin_verified const &msg) { state = &atm::wait_for_action; })
            .handle<pin_incorrect>([&](pin_incorrect const &msg) {
                interface_hardware.send(display_pin_incorrect_message());
                state = &atm::done_processing;
            })
            .handle<cancel_pressed>([&](cancel_pressed const &msg) {
                state = &atm::done_processing;
            });
    }
    void getting_pin() {
        incoming.wait()
            .handle<digit_pressed>([&](digit_pressed const &msg) {
                unsigned const pin_length = 4;
                pin += msg.digit;
                if (pin.length() == pin_length) {
                    bank.send(verify_pin(account, pin, incoming));
                    state = &atm::verifying_pin;
                }
            })
            .handle<clear_last_pressed>([&](clear_last_pressed const &msg) {
                if (!pin.empty()) {
                    pin.pop_back();
                }
            })
            .handle<cancel_pressed>([&](cancel_pressed const &msg) {
                state = &atm::done_processing;
            });
    }
    void waiting_for_card() {
        interface_hardware.send(display_enter_card());
        incoming.wait().handle<card_inserted>([&](card_inserted const &msg) {
            account = msg.account;
            pin = "";
            interface_hardware.send(display_enter_pin());
            state = &atm::getting_pin;
        });
    }
    void done_processing() {
        interface_hardware.send(eject_card());
        state = &atm::waiting_for_card;
    }
    atm(atm const &) = delete;
    atm &operator=(atm const &) = delete;

public:
    atm(messaging::sender bank_, messaging::sender interface_hardware_)
        : bank(bank_), interface_hardware(interface_hardware_) {}
    void done() { get_sender().send(messaging::close_queue()); }
    void run() {
        state = &atm::waiting_for_card;
        try {
            for (;;) {
                (this->*state)();
            }
        } catch (messaging::close_queue const &) {}
    }
    messaging::sender get_sender() { return incoming; }
};
// bank state machine
class bank_machine {
    messaging::receiver incoming;
    unsigned balance;
public:
    bank_machine() : balance(199) {}
    void done() { get_sender().send(messaging::close_queue()); }
    void run() {
        try {
            for (;;) {
                incoming.wait()
                    .handle<verify_pin>([&](verify_pin const &msg) {
                        if (msg.pin == "1937") {
                            msg.atm_queue.send(pin_verified());
                        } else {
                            msg.atm_queue.send(pin_incorrect());
                        }
                    })
                    .handle<withdraw>([&](withdraw const &msg) {
                        if (balance >= msg.amount) {
                            msg.atm_queue.send(withdraw_ok());
                            balance -= msg.amount;
                        } else {
                            msg.atm_queue.send(withdraw_denied());
                        }
                    })
                    .handle<get_balance>([&](get_balance const &msg) {
                        msg.atm_queue.send(::balance(balance));
                    })
                    .handle<withdrawal_processed>(
                        [&](withdrawal_processed const &msg) {})
                    .handle<cancel_withdrawal>(
                        [&](cancel_withdrawal const &msg) {});
            }
        }
        catch (messaging::close_queue const &) {}
    }
    messaging::sender get_sender() { return incoming; }
};
// user-interface state machine
class interface_machine
{
    messaging::receiver incoming;
public:
    void done() { get_sender().send(messaging::close_queue()); }
    void run() {
        try {
            for (;;) {
                incoming.wait()
                    .handle<issue_money>(
                        [&](issue_money const &msg) {
                        {
                            std::lock_guard<std::mutex> lk(iom);
                            std::cout << "Issuing " << msg.amount << std::endl;
                        }
                    })
                    .handle<display_insufficient_funds>(
                        [&](display_insufficient_funds const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "Insufficient funds" << std::endl;
                            }
                        })
                    .handle<display_enter_pin>(
                        [&](display_enter_pin const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "Please enter your PIN (0-9)"
                                          << std::endl;
                            }
                        })
                    .handle<display_enter_card>(
                        [&](display_enter_card const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "Please enter your card (I)"
                                          << std::endl;
                            }
                        })
                    .handle<display_balance>(
                        [&](display_balance const &msg) {
                        {
                            std::lock_guard<std::mutex> lk(iom);
                            std::cout << "The balance of your account is "
                                      << msg.amount << std::endl;
                        }
                    })
                    .handle<display_withdrawal_options>(
                        [&](display_withdrawal_options const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "Withdraw 50? (w)" << std::endl;
                                std::cout << "Display Balance? (b)"
                                          << std::endl;
                                std::cout << "Cancel? (c)" << std::endl;
                            }
                        })
                    .handle<display_withdrawal_cancelled>(
                        [&](display_withdrawal_cancelled const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "Withdrawal cancelled"
                                          << std::endl;
                            }
                        })
                    .handle<display_pin_incorrect_message>(
                        [&](display_pin_incorrect_message const &msg) {
                            {
                                std::lock_guard<std::mutex> lk(iom);
                                std::cout << "PIN incorrect" << std::endl;
                            }
                        })
                    .handle<eject_card>([&](eject_card const &msg) {
                        {
                            std::lock_guard<std::mutex> lk(iom);
                            std::cout << "Ejecting card" << std::endl;
                        }
                    });
            }
        }
        catch (messaging::close_queue &) {}
    }
    messaging::sender get_sender() { return incoming; }
};
// main driving
int main() {
    bank_machine bank;
    interface_machine interface_hardware;
    atm machine(bank.get_sender(), interface_hardware.get_sender());
    std::thread bank_thread(&bank_machine::run, &bank);
    std::thread if_thread(&interface_machine::run, &interface_hardware);
    std::thread atm_thread(&atm::run, &machine);
    messaging::sender atmqueue(machine.get_sender());
    bool quit_pressed = false;
    while (!quit_pressed)
    {
        char c = getchar();
        switch (c)
        {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            atmqueue.send(digit_pressed(c));
            break;
        case 'b':
            atmqueue.send(balance_pressed());
            break;
        case 'w':
            atmqueue.send(withdraw_pressed(50));
            break;
        case 'c':
            atmqueue.send(cancel_pressed());
            break;
        case 'q':
            quit_pressed = true;
            break;
        case 'i':
            atmqueue.send(card_inserted("acc1234"));
            break;
        }
    }
    bank.done();
    machine.done();
    interface_hardware.done();
    atm_thread.join();
    bank_thread.join();
    if_thread.join();
    
    return 0;
}
