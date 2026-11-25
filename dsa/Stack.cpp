#include "Stack.h"
#include "../backend/Exceptions.h"

namespace scms {

Stack::Stack(std::size_t capacity) : data_(new StackEntry[capacity == 0 ? 1 : capacity]), capacity_(capacity == 0 ? 1 : capacity) {}

Stack::~Stack() { delete[] data_; }

void Stack::push(const StackEntry& e) {
    if (isFull()) throw CapacityException("Stack overflow: capacity " + std::to_string(capacity_) + " reached.");
    data_[top_++] = e;
}

void Stack::pushBounded(const StackEntry& e) {
    if (isFull()) {
        for (std::size_t i = 1; i < capacity_; ++i) data_[i - 1] = data_[i];   // drop the oldest (bottom)
        --top_;
    }
    data_[top_++] = e;
}

StackEntry Stack::pop() {
    if (isEmpty()) throw EmptyStructureException("Stack underflow: the stack is empty.");
    return data_[--top_];
}

const StackEntry& Stack::peek() const {
    if (isEmpty()) throw EmptyStructureException("Stack is empty - nothing on top.");
    return data_[top_ - 1];
}

std::vector<StackEntry> Stack::topToBottom() const {
    std::vector<StackEntry> v;
    for (std::size_t i = top_; i > 0; --i) v.push_back(data_[i - 1]);
    return v;
}

} // namespace scms
