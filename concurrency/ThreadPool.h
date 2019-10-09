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

// refer to https://github.com/vit-vit/CTPL/blob/master/ctpl_stl.h
// refer to boost.thread_pool

namespace Thread
{
    // Threadlocal
    template <typename Task>
    class TaskThread {
    public:

        std::thread mThread;

        //TODO Constructor

        ~TaskThread() noexcept
        {
            Clear();
        }

        void Push(Task const & value) noexcept
        {
            //{
            //std::lock_guard<std::mutex> lg(mQueueMutex);
            std::unique_lock<std::mutex> ul(mQueueMutex);
            mTaskQueue.push(val);
            //}
            //mQueueCondVar.notify_one();
        }

        bool Pop(Task & value) noexcept
        {
            std::unique_lock<std::mutex> ul(mQueueMutex);
            // keep waiting?
            //mQueueCondVar.wait(ul, [&ret] { return !mQueue.empty(); /*double check*/});
            if (mTaskQueue.empty())
                return false;
            value = mTaskQueue.front();
            mTaskQueue.pop();
            return true;
        }

        void Clear() noexcept
        {
            Task* task;
            while (Pop(task))
                delete task;
        }

    private:
        //std::thread mThread;
        std::queue<Task> mTaskQueue;
        std::mutex mQueueMutex;
        //std::condition_variable mQueueCondVar;
    };

    template <typename Task>
    class ThreadPool {
    public:
        // TODO: copy & move constructor
        // TODO: override =
        ThreadPool(size_t const numThreads = 0) noexcept :
            mWaitingThreadCount(0),
            mFinished(false),
            mTerminated(false)
        {
            MAX_THREAD_NUM = std::thread::hardware_concurrency() * 2;
            Initialize((numThreads != 0 && numThreads < MAX_THREAD_NUM) ? numThreads : MAX_THREAD_NUM);
        }

        ~ThreadPool() noexcept
        {
            Terminate();
        }

        size_t GetSize() noexcept
        {
            return mThreads.size();
        }

        void GetThread(size_t pos, TaskThread<Task> && ret)  noexcept
        {
            if (pos < mThreads.size())
                ret = mThreads[pos];
        }

        //TODO
        void SetThread(size_t pos)
        {
            std::shared_ptr<std::atomic<bool>> flag(mFlags[pos]);
            auto pTaskThread(mThreads[pos]);
            auto f = [this, pos, flag]() {
                std::atomic<bool>& _flag = *flag;
                Task* task = nullptr;
                bool isPop = pTaskThread.Pop(task);
                while (true)
                {
                    // In case the thread contains tasks
                    while (isPop)
                    {
                        //Execution
                        (*task)();
                        delete task;

                        if (_flag)
                            return;
                        else
                            isPop = pTaskThread.Pop(task);
                    }

                    std::unique_lock<std::mutex> ul(mThreadMutex);
                    ++mWaitingThreadCount;
                    mThreadCondVar.wait(ul, [this, &task, &isPop, &_flag]() { isPop = pTaskThread.Pop(task); return isPop || mFinished || _flag; });
                    --mWaitingThreadCount;
                    if (!isPop)
                        return;  // if the queue is empty and this->isDone == true or *flag then return
                }
            };
            (mThreads[i]->mThread).reset(new std::thread(f)); // compiler may not support std::make_unique()
        }

        // TODO
        bool AppendThread()
        {
            if (!mFinished && !mTerminated)
                return false;
            if (mThreads.size() >= MAX_THREAD_NUM)
                return false;

            mThreads.push_back(nullptr);
            SetThread(mThreads.size() - 1);
            mFlags.push_back(std::make_shared<std::atomic<bool>>(false));
            return true;
        }

        //TODO
        bool DetachThread()
        {
            if (!mFinished && !mTerminated)
                return false;
            if (mThreads.size() <= 0)
                return false;

            {
                std::unique_lock<std::mutex> ul(mThreadMutex);
                *mFlags[mThreads.size() - 1] = true;  // this thread will finish
                (mThreads[mThreads.size() - 1]->mThread).detach();
                mThreadCondVar.notify_all();
            }
            mThreads.pop_back();
            mFlags.pop_back();
            return true;
        }

        void Initialize(const size_t numThreads)
        {
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
                            (mThreads[i]->mThread).detach();
                        }
                        mThreadCondVar.notify_all();
                    }
                    mThreads.resize(numThreads);
                    mFlags.resize(numThreads);
                }
            }
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
            }
            for (size_t i = 0; i < mThreads.size(); ++i)
            {
                if ((mThreads[i]->mThread).joinable())
                    (mThreads[i]->mThread).join();
            }

            mThreads.clear();
            mFlags.clear();
        }

    private:
        size_t MAX_THREAD_NUM;
        std::vector<std::unique_ptr<TaskThread<Task>>> mThreads;
        std::mutex mThreadMutex;
        std::condition_variable mThreadCondVar;
        std::atomic<size_t> mWaitingThreadCount;

        // Flags
        std::vector<std::shared_ptr<std::atomic<bool>>> mFlags;
        std::atomic<bool> mFinished;
        std::atomic<bool> mTerminated;
    };
}
