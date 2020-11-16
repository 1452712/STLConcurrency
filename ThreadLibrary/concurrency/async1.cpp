#include "stdafx.h"
#include "async1.h"

namespace async1 {
    int DoSomething(char c) {
        std::default_random_engine dre(c);
        std::uniform_int_distribution<int> id(10, 1000);

        for (int i = 0; i < 10; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(id(dre)));
            std::cout.put(c).flush();
        }

        return c;
    }

    int Func1() {
        return DoSomething('.');
    }

    int Func2() {
        return DoSomething('+');
    }

    int Run() {

        // Timer
        std::clock_t start;
        double duration = 0;

        /////////////////////
        // Async
        std::cout << "Starting Func1() in background"
            << " and Func2() in foreground:" << std::endl;

        start = std::clock();
        std::future<int> result1(std::async(Func1));

        int result2 = Func2();

        // Ensure this sequence.
        int result = result1.get() + result2; // .get() raise exception if any
        duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;

        std::cout << "\nResult of Func1() + Func2(): " << result 
            << " , using time: " << duration
            << " seconds" << std::endl;

        /////////////////////
        // Comparision
        std::cout << "Non-concurrency:" << std::endl;
        start = std::clock();

        result = Func1() + Func2();
        duration = (std::clock() - start) / (double)CLOCKS_PER_SEC;

        std::cout << "\nResult of Func1() + Func2(): " << result
            << " , using time: " << duration
            << " seconds" << std::endl;

        return 0;
    }
}
