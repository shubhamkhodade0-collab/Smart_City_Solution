// =====================================================================
// DoublyLinkedList.h - owns every ACTIVE complaint object in memory
//
// Why a doubly linked list: complaints are inserted at the tail as they
// arrive, removed from the middle when CLOSED (O(1) unlink once found),
// and the UI traverses both newest-first (backward) and oldest-first.
//
// [CO-EVIDENCE] DSA-CO2 / PROG-CO2: insert, delete, search, traverse, update
// =====================================================================
#pragma once
#include "../backend/Complaint.h"
#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace scms {

class DoublyLinkedList {
public:
    struct Node {
        std::unique_ptr<Complaint> data;
        Node* prev = nullptr;
        Node* next = nullptr;
        explicit Node(std::unique_ptr<Complaint> c) : data(std::move(c)) {}
    };

    DoublyLinkedList() = default;
    ~DoublyLinkedList();                                   // frees every node + complaint
    DoublyLinkedList(const DoublyLinkedList&) = delete;
    DoublyLinkedList& operator=(const DoublyLinkedList&) = delete;

    Complaint* insertBack(std::unique_ptr<Complaint> c);   // O(1)
    Complaint* insertFront(std::unique_ptr<Complaint> c);  // O(1)
    std::unique_ptr<Complaint> remove(long long id);       // O(n) search + O(1) unlink; nullptr if absent
    Complaint* search(long long id) const;                 // O(n), counts visited nodes
    bool update(long long id, const std::function<void(Complaint&)>& fn);
    void traverseForward(const std::function<void(Complaint&)>& fn) const;
    void traverseBackward(const std::function<void(Complaint&)>& fn) const;
    std::vector<Complaint*> toVector(bool newestFirst = false) const;
    void clear();

    std::size_t size() const { return size_; }
    bool empty() const { return size_ == 0; }
    const Node* head() const { return head_; }
    const Node* tail() const { return tail_; }
    std::size_t lastVisited() const { return lastVisited_; }

private:
    Node* findNode(long long id) const;
    Node* head_ = nullptr;
    Node* tail_ = nullptr;
    std::size_t size_ = 0;
    mutable std::size_t lastVisited_ = 0;
};

} // namespace scms
