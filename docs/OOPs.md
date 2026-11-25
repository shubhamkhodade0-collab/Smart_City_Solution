<-------------------------------------CO_1--------------------------------------->

Use Of:
__________________________________________________________________________________
1. Class and Object
- Classes are used throughout the backend and DSA modules.
- `Complaint` is the base complaint class in `backend/Complaint.h`.
- `TrafficComplaint`, `GarbageComplaint`, `ElectricityComplaint`, `WaterComplaint`, `RoadComplaint` and `DrainageComplaint` are concrete complaint objects derived from `Complaint`.
- `backend/Complaint.cpp`, lines 72-81
- The application also creates service, database, department, array, linked-list, stack, queue and hash-table objects.

__________________________________________________________________________________
2. Encapsulation
- `Complaint` keeps internal state private in `backend/Complaint.h`.
- Outside code uses public accessors and validated methods such as `status()`, `departmentId()`, `assignDepartment()` and `transitionTo()`.
- Lines: 57-109
- Private fields such as complaint ID, reference, location, description, status and severity cannot be modified directly.

__________________________________________________________________________________
3. Inheritance
- `backend/Complaint.h` defines one base class: `Complaint`.
- Six concrete classes use public inheritance from `Complaint`, for example `TrafficComplaint : public Complaint`.
- Lines: 113-167
- This is single inheritance, with the derived classes sharing common complaint state and behaviour.

__________________________________________________________________________________
4. Polymorphism
- `Complaint` declares virtual pure functions:
  - `typeName()`
  - `asmCategoryCode()`
  - `slaHours()`
  - `handlingAdvice()`
- Derived category classes override these functions.
- `Complaint::create()` is a factory that returns a `std::unique_ptr<Complaint>` containing the selected concrete category.
- Calls through a base `Complaint*` or `Complaint&` resolve the correct derived implementation at runtime.
- `backend/Complaint.h`, lines 45-48 and 113-167
- `backend/Complaint.cpp`, lines 72-81

__________________________________________________________________________________
5. Abstraction
- `Complaint` is an abstract class because `typeName()`, `asmCategoryCode()` and `slaHours()` are pure virtual functions.
- The public complaint interface hides the internal category-specific implementation.
- `isOverdue()`, `computeSeverity()` and `matches()` provide higher-level operations without exposing their background calculations to UI code.
- `backend/Complaint.h`, lines 35-48


<-------------------------------------CO_2--------------------------------------->

Reducing Time and Space Complexity:
- `dsa/HashTable.cpp` implements hashing for complaint reference lookup.
- `bucketOf()` uses the Assembly DJB2 hash and maps the result to a bucket.
- `search()` checks only the matching chain instead of scanning every complaint.
- Average lookup is O(1); worst-case lookup is O(n) for a fully collided chain.
- `rehash()` grows the table and redistributes entries when load factor exceeds 0.75.
- `dsa/HashTable.cpp`, lines 30-132
- Doubly linked list insertion at front or back is O(1), while search is O(n).
- Queue enqueue and dequeue operations are O(1).


<-------------------------------------CO_3--------------------------------------->
Exception Handling:
- Custom exceptions are defined in `backend/Exceptions.h`.
- Validation failures throw `ValidationException`.
- Invalid complaint status transitions throw `InvalidTransitionException`.
- Database/configuration failures use `AppException` and derived exceptions.
- Empty stack and queue operations throw `EmptyStructureException`.
- Full stack operations throw `CapacityException`.

Try/catch/throw examples:
- `backend/Validator.cpp`
  - Validates names, phone numbers, usernames, passwords, locations and ratings.
  - Throws `ValidationException` for invalid input.
  - Lines: 13-69

- `backend/Complaint.cpp`
  - `transitionTo()` validates status transitions and throws `InvalidTransitionException`.
  - `assignDepartment()` validates department ID and throws `ValidationException`.
  - Lines: 39-55

- `backend/ComplaintService.cpp`
  - Uses try/catch handling around complaint creation and service operations.
- `backend/AiChat.cpp`, `backend/AiInsight.cpp` and `backend/AiLlm.cpp`
  - Catch or propagate AI and database exceptions so the UI does not receive a raw failure.
- `backend/cli_main.cpp`, lines 106-112 and 276-283
  - Handles demo and bulk-operation exceptions.


<-------------------------------------CO_4--------------------------------------->

Combine all parts into one working project and add a feature that needs decision-making.
Test it with 2-3 different inputs by applying different logic conditions.

Decision making:
- `backend/AiAgent.cpp`
- `detectIntent()` normalises a message, checks emergency words and scores keyword rules.
- `extractReference()` extracts a complaint reference and handles numeric pattern validation.
- Lines: 440-473
- `AiAgent.cpp` also maps severity scores to labels and status values using if/else and switch logic.

- `backend/AiChat.cpp`
- Uses if/else and switch statements to select responses according to viewer role, complaint focus, intent and system state.
- The AI service can provide built-in local answers or fall back to Groq according to configuration.

- `backend/AiInsight.cpp`
- Uses status and SLA branches to calculate priority, triage urgency and recommendations.

- `frontend/opengl/App/App.cpp`
- Coordinates login, role routing, search, navigation and screen selection.

These features take different user inputs and select different output paths.


Template:
- `dsa/Sorting.h`
- `bubbleSort()`, `quickSort()`, `mergeSort()` and `insertionSort()` use function templates.
- Less-than predicates allow one implementation to sort city, complaint or report data.
- Lines: 47-153

- `dsa/Searching.h`
- `linearSearch()` and `binarySearch()` use templates and callable predicates/comparators.

Standard Template Library (STL):
- `std::vector` is used for sorting buffers and ordered snapshots.
- `std::unique_ptr` provides complaint and node ownership.
- `std::move` transfers resource ownership.
- `std::array`, `std::map`, `std::string` and utility containers are used in backend and DSA modules.

Move in Utility:
- `dsa/DoublyLinkedList.h`
- `Node` uses `std::move(c)` to transfer ownership of a complaint.
- `dsa/DoublyLinkedList.cpp`, lines 18-34
- `dsa/Sorting.h` uses `std::move()` in the swap and insertion-sort implementations.

Type Casting:
- `dsa/Searching.h`, lines 26-58
- Uses `static_cast<long long>(i)` to store search indexes.
- `backend/Complaint.cpp`, line 35
- Uses `static_cast<SeverityLevel>()` for the severity enum result from Assembly.
- `frontend/opengl/Map/IndiaMap.cpp`
- Uses `static_cast<int>()` and coordinate casts where world coordinates are converted to array or screen values.
