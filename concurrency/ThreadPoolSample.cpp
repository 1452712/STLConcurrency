#include "stdafx.h"
#include "ThreadPool.h"

namespace Thread
{
    int Run()
    {
        int size = 4;
        ThreadPool pool(size);
        
        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < size; ++j) 
                pool.PushTask(j); // Start to run background

        size = 8;
        pool.Resize(size);
        for (int i = 0; i < 10; ++i)
            for (int j = 0; j < size; ++j) 
                pool.PushTask(j);

        pool.Terminate();
        
        return 0;
    }
}