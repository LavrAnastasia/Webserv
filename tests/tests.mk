TEST_DIR := tests

TEST_NAMES := \
	config_loader \
	http_parser \
	request_handler \
	router \
	serializer \
	server \
	static_handler

TEST_SRC := $(addprefix $(TEST_DIR)/,$(addsuffix _tests.cpp,$(TEST_NAMES)))
TEST_OBJ := $(addprefix $(BUILD_DIR)/,$(TEST_SRC:.cpp=.o))
TEST_BIN := $(TEST_OBJ:.o=)
TEST_DEPS := $(TEST_OBJ:.o=.d)

LIB_OBJ := $(filter-out $(BUILD_DIR)/app/main.o,$(OBJ))

GTEST_ROOT ?=

ifneq ($(GTEST_ROOT),)
GTEST_CFLAGS ?= -I$(GTEST_ROOT)/include
GTEST_LIBS ?= -L$(GTEST_ROOT)/lib -lgtest_main -lgtest -pthread
else
GTEST_CFLAGS ?= $(shell pkg-config --cflags gtest_main 2>/dev/null)
GTEST_LIBS ?= $(shell pkg-config --libs gtest_main 2>/dev/null || echo -lgtest_main -lgtest -pthread)
endif

$(TEST_OBJ): INCLUDES += $(GTEST_CFLAGS)

$(TEST_BIN): $(BUILD_DIR)/$(TEST_DIR)/%: $(BUILD_DIR)/$(TEST_DIR)/%.o $(LIB_OBJ)
	$(CXX) $(CXXFLAGS) $^ $(GTEST_LIBS) -o $@

-include $(TEST_DEPS)

test: $(TEST_BIN)
	@failed=0; for bin in $(TEST_BIN); do \
		echo "== $$bin"; \
		"$$bin" --gtest_brief=1 || failed=1; \
	done; \
	exit $$failed

tests: test

$(addprefix test-,$(TEST_NAMES)): test-%: $(BUILD_DIR)/$(TEST_DIR)/%_tests
	"$<"

test-unit: test-http_parser
test-config: test-config_loader
test-handler: test-request_handler
test-static: test-static_handler

.PHONY: test tests $(addprefix test-,$(TEST_NAMES)) test-unit test-config test-handler test-static
.SECONDARY: $(TEST_OBJ)
