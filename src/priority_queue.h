/* ============================================================================
 *  priority_queue.h  --  Custom min-priority queue
 * ----------------------------------------------------------------------------
 *  ORIGINAL DATA STRUCTURE PRESERVED.
 *
 *  This is the hand-written priority queue the project was built around: a
 *  sorted singly linked list of nodes. Insertion walks the list and splices the
 *  new node in order (O(n)), and extracting the minimum is O(1). It is used by
 *  Dijkstra's algorithm in city_graph.cpp -- no std::priority_queue anywhere.
 * ==========================================================================*/
#ifndef DMS_PRIORITY_QUEUE_H
#define DMS_PRIORITY_QUEUE_H

namespace dms {

class PriorityQueue {
public:
    PriorityQueue();
    ~PriorityQueue();

    /* Non-copyable: owns raw nodes (C++11 -> delete keyword available). */
    PriorityQueue(const PriorityQueue &) = delete;
    PriorityQueue &operator=(const PriorityQueue &) = delete;

    /** Insert `node` with key `priority`, keeping the list sorted ascending. */
    void push(int node, double priority);

    /** Remove and return the smallest element. Returns false when empty. */
    bool pop(int &node, double &priority);

    /** Look at the smallest element without removing it. */
    bool peek(int &node, double &priority) const;

    bool   empty() const { return head_ == 0; }
    int    size()  const { return size_; }
    void   clear();

private:
    struct PQNode {
        int     node;
        double  priority;
        PQNode *next;
    };

    PQNode *head_;
    int     size_;
};

}  /* namespace dms */

#endif  /* DMS_PRIORITY_QUEUE_H */
