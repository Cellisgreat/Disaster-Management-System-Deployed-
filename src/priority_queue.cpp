#include "priority_queue.h"

namespace dms {

PriorityQueue::PriorityQueue() : head_(0), size_(0) {}

PriorityQueue::~PriorityQueue() { clear(); }

void PriorityQueue::clear() {
    PQNode *cur = head_;
    while (cur) {
        PQNode *next = cur->next;
        delete cur;
        cur = next;
    }
    head_ = 0;
    size_ = 0;
}

void PriorityQueue::push(int node, double priority) {
    PQNode *fresh = new PQNode;
    fresh->node = node;
    fresh->priority = priority;
    fresh->next = 0;

    /* Splice into the sorted list: walk while existing priorities are smaller
     * or equal, so equal keys keep FIFO order (stable behaviour). */
    if (head_ == 0 || priority < head_->priority) {
        fresh->next = head_;
        head_ = fresh;
    } else {
        PQNode *prev = head_;
        while (prev->next && prev->next->priority <= priority) prev = prev->next;
        fresh->next = prev->next;
        prev->next = fresh;
    }
    ++size_;
}

bool PriorityQueue::pop(int &node, double &priority) {
    if (head_ == 0) return false;
    PQNode *top = head_;
    node = top->node;
    priority = top->priority;
    head_ = top->next;
    delete top;
    --size_;
    return true;
}

bool PriorityQueue::peek(int &node, double &priority) const {
    if (head_ == 0) return false;
    node = head_->node;
    priority = head_->priority;
    return true;
}

}  /* namespace dms */
