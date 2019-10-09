#include "stdafx.h"
#include "ThreadPool.h"

namespace Thread
{
    class Executer
    {
        void operator()()
        {
            for (int i = 0; i < 100; i++)
            {
                cout << "Thread: " << this_thread::get_id() << endl;
                this_thread::sleep_for(chrono::milliseconds(i));
            }
        }
    };

    int Run()
    {
        ThreadPool<Executer> pool(16);
        
        for (int i = 0; i < 10; i++)
            pool.DetachThread();

        for (int i = 0; i < 20; i++)
            pool.AppendThread();

        pool.Terminate();
    }
}