NAME := webserv

CXX := c++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror
INCLUDES := \
	-Isrc/http/include \
	-Isrc/config/include \
	-Isrc/net/include

DEPFLAGS := -MMD -MP

CONFIG_SRC := \
	src/config/src/ConfigLoader.cpp \
	src/config/src/ConfigLexer.cpp \
	src/config/src/ConfigParser.cpp \
	src/config/src/ConfigSpecification.cpp \
	src/config/src/ConfigValidator.cpp \
	src/config/src/ConfigDecoder.cpp \
	src/config/src/ConfigBuilder.cpp \
	src/config/src/ConfigDecodingError.cpp \
	src/config/src/ConfigValidationError.cpp \
	src/config/src/ConfigError.cpp \
	src/config/src/ConfigReadError.cpp \
	src/config/src/ConfigSyntaxError.cpp

HTTP_SRC := \
	src/http/src/HttpMethod.cpp \
	src/http/src/RequestLineParser.cpp \
	src/http/src/HttpParser.cpp \
	src/http/src/HttpHeaders.cpp \
	src/http/src/HttpUtils.cpp \
	src/http/src/HeadersParser.cpp

NET_SRC := \
	src/net/src/Socket.cpp \
	src/net/src/ServerSocket.cpp \
	src/net/src/SocketManager.cpp \
	src/net/src/Connection.cpp \
	src/net/src/ConnectionRegistry.cpp \
	src/net/src/Poller.cpp \
	src/net/src/TcpServer.cpp \
	src/net/src/EventLoop.cpp

SRC := \
	app/main.cpp \
	${CONFIG_SRC} \
	${HTTP_SRC} \
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
