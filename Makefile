# =====================================================================
# Smart City Complaint Monitoring & Management System
# Build with MSYS2 UCRT64:   make all | make backend | make frontend | make test | make test-ai | make test-llm | make test-input | make database | make demo | make clean
# =====================================================================
CXX      ?= g++
NASM     ?= nasm
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter
CPPFLAGS += -I. -Ithird_party/stb -pthread
BUILD    := build
OBJ      := $(BUILD)/obj

UNAME := $(shell uname -s 2>/dev/null)
ifneq (,$(or $(findstring _NT,$(UNAME)),$(filter Windows_NT,$(OS))))
  EXE      := .exe
  NASMFMT  := win64
  GL_LIBS  := -lfreeglut -lglu32 -lopengl32 -lgdi32 -luser32 -lwinmm
  NET_LIBS := -pthread -lwinhttp
else
  EXE      :=
  NASMFMT  := elf64
  GL_LIBS  := -lglut -lGLU -lGL
  NET_LIBS := -pthread
endif
PQ_LIBS := -lpq

ASM_SRC     := assembly/severity.asm assembly/strops.asm
ASM_OBJ     := $(patsubst assembly/%.asm,$(OBJ)/asm_%.o,$(ASM_SRC))
CORE_SRC    := $(wildcard backend/*.cpp) $(wildcard dsa/*.cpp)
CORE_SRC    := $(filter-out backend/cli_main.cpp,$(CORE_SRC))
CORE_OBJ    := $(patsubst %.cpp,$(OBJ)/%.o,$(CORE_SRC))
UI_SRC      := $(wildcard frontend/opengl/*.cpp) $(wildcard frontend/opengl/*/*.cpp)
UI_OBJ      := $(patsubst %.cpp,$(OBJ)/%.o,$(UI_SRC))
TEST_SRC    := tests/test_main.cpp
TEST_UI_SRC := frontend/opengl/UI/Input.cpp
AI_TEST_SRC := tests/test_ai.cpp
LLM_TEST_SRC := tests/test_llm.cpp
INPUT_TEST_SRC := tests/test_input.cpp

APP   := $(BUILD)/scms_app$(EXE)
CLI   := $(BUILD)/scms_cli$(EXE)
TESTS := $(BUILD)/scms_tests$(EXE)
AI_TESTS := $(BUILD)/scms_ai_tests$(EXE)
LLM_TESTS := $(BUILD)/scms_llm_tests$(EXE)
INPUT_TESTS := $(BUILD)/scms_input_tests$(EXE)

.PHONY: all backend frontend test test-ai test-llm test-input database demo run clean asm

# the general test suite is only built when tests/test_main.cpp exists in the project folder
all: backend frontend $(if $(wildcard $(TEST_SRC)),$(TESTS))

asm: $(ASM_OBJ)
backend: $(CLI)
frontend: $(APP)

# --- Assembly: NASM -> object file (linked straight into every executable) ---
$(OBJ)/asm_%.o: assembly/%.asm
	@mkdir -p $(dir $@)
	$(NASM) -f $(NASMFMT) $< -o $@

$(OBJ)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(CLI): $(CORE_OBJ) $(ASM_OBJ) $(OBJ)/backend/cli_main.o
	$(CXX) $^ -o $@ $(PQ_LIBS) $(NET_LIBS)

$(APP): $(CORE_OBJ) $(ASM_OBJ) $(UI_OBJ)
	$(CXX) $^ -o $@ $(PQ_LIBS) $(NET_LIBS) $(GL_LIBS)

$(TESTS): $(CORE_OBJ) $(ASM_OBJ) $(OBJ)/tests/test_main.o $(patsubst %.cpp,$(OBJ)/%.o,$(TEST_UI_SRC))
	$(CXX) $^ -o $@ $(PQ_LIBS) $(NET_LIBS)

test: $(TESTS)
	./$(TESTS)

# unit tests of the AI assistant (no database needed)
$(AI_TESTS): $(CORE_OBJ) $(ASM_OBJ) $(patsubst %.cpp,$(OBJ)/%.o,$(AI_TEST_SRC))
	$(CXX) $^ -o $@ $(PQ_LIBS) $(NET_LIBS)

test-ai: $(AI_TESTS)
	./$(AI_TESTS)

# unit tests of the optional Groq connection (JSON, request/response, settings, privacy rules); no network needed
$(LLM_TESTS): $(CORE_OBJ) $(ASM_OBJ) $(patsubst %.cpp,$(OBJ)/%.o,$(LLM_TEST_SRC))
	$(CXX) $^ -o $@ $(PQ_LIBS) $(NET_LIBS)

test-llm: $(LLM_TESTS)
	./$(LLM_TESTS)

# unit tests of the shared text-input model and paste routing
$(INPUT_TESTS): $(patsubst %.cpp,$(OBJ)/%.o,$(INPUT_TEST_SRC) frontend/opengl/UI/Input.cpp)
	$(CXX) $^ -o $@ $(NET_LIBS)

test-input: $(INPUT_TESTS)
	./$(INPUT_TESTS)

database:
	bash database/setup_db.sh

demo: $(CLI)
	./$(CLI) --seed-demo 300

run: $(APP)
	./$(APP)

clean:
	rm -rf $(BUILD)

-include $(shell find $(OBJ) -name '*.d' 2>/dev/null)
