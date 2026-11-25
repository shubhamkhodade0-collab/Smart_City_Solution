___________________________________________________________________________________
<--------------------------------------CO_1-------------------------------------->
Arrays:
Fixed-size array and 2D grid operations are implemented in:
- `dsa/CityArray.h`, lines 16-53
- `dsa/CityArray.cpp`

Operations:
- `insert()` appends a city record and throws `CapacityException` when full.
- `update()` finds a city ID and replaces its record.
- `remove()` searches and shifts later records left.
- `indexOf()` performs linear search by city ID.
- `at()` provides indexed access.
- `totalComplaints()` calculates the total complaints.
- `StatGrid` implements state x category 2D-array operations with `set()`, `get()`, row and column totals and `maxCell()`.


___________________________________________________________________________________
<--------------------------------------CO_2-------------------------------------->
Linked List:
`dsa/DoublyLinkedList.h` and `dsa/DoublyLinkedList.cpp` store active complaint objects.

Operations:
- `insertBack()` inserts an entry at the tail.
- `insertFront()` inserts an entry at the head.
- `remove()` finds and unlinks a complaint node.
- `search()` searches by complaint ID and tracks visited nodes.
- `update()` applies a callback to an existing complaint.
- `traverseForward()` traverses from head to tail.
- `traverseBackward()` traverses from tail to head.
- `toVector()` prepares a forward or newest-first view.
- `clear()` releases all nodes and complaint objects.

The linked list stores `Complaint` objects through `std::unique_ptr`, preserving ownership and preventing double deletion.


___________________________________________________________________________________
<--------------------------------------CO_3-------------------------------------->
Stack and Queue:
----------------------------------------------------------------------------------
Stack:
Implemented in `dsa/Stack.h` and `dsa/Stack.cpp`.

Normal cases:
- `push()` adds an entry when the stack has room.
- `pop()` removes and returns the top entry.
- `peek()` reads the top entry without removing it.
- `topToBottom()` returns entries newest to oldest.

Boundary cases:
- `pop()` or `peek()` on an empty stack throws `EmptyStructureException`.
- `push()` on a full stack throws `CapacityException`.
- `pushBounded()` discards the oldest entry before inserting the new entry.
- A requested capacity of 0 is changed to 1 by the constructor.
- `dsa/Stack.cpp`, lines 10-37

----------------------------------------------------------------------------------

Queue:
Implemented in `dsa/Queue.h` and `dsa/Queue.cpp`.

Normal cases:
- `enqueue()` adds a complaint to the rear of the queue.
- `dequeue()` removes and returns the front complaint (FIFO).
- `front()` reads the first complaint without removing it.
- `snapshot()` returns queued items from front to rear.
- `contains()` checks whether a complaint ID is already queued.
- `clear()` removes all queued items.

Boundary cases:
- `dequeue()` or `front()` on an empty queue throws `EmptyStructureException`.
- `dsa/Queue.cpp`, lines 18-53


___________________________________________________________________________________
<------------------------------------CO_4 & 5------------------------------------>

1. Searching:
- `dsa/Searching.h`
- `linearSearch()` searches an unsorted complaint or city vector and can collect multiple matching indexes.
- `binarySearch()` searches a sorted city vector by name.
- `dsa/Searching.cpp` also provides case-insensitive name and prefix matching.

2. Sorting:
- Implemented in `dsa/Sorting.h`.
- Bubble sort ranks or compares elements using `O(n^2)` comparisons.
- Quick sort uses median-of-three partitioning and an iterative-style recursion strategy for ranking city records and reports.
- Merge sort provides stable `O(n log n)` sorting for complaint results and city names.
- Insertion sort sorts nearly ordered district or report records with `O(n^2)` worst-case complexity.
- Template functions allow the sorting implementations to work with different types.
- Lines: 47-153

3. Hashing:
- Implemented in `dsa/HashTable.h` and `dsa/HashTable.cpp`.
- `insert()` adds a complaint using a reference key and handles collision through separate chaining.
- `search()` finds the matching key in the bucket chain.
- `update()` replaces the value for an existing key.
- `remove()` deletes an entry from the chain.
- `rehash()` rebuilds the table with more buckets and redistributes entries.
- `nextPrime()` selects a suitable bucket size.
- `chainLengths()` returns per-bucket chain lengths.
- `stats()` reports records, buckets, collisions, load factor and search metrics.
- Automatic rehashing occurs when load factor exceeds 0.75.
- `dsa/HashTable.cpp`, lines 30-132
