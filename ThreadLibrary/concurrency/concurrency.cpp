// concurrency.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "async1.h"
#include "async3.h"
#include "sharedfuture1.h"
#include "condvar2.h"
#include "atomics1.h"

int main()
{
    //async1::Run();
    //async3::Run();
    //sharedfuture1::Run();
    //condvar2::Run();
    atomics1::Run();

    system("pause");
    return 0;
}

