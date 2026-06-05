NAME := webserv

CXX := c++
CXXFLAGS := -std=c++23 -Wall -Wextra -Werror
INCLUDES := \
	-Isrc/http/include \
	-Isrc/config/include \
	-Isrc/net/include

DEPFLAGS := -MMD -MP

CONFIG_SRC := \
	src/config/src/ConfigLoader.cpp \
	src/config/src/ConfigLexer.cpp \
	src/config/src/ConfigParser.cpp \
	src/config/src/ConfigError.cpp \
	src/config/src/ConfigReadError.cpp \
	src/config/src/ConfigSyntaxError.cpp

NET_SRC := \
	src/net/src/ServerSocket.cpp \
	src/net/src/SocketManager.cpp

SRC := \
	app/main.cpp \
	${CONFIG_SRC} \
	${NET_SRC}

BUILD_DIR := build

OBJ := $(addprefix $(BUILD_DIR)/, $(SRC:.cpp=.o))
DEPS := $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(INCLUDES) $(DEPFLAGS) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
.SECONDARY: $(BUILD_DIR) $(OBJ)
