# Computer Organization and Architecture (COA)

This mapping uses the COA Lab outcomes in the supplied *Exam Reform Guidelines for Students* (page 7). The course-level COA outcomes on page 15 are mapped separately below because they include additional 80386, control-signal and pipelining expectations.

## COA Lab outcomes

### CO1 — Processor-Level Modeling of the Application

Identify application data and computations, map them to processor components (registers, ALU and control unit), and implement selected arithmetic operations using a CPU Simulator.

**Project evidence:**

- `../assembly/severity.asm`
  - `weight_table` stores the category base weights as 64-bit values.
  - `calculateSeverityAsm` maps category, impact and urgency to Win64 argument registers (`RCX`, `RDX`, `R8`), loads the base weight into `R10`, computes the product in `R11`, and returns the score in `RAX`.
  - `CMP`, conditional jumps, `ADD`, `SHL` and `SHR` show the arithmetic and control flow.
- `../frontend/opengl/App/ScreensAdmin.cpp`
  - `App::screenAssembly()` displays a register trace for the real NASM severity calculation.

**Coverage:** The project demonstrates application-to-register mapping and runs the arithmetic in Assembly. Its register trace is an explanation of the real Assembly routine, not a CPU Simulator. Complete the simulator portion with the separate CPU Simulator demonstration; identify the software and include the simulator inputs/results in the presentation or submission.

### CO2 — 64-bit Assembly-Based Application Processing

Identify numerical computations from the application and implement at least two using 64-bit Assembly, demonstrating 64-bit data, registers, arithmetic, comparison and conditional branching.

**Implemented in:**

- `../assembly/severity.asm`
  - `asm_multiply_shift_add` implements multiplication using the add-and-shift algorithm.
  - `asm_multiply_successive` implements multiplication using successive addition.
  - `calculateSeverityAsm` computes a complaint severity score using category weight, impact and urgency; it clamps inputs and caps the score.
  - `classifySeverityAsm` maps the score to a severity level using comparisons and conditional branches.
- `../backend/AsmEngine.h` declares the Assembly entry points using `extern "C"` and the Win64 calling convention.
- `../backend/AsmEngine.cpp` calls the Assembly routines through the `AsmEngine` facade.
- `../frontend/opengl/App/ScreensAdmin.cpp`, `App::screenAssembly()`, provides an interactive severity calculator and compares the two multiplication algorithms.

### CO3 — Algorithmic and String Processing

Implement a computationally intensive application operation with an arithmetic algorithm and implement relevant string/data-processing operations in 64-bit Assembly.

**Implemented in:**

- `../assembly/severity.asm`
  - `asm_multiply_shift_add` and `asm_multiply_successive` provide two multiplication algorithms.
- `../assembly/strops.asm`
  - `asm_strlen` measures a null-terminated string.
  - `asm_to_lower` performs in-place ASCII case conversion.
  - `asm_contains_ci` searches strings without ASCII case sensitivity.
  - `asm_hash_djb2` computes a case-insensitive DJB2 hash.
- `../backend/AsmEngine.cpp` exposes the string and hash routines to C++.
- `../backend/Complaint.cpp`, `Complaint::matches()`, uses Assembly string search for complaint fields.
- `../dsa/HashTable.cpp`, `HashTable::bucketOf()`, uses the Assembly hash for complaint-index bucket selection.

### CO4 — COA Integration and Demonstration

Integrate the COA components into the application and demonstrate application operations at processor level, including registers, arithmetic, control flow and string/memory processing, using a CPU Simulator and 64-bit Assembly.

**Implemented in the application:**

- `../backend/Complaint.cpp`, `Complaint::computeSeverity()`, calls Assembly to compute and classify complaint severity.
- `../backend/Complaint.cpp`, `Complaint::matches()`, calls Assembly string matching.
- `../dsa/HashTable.cpp`, `HashTable::bucketOf()`, calls Assembly hashing.
- `../backend/ComplaintService.cpp` computes severity for new complaints and uses complaint matching during search.
- `../frontend/opengl/App/ScreensAdmin.cpp`, `App::screenAssembly()`, shows Assembly call counts, a severity register trace, and multiplication algorithm steps.
- `../backend/AsmEngine.cpp`, `AsmEngine::selfTest()`, checks representative Assembly results; `../frontend/opengl/App/App.cpp` runs that self-test during initialization.

**Coverage:** The Assembly is integrated into the running application and the UI demonstrates its register-level interpretation. The separate CPU Simulator demonstration remains necessary for the simulator part of this outcome.

## COA course-level outcomes

The supplied guidelines also list the following course-level outcomes on page 15. These are broader than the COA Lab outcomes above.

### CO1 — Explain the architecture and instruction set of the 80386 microprocessor

- `../assembly/severity.asm` and `../assembly/strops.asm` use x86-family registers, instructions, memory operands and conditional control flow.
- `../backend/AsmEngine.h` documents the Win64 calling convention used to pass arguments and return values.

**Coverage:** The code demonstrates processor instructions and the x86-64/Win64 ABI, but it is not a full explanation of the 80386 architecture. Address this distinction in the simulator explanation if this course-level CO is being assessed.

### CO2 — Develop Assembly programs using the 80386 instruction set

- The project develops and integrates 64-bit NASM programs in `../assembly/severity.asm` and `../assembly/strops.asm`.

**Coverage:** The project targets x86-64 (`bits 64`) and the Win64 ABI, not a 32-bit 80386 execution environment. The routines demonstrate Assembly programming, but should not be described as directly running on a 80386 simulator.

### CO3 — Illustrate processor-level arithmetic using computer arithmetic algorithms

- `../assembly/severity.asm` implements and compares add-and-shift and successive-addition multiplication.
- `../frontend/opengl/App/ScreensAdmin.cpp`, `App::screenAssembly()`, displays the multiplication results and an add-and-shift step trace.

### CO4 — Demonstrate control-signal generation, organization techniques and instruction pipelining

- The Assembly routines use comparisons and conditional jumps, which demonstrate instruction-level control flow.
- The project does not implement a control-signal generator or an instruction-pipeline model.

**Coverage:** Use the separate CPU Simulator demonstration to address control signals and pipelining if these course-level requirements apply.

