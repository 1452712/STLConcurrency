#include "stdafx.h"
#include "sharedfuture1.h"

namespace sharedfuture1 {

    int QueryNumber() {
        int num = 0;

        cout << "Read number: ";
        cin >> num;
        if (!cin) {
            throw runtime_error("No number read.");
        }

        return num;
    }

    void DoSomething(char c, std::shared_future<int> f) {
        try {
            int num = f.get();
            for (int i = 0; i < num; i++) {
                this_thread::sleep_for(chrono::milliseconds(100));
                cout.put(c).flush();
            }
        }
        catch (const exception& e) {
            cerr << "EXCEPTION in thread " << this_thread::get_id()
                << ": " << e.what() << endl;
        }
    }

    int Run() {
        try {
            shared_future<int> f = async(QueryNumber);

            future<void> f1 = async(launch::async, DoSomething, '.', f);
            future<void> f2 = async(launch::async, DoSomething, '+', f);
            future<void> f3 = async(launch::async, DoSomething, 'x', f);

            f1.get();
            f2.get();
            f3.get();
        }
        catch (const exception& e) {
            cout << "\nEXCEPTION: " << e.what() << endl;
        }
        cout << "\nDone" << endl;

        system("pause");
        return 0;
    }
}

