NAME := webserv

CXX := c++
CXXFLAGS = -std=c++23 -Wall -Wextra -Werror -MMD -MP

SRC := \
	app/main.cpp

BUILD_DIR := build

OBJ := $(addprefix $(BUILD_DIR)/, $(SRC:.cpp=.o))
DEPS := $(OBJ:.o=.d)

all: $(NAME)

$(NAME): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $(NAME)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

-include $(DEPS)

clean:
	rm -rf $(BUILD_DIR)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
.SECONDARY: $(BUILD_DIR) $(OBJ)
