NAME := webserv

CXX := c++
CXXFLAGS := -std=c++20 -Wall -Wextra -Werror
INCLUDES := \
	-Isrc/http/include \
	-Isrc/config/include \
	-Isrc/net/include \
	-Isrc/fs/include \
	-Isrc/cgi/include \
	-Isrc/log/include

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

FS_SRC := $(addprefix src/fs/src/, \
	Path.cpp \
	FileDescriptor.cpp \
)

LOG_SRC := $(addprefix src/log/src/, \
	Log.cpp \
)

CGI_SRC := $(addprefix src/cgi/src/, \
	CgiProcess.cpp \
	CgiRegistry.cpp \
)

HTTP_SRC := $(addprefix src/http/src/, \
	HttpMethod.cpp \
	RequestLineParser.cpp \
	HttpParser.cpp \
	HttpHeaders.cpp \
	HttpUtils.cpp \
	HeadersParser.cpp \
	Router.cpp \
	ErrorResponseFactory.cpp \
	HttpResponseFactory.cpp \
	HttpStatus.cpp \
	HttpSerializer.cpp \
	StaticHandler.cpp \
	RedirectHandler.cpp \
	UploadHandler.cpp \
	UrlCodec.cpp \
	MimeTypes.cpp \
	HttpHtmlUtils.cpp \
	RequestDispatcher.cpp \
	HttpRequest.cpp \
	CgiHandler.cpp \
	CgiResponseParser.cpp \
)

NET_SRC := $(addprefix src/net/src/, \
	Socket.cpp \
	ServerSocket.cpp \
	SocketManager.cpp \
	Connection.cpp \
	ConnectionRegistry.cpp \
	Poller.cpp \
	TcpServer.cpp \
	EventLoop.cpp \
)

SRC := \
	app/main.cpp \
	${CONFIG_SRC} \
	${HTTP_SRC} \
	${NET_SRC} \
	${CGI_SRC} \
	${FS_SRC} \
	${LOG_SRC}

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

format:
	find src \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format -i

format-check:
	find src \( -name "*.cpp" -o -name "*.hpp" \) -print0 | xargs -0 clang-format --dry-run --Werror

.PHONY: all clean fclean re format format-check
.SECONDARY: $(BUILD_DIR) $(OBJ)

include tests/tests.mk
