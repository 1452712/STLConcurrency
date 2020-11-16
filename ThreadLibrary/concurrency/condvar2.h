#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

// Implement an concurrrency queue via Condition Variable
namespace condvar2 {

    void Provider(int val);

    void Consumer(int num);

    int Run();
}
