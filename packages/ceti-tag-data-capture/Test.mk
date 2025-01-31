
TEST_DIR := tests
STUB_DIR := $(TEST_DIR)/stubs
MOCK_DIR := $(TEST_DIR)/mocks
FAKE_DIR := $(TEST_DIR)/fakes
TEST_SRC_DIR := $(TEST_DIR)/src
TEST_BIN_DIR := $(TEST_DIR)/bin

STUB_SRC := $(shell find $(STUB_DIR) -type f -iname '*.c' 2> /dev/null)
MOCK_SRC := $(shell find $(MOCK_DIR) -type f -iname '*.c' 2> /dev/null)
FAKE_SRC := $(shell find $(FAKE_DIR) -type f -iname '*.c' 2> /dev/null)

STUB_OBJ = $(STUB_SRC:.c=.o)
MOCK_OBJ = $(MOCK_SRC:.c=.o)
FAKE_OBJ = $(FAKE_SRC:.c=.o)
	
TEST_SRC := $(shell find $(TEST_SRC_DIR) -type f -iname '*.c' 2> /dev/null)
TEST_BIN = $(patsubst $(TEST_SRC_DIR)/%.c, $(TEST_BIN_DIR)/%, $(TEST_SRC))
TEST_OUT_DIRS :=  $(sort $(dir $(TEST_BIN)))
TEST_OBJ = $(TEST_SRC:.c=.o)

TEST_C_INCLUDE_FLAGS = -I $(UNITY_DIR)/src/ -I src/
TEST_CFLAGS     = -Wall -O2 -Wdate-time -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE -DUNIT_TEST $(TEST_C_INCLUDE_FLAGS)
TEST_LDFLAGS    = -lpthread -lFLAC -lm -lrt -L $(UNITY_DIR) -lunity

TESTABLE_OBJ = $(SRC_DIR)/cetiTagApp/utils/str.o \
	$(SRC_DIR)/cetiTagApp/state_machine.o \
	$(SRC_DIR)/cetiTagApp/aprs.o \
	$(SRC_DIR)/cetiTagApp/utils/logging.o \
	$(SRC_DIR)/cetiTagApp/utils/error.o

# Colorful text printing
NO_COL  := \033[0m
RED     := \033[0;31m
GREEN   := \033[0;32m
BLUE    := \033[0;34m
YELLOW  := \033[0;33m
BLINK   := \033[33;5m

# Libraries
## Unity
UNITY_DIR = $(TEST_DIR)/lib/Unity
UNITY = $(UNITY_DIR)/libunity.a
$(UNITY_DIR)/Makefile:
	cd $(UNITY_DIR) && cmake .

$(UNITY): $(UNITY_DIR)/Makefile
	$(MAKE) -C $(UNITY_DIR)

-include $(DEPFILES)

## test directories
$(TEST_OUT_DIRS):
	@mkdir -p $@

%.test.o: %.test.c
	$(CC) $(TEST_CFLAGS) -o $@ -c $<

%.mock.o: %.mock.c
	$(CC) $(TEST_CFLAGS) -o $@ -c $<

%.fake.o: %.fake.c
	$(CC) $(TEST_CFLAGS) -o $@ -c $<

%.stub.o: %.stub.c
	$(CC) $(TEST_CFLAGS) -o $@ -c $<



$(TEST_BIN_DIR)/%.test: $(SRC_DIR)/%.o $(TESTABLE_OBJ) $(TEST_OBJ) $(MOCK_OBJ) $(STUB_OBJ) $(FAKE_OBJ) $(UNITY) | $(TEST_OUT_DIRS)
	$(CC) $(TEST_CFLAGS) -o $@ $(TEST_DEP) $(UNITY) $(TEST_LDFLAGS)

test: CFLAGS = $(TEST_CFLAGS)
test: $(TEST_BIN)
	@echo 0 > .test.result
	@echo "" > test.results
	-@for test in $(TEST_BIN); do \
		./$$test && echo 0 >> .test.result || echo 1 >> .test.result; \
	done
	@if grep -q "1" .test.result; then \
		printf "\n$(RED)FAIL$(NO_COL): One or more tests failed!\n\n"; \
	else \
		printf "\n$$(cat tests/microrobotics.ansi.txt)\n"; \
		printf "All tests passed\n"; \
		printf "$(GREEN)OK$(NO_COL)\n"; \
	fi 
	@rm -f .test.result

test_clean:
	rm -f $(TEST_OBJ) $(TESTABLE_OBJ)
	rm -rf $(TEST_BIN_DIR)

.PHONY: \
	test \
	test_clean

# test dependencies
TEST_REAL_DEP = 
TEST_TEST_DEP = 
TEST_STUB_DEP = 
TEST_MOCK_DEP = 
TEST_FAKE_DEP = 
TEST_DEP = $(addprefix $(SRC_DIR)/, $(TEST_REAL_DEP)) \
	$(addprefix $(TEST_SRC_DIR)/,  $(patsubst %.o, %.test.o, $(TEST_TEST_DEP))) \
	$(addprefix $(STUB_DIR)/, $(patsubst %.o, %.stub.o, $(TEST_STUB_DEP))) \
	$(addprefix $(MOCK_DIR)/, $(patsubst %.o, %.mock.o, $(TEST_MOCK_DEP))) \
	$(addprefix $(FAKE_DIR)/, $(patsubst %.o, %.fake.o, $(TEST_FAKE_DEP)))

$(TEST_BIN_DIR)/cetiTagApp/aprs.test: TEST_TEST_DEP = cetiTagApp/aprs.o
$(TEST_BIN_DIR)/cetiTagApp/aprs.test: TEST_REAL_DEP = cetiTagApp/aprs.o

$(TEST_BIN_DIR)/cetiTagApp/utils/timing.test: TEST_TEST_DEP = cetiTagApp/utils/timing.o
$(TEST_BIN_DIR)/cetiTagApp/utils/timing.test: TEST_REAL_DEP = cetiTagApp/utils/timing.o cetiTagApp/utils/error.o
$(TEST_BIN_DIR)/cetiTagApp/utils/timing.test: TEST_STUB_DEP = cetiTagApp/device/rtc.o

$(TEST_BIN_DIR)/cetiTagApp/utils/str.test: TEST_TEST_DEP = cetiTagApp/utils/str.o
$(TEST_BIN_DIR)/cetiTagApp/utils/str.test: TEST_REAL_DEP = cetiTagApp/utils/str.o

$(TEST_BIN_DIR)/cetiTagApp/state_machine.test: TEST_TEST_DEP = cetiTagApp/state_machine.o 
$(TEST_BIN_DIR)/cetiTagApp/state_machine.test: TEST_REAL_DEP = cetiTagApp/state_machine.o cetiTagApp/aprs.o cetiTagApp/utils/error.o cetiTagApp/utils/logging.o cetiTagApp/utils/str.o
$(TEST_BIN_DIR)/cetiTagApp/state_machine.test: TEST_STUB_DEP = cetiTagApp/utils/power.o cetiTagApp/burnwire.o cetiTagApp/recovery.o
