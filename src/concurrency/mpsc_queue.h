#pragma once

#include <memory>
#include <atomic>

// Multi-Producer Single-Consumer unbounded queue.

template<typename T>
class MpscQueue {
    struct Node {
        T data {};
        std::atomic<Node*> next {nullptr};
    };
    std::atomic<Node*> tail_;  // tail_ is the insertion point; producers race to update it
    Node* head_;  // head_ is the consumption point; only the consumer ever reads/writes it. No atomic needed — single-threaded access.

public:
    MpscQueue() {
        Node* sentinel = new Node{};
        tail_.store(sentinel, std::memory_order_relaxed);
        head_ = sentinel;
    }

    ~MpscQueue() {
        T ignored;
        while (pop(ignored)) {}
        delete head_;
    }

    MpscQueue(const MpscQueue&) = delete;
    MpscQueue& operator=(const MpscQueue&) = delete;

    // Called by ANY thread (multiple producers safe).
    void push(T value) {
        Node* new_node = new Node{std::move(value)};
        Node* prev_tail = tail_.exchange(new_node, std::memory_order_acq_rel);
        prev_tail->next.store(new_node, std::memory_order_release);
    }

    // Called ONLY by the single consumer thread.
    bool pop(T& out) {
        Node* next = head_->next.load(std::memory_order_acquire);
        if (!next) return false;

        out = std::move(next->data);
        Node* old_head = head_;
        head_ = next;
        delete old_head;
        return true;
    }

    bool empty() const {
        return head_->next.load(std::memory_order_acquire) == nullptr;
    }
};