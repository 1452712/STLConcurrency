#include "stdafx.h"
#include "condvar2.h"

namespace condvar2 {
    std::queue<int> gQueue;
    std::mutex gQueueMutex;
    std::condition_variable gQueueCondVar;

    void Provider(int val) {
        for (int i = 0; i < 10; i++) {
            {
                lock_guard<mutex> lg(gQueueMutex);
                gQueue.push(val + i);
            }
            gQueueCondVar.notify_one();

            this_thread::sleep_for(chrono::milliseconds(val));
        }
    }

    void Consumer(int num) {
        while (true) {
            int val = 0;
            {
                unique_lock<mutex> ul(gQueueMutex);
                gQueueCondVar.wait(ul, [] { return !gQueue.empty(); /*double check*/});
                val = gQueue.front();
                gQueue.pop();
            }

            cout << "Consumer " << num << ": " << val << endl;
        }
    }

    int Run() {
        future<void> p1 = async(launch::async, Provider, 1000);
        future<void> p2 = async(launch::async, Provider, 3000);
        future<void> p3 = async(launch::async, Provider, 7000);

        future<void> c1 = async(launch::async, Consumer, 2);
        future<void> c2 = async(launch::async, Consumer, 5);

        return 0;
    }
}
