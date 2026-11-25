#include "DoublyLinkedList.h"

namespace scms {

DoublyLinkedList::~DoublyLinkedList() { clear(); }

void DoublyLinkedList::clear() {
    Node* n = head_;
    while (n) {
        Node* next = n->next;
        delete n;          // unique_ptr inside deletes the Complaint (virtual destructor)
        n = next;
    }
    head_ = tail_ = nullptr;
    size_ = 0;
}

Complaint* DoublyLinkedList::insertBack(std::unique_ptr<Complaint> c) {
    Node* n = new Node(std::move(c));
    n->prev = tail_;
    if (tail_) tail_->next = n; else head_ = n;
    tail_ = n;
    ++size_;
    return n->data.get();
}

Complaint* DoublyLinkedList::insertFront(std::unique_ptr<Complaint> c) {
    Node* n = new Node(std::move(c));
    n->next = head_;
    if (head_) head_->prev = n; else tail_ = n;
    head_ = n;
    ++size_;
    return n->data.get();
}

DoublyLinkedList::Node* DoublyLinkedList::findNode(long long id) const {
    lastVisited_ = 0;
    for (Node* n = head_; n; n = n->next) {
        ++lastVisited_;
        if (n->data->id() == id) return n;
    }
    return nullptr;
}

std::unique_ptr<Complaint> DoublyLinkedList::remove(long long id) {
    Node* n = findNode(id);
    if (!n) return nullptr;
    // unlink: the two neighbours point past n
    if (n->prev) n->prev->next = n->next; else head_ = n->next;
    if (n->next) n->next->prev = n->prev; else tail_ = n->prev;
    std::unique_ptr<Complaint> out = std::move(n->data);
    delete n;
    --size_;
    return out;
}

Complaint* DoublyLinkedList::search(long long id) const {
    Node* n = findNode(id);
    return n ? n->data.get() : nullptr;
}

bool DoublyLinkedList::update(long long id, const std::function<void(Complaint&)>& fn) {
    Node* n = findNode(id);
    if (!n) return false;
    fn(*n->data);
    return true;
}

void DoublyLinkedList::traverseForward(const std::function<void(Complaint&)>& fn) const {
    for (Node* n = head_; n; n = n->next) fn(*n->data);
}

void DoublyLinkedList::traverseBackward(const std::function<void(Complaint&)>& fn) const {
    for (Node* n = tail_; n; n = n->prev) fn(*n->data);
}

std::vector<Complaint*> DoublyLinkedList::toVector(bool newestFirst) const {
    std::vector<Complaint*> v;
    v.reserve(size_);
    if (newestFirst) traverseBackward([&](Complaint& c) { v.push_back(&c); });
    else traverseForward([&](Complaint& c) { v.push_back(&c); });
    return v;
}

} // namespace scms
