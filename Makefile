NAME := webserv

CXX := c++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror
INCLUDES := \
	-Isrc/http/include \
	-Isrc/config/include \
	-Isrc/net/include

DEPFLAGS := -MMD -MP

CONFIG_SRC := $(addprefix src/config/src/, \
	ConfigLoader.cpp \
	ConfigLexer.cpp \
	ConfigParser.cpp \
	ConfigSpecification.cpp \
	ConfigValidator.cpp \
	ConfigDecoder.cpp \
	ConfigBuilder.cpp \
	ConfigDecodingError.cpp \
	ConfigValidationError.cpp \
	ConfigError.cpp \
	ConfigReadError.cpp \
	ConfigSyntaxError.cpp \
)

HTTP_SRC := $(addprefix src/http/src/, \
	HttpMethodUtils.cpp \
	RequestLineParser.cpp \
	HttpParser.cpp \
	HttpHeaders.cpp \
	HttpUtils.cpp \
	HeadersParser.cpp \
	Router.cpp \
	ErrorResponseFactory.cpp \
	HttpResponseFactory.cpp \
	HttpStatusUtils.cpp \
	HttpSerializer.cpp \
	HttpHeadersUtils.cpp \
)

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

format: find src \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i

format-check: find src \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format --dry-run --Werror

.PHONY: all clean fclean re
.SECONDARY: $(BUILD_DIR) $(OBJ)
