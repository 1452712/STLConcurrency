#include "stdafx.h"
#include "ThreadPool.h"

namespace Thread
{
    int Run()
    {
        int size = 16;
        ThreadPool pool(size);
        
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < size; ++j) 
                pool.PushTask(j);

        size = 32;
        pool.Resize(size);
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < size; ++j) 
                pool.PushTask(j);

        pool.Terminate();
        
        return 0;
    }
}