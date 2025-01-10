#include "SubInfoMonitor.h"

using namespace ThreadMessageQueue;

SubInfoMonitor::SubInfoMonitor() {

}

SubInfoMonitor::~SubInfoMonitor() {

}

std::unordered_map<int, SubInfo> SubInfoMonitor::getSubInfo() {
    auto subInfoMap = MessageHandle::getInstance().getSubInfo();
    return subInfoMap;
}
