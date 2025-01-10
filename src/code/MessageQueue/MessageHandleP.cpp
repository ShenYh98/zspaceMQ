#include "MessageHandle.h"

using namespace ProcessMessageQueue;

void MessageHandle::setVTopic(const std::vector<std::string>& v_recvTopic) {
    this->v_recv_topic = v_recvTopic;
}

void MessageHandle::getVTopic(std::vector<std::string>& v_recvTopic) {
    v_recvTopic = this->v_recv_topic;
}
