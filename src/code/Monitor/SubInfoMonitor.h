#pragma once

#include "MessageHandle.h"

using namespace std;

namespace ThreadMessageQueue {
    class SubInfoMonitor
    {
    public:
        SubInfoMonitor();
        ~SubInfoMonitor();

        std::unordered_map<int, SubInfo> getSubInfo();
    };
};
