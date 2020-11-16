#include "stdafx.h"
#include "atomics1.h"

namespace atomics1 {
    long gData = 0;
    atomic<bool> gReadyFlag(false);

    void Provider() {
        cout << "<return>" << endl;
        cin.get();

        gData = 145;

        gReadyFlag.store(true);
    }

    void Consumer() {
        while (!gReadyFlag.load()) {
            cout.put('x').flush();
            this_thread::sleep_for(chrono::milliseconds(1000));
        }

        cout << "\nValue: " << gData << endl;
    }

    int Run() {
        future<void> p = async(launch::async, Provider);
        future<void> c = async(launch::async, Consumer);

        return 0;
    }
}
