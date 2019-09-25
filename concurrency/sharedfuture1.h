#pragma once

#include <future>

namespace sharedfuture1 {
    
    int QueryNumber();

    void DoSomething(char c, std::shared_future<int> f);

    int Run();
}
