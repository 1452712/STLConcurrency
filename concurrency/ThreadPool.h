#pragma once

#include <queue>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>

//pool的接口:
//提供一下创建线程，挂起线程的接口
//每个线程在pool里维护一个自己当前任务的数量
//可以被manager调用
//只要能跑，能启动，能挂起，就行了
//不要返回值，线程间同步

//TODO: 错误处理

// Reference：
// ctpl: https://github.com/vit-vit/CTPL/blob/master/ctpl_stl.h
// boost: boost.thread_pool

namespace Thread
{
    typedef struct TaskThread
    {
        std::unique_ptr<std::thread> mThread = nullptr;
        std::atomic<size_t> mThreadID = 0;
        std::atomic<size_t> mTaskNum = 0;
    }TaskThread;

    class Task
    {
    public:
        Task(TaskThread* owner) :
            mThreadOwner(owner)
        {
            mThreadOwner->mTaskNum++;
        }

        virtual ~Task()
        {
            mThreadOwner->mTaskNum--;
        }

        virtual void Execute()
        {
            // For debugging
            for (int i = 0; i < 100; i++)
            {
                cout << "Thread: " << this_thread::get_id() << endl;
                this_thread::sleep_for(chrono::milliseconds(i));
            }
        };

    private:
        TaskThread *mThreadOwner;
    };

    // Concurrency task queue.
    class TaskQueue {
    public:
        TaskQueue() {}

        ~TaskQueue() noexcept
        {
            Clear();
        }

        //void Push(const std::unique_ptr<Task> pTask) noexcept
        void Push(Task* pTask) noexcept
        {
            //{
            //std::lock_guard<std::mutex> lg(mQueueMutex);
            std::unique_lock<std::mutex> ul(mQueueMutex);
            mTaskQueue.push(pTask);
            //}
            //mQueueCondVar.notify_one();
        }

        // Execute & Remove from the queue
        bool Pop() noexcept
        {
            std::unique_lock<std::mutex> ul(mQueueMutex);
            // keep waiting?
            //mQueueCondVar.wait(ul, [&ret] { return !mQueue.empty(); /*double check*/});
            if (mTaskQueue.empty())
                return false;
            mTaskQueue.front()->Execute();
            delete mTaskQueue.front();
            mTaskQueue.pop();
            return true;
        }

        void Clear() noexcept
        {
            while (Pop())
            {
                continue;
            }
        }

    private:
        //std::thread mThread;
        //std::queue<std::unique_ptr<Task>> mTaskQueue;
        std::queue<Task*> mTaskQueue;
        std::mutex mQueueMutex;
        //std::condition_variable mQueueCondVar;
    };

    class ThreadPool {
    public:
        // TODO: copy & move constructor
        // TODO: override =
        ThreadPool(const size_t numThreads = 0) noexcept :
            mWaitingThreadCount(0),
            mFinished(false),
            mTerminated(false)
        {
            MAX_THREAD_NUM = std::thread::hardware_concurrency() * 2;
            Resize((numThreads != 0 && numThreads < MAX_THREAD_NUM) ? numThreads : MAX_THREAD_NUM);
        }

        ~ThreadPool() noexcept
        {
            Terminate();
        }

        size_t GetSize() noexcept
        {
            return mThreads.size();
        }

        void GetThread(size_t pos, TaskThread* ret)  noexcept
        {
            if (pos < mThreads.size())
                ret = mThreads[pos];
        }

        // Initialize a thread
        void SetThread(size_t pos)
        {
            std::shared_ptr<std::atomic<bool>> flag(mFlags[pos]);

            auto f = [this, pos, flag]() {
                std::atomic<bool>& _flag = *flag;
                bool isPop = mTaskQueue.Pop();
                while (true)
                {
                    while (isPop)
                    {
                        if (_flag)
                            return;
                        else
                            isPop = mTaskQueue.Pop();
                    }

                    std::unique_lock<std::mutex> ul(mThreadMutex);
                    ++mWaitingThreadCount;
                    mThreadCondVar.wait(ul, 
                        [this, &isPop, &_flag]()
                        { 
                            isPop = mTaskQueue.Pop();
                            return isPop || mFinished || _flag;
                        });
                    --mWaitingThreadCount;
                    if (!isPop)
                        return;
                }
                
            };

            if (mThreads[pos] != nullptr)
                delete mThreads[pos];
            mThreads[pos] = new TaskThread();
            mThreads[pos]->mThread = std::make_unique<std::thread>(f);
        }

        bool AppendThread()
        {
            return Resize(mThreads.size() + 1);
        }

        bool Resize(const size_t numThreads)
        {
            if (numThreads > MAX_THREAD_NUM || numThreads <= 0)
                return false;

            if (!mFinished && !mTerminated)
            {
                size_t lastNumThreads = mThreads.size();
                if (lastNumThreads <= numThreads)
                {
                    mThreads.resize(numThreads);
                    mFlags.resize(numThreads);

                    // Initialize the enlarged part
                    for (size_t i = lastNumThreads; i < numThreads; ++i)
                    {
                        mFlags[i] = std::make_shared<std::atomic<bool>>(false);
                        SetThread(i);
                    }
                }
                else
                {
                    // Detach the decreased part
                    {
                        std::unique_lock<std::mutex> ul(mThreadMutex);
                        for (size_t i = lastNumThreads - 1; i >= numThreads; --i)
                        {
                            *mFlags[i] = true;  // this thread will finish
                            (mThreads[i]->mThread)->detach();
                        }
                        mThreadCondVar.notify_all();
                    }
                    mThreads.resize(numThreads);
                    mFlags.resize(numThreads);
                }
                return true;
            }
            return false;
        }

        // TODO
        void Terminate(bool forceStop = false) {
            if (forceStop)
            {
                if (mTerminated)
                    return;
                mTerminated = true;

                for (size_t i = 0; i < mThreads.size(); ++i)
                {
                    *mFlags[i] = true;
                }
                mTaskQueue.Clear();
            }
            else
            {
                if (mTerminated || mFinished)
                    return;
                mFinished = true;
            }

            {
                std::unique_lock<std::mutex> ul(mThreadMutex);
                mThreadCondVar.notify_all();  // stop all waiting threads

                for (size_t i = 0; i < mThreads.size(); ++i)
                {
                    // Wait until task is done
                    if ((mThreads[i]->mThread)->joinable())
                        (mThreads[i]->mThread)->join();
                }
            }

            mTaskQueue.Clear();
            mThreads.clear();
            mFlags.clear();
        }

        void PushTask(size_t pos)
        {
            //mTaskQueue.Push(std::make_unique<Task>(mThreads[pos]));
            Task* pTask = new Task(mThreads[pos]);
            mTaskQueue.Push(pTask);
            std::unique_lock<std::mutex> ul(mThreadMutex);
            mThreadCondVar.notify_one();
        }

        // Execute & Remove a task
        void PopTask()
        {
            mTaskQueue.Pop();
        }

    private:
        size_t MAX_THREAD_NUM;
        std::vector<TaskThread*> mThreads;
        TaskQueue mTaskQueue;
        std::mutex mThreadMutex;
        std::condition_variable mThreadCondVar;
        std::atomic<size_t> mWaitingThreadCount;

        // Flags
        std::vector<std::shared_ptr<std::atomic<bool>>> mFlags;
        std::atomic<bool> mFinished;
        std::atomic<bool> mTerminated;
    };
}

namespace Thread
{
    // Sample run
    int Run();
}