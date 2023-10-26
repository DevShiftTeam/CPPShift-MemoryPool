//
// Created by Sapir Shemer on 23/10/2023.
//

#include "../include/execution/ExecutionQueue.h"

using namespace AppShift::Execution;

ExecutionQueueResult::ExecutionQueueResult() {
    event_block = nullptr;
    start = 0;
    count = 0;
}

ExecutionQueueResult::~ExecutionQueueResult() {
    if (event_block != nullptr)
        event_block->ref_count--;
}

ExecutionQueueResult::ExecutionQueueResult(ExecutionQueueResult &&other) noexcept {
    event_block = other.event_block;
    start = other.start;
    count = other.count;
    other.event_block = nullptr;
}

ExecutionQueue::ExecutionQueue(size_t size) : _size(size) {
    first_block = reinterpret_cast<ExecutionQueueBlock *>(malloc(
            sizeof(ExecutionQueueBlock) + _size * sizeof(Callable)));
    current_block = first_block;
    first_block->next = first_block;
    first_block->size = _size;
    first_block->ref_count = 0;
}

void ExecutionQueue::push(const Callable &item) {
    std::lock_guard<std::mutex> lock(mutex);

    // Handle end of current queue block
    if (rear == current_block->size) {
        rear = 0;
        auto *next_block = current_block->next;

        // If next block is the first block or in use, then allocate new block in between
        if (next_block == first_block || next_block->ref_count != 0) {
            auto *new_block = reinterpret_cast<ExecutionQueueBlock *>(
                    malloc(sizeof(ExecutionQueueBlock) + _size * sizeof(Callable)));
            new_block->next = next_block;
            new_block->size = _size;
            new_block->ref_count = 0;
            current_block->next = new_block;
            current_block = new_block;
        }
            // Otherwise, just use the next block
        else {
            current_block = next_block;
        }
    }

    // Add to queue
    auto queue = reinterpret_cast<Callable *>(current_block + 1);
    queue[rear++] = item;
}

ExecutionQueueResult ExecutionQueue::pop(size_t count) {
    std::lock_guard<std::mutex> lock(mutex);
    ExecutionQueueResult result;

    if (isEmpty()) return result;

    result.event_block = first_block;
    result.start = front;

    // Calculate count available to determine amount of events to return
    auto size_available =
            first_block == current_block ?
            rear - front :
            first_block->size - front;
    result.count = std::min(count, size_available);

    front += result.count;
    result.event_block->ref_count++;

    // If front is at the end of the block then move to next block
    if (front == first_block->size && first_block->next != first_block) {
        front = 0;
        first_block = first_block->next;
    }

    return result;
}