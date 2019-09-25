#include "stdafx.h"
#include "async3.h"

namespace async3 {
    void DoSomething(char c) {
        default_random_engine dre(c);
        uniform_int_distribution<int> id(10, 1000);

        for (int i = 0; i <= 10; i++) {
            this_thread::sleep_for(chrono::milliseconds(id(dre)));
            cout.put(c).flush();
        }
    }

    int Run() {
        cout << "Starting 2 operations asynchronously" << endl;

        // MUST: pass by value / const ref no mutable
        future<void> f1 = async([] { DoSomething('.'); });
        future<void> f2 = async([] { DoSomething('+'); });
        // Another method:
        //auto f3 = async(launch::async, [] { DoSomething('_'); });

        if (f1.wait_for(chrono::seconds(0)) != future_status::deferred ||
            f2.wait_for(chrono::seconds(0)) != future_status::deferred) {

            while (f1.wait_for(chrono::seconds(0)) != future_status::ready &&
                f2.wait_for(chrono::seconds(0)) != future_status::ready) {
                // ......
                this_thread::yield();
            }
        }
        cout.put('\n').flush();

        try {
            f1.get();
            f2.get();
        }
        catch (const exception& e) {
            cout << "\nEXCEPTION: " << e.what() << endl;
        }
        cout << "\nDone" << endl;

        system("pause");
        return 0;
    }
}