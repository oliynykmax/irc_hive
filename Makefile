NAME    := ircserv
CXX     := c++
CXXFLAGS:= -Wall -Wextra -Werror -std=c++20 -Iinc
SRC    := \
		main.cpp \
		User.cpp \
		Channel.cpp \
		Server.cpp \
		RecvParser.cpp \
		Client.cpp \
		Command.cpp \
		CommandDispatcher.cpp \
		Handler.cpp
SRCS	:= $(addprefix src/, $(SRC))
OBJS    := $(SRCS:src/%.cpp=.build/%.o)
BOT_NAME := ircbot
BOT_SRC  := bot_main.cpp DeepSeekClient.cpp
BOT_SRCS := $(addprefix src_bot/, $(BOT_SRC))
BOT_OBJS := $(BOT_SRCS:src_bot/%.cpp=.build/bot_%.o)
CURL_CFLAGS ?= $(shell pkg-config --cflags libcurl 2>/dev/null)
CURL_LIBS   ?= $(shell pkg-config --libs libcurl 2>/dev/null)

# Graceful fallback: if libcurl not found, disable LLM features instead of failing the link
ifeq ($(strip $(CURL_LIBS)),)
  BOT_CURL_DISABLED := 1
  CURL_NOTE := "(no libcurl found; LLM disabled)"
  CURL_DEFS := -DDEEPSEEK_NO_CURL
else
  CURL_NOTE := "(curl enabled)"
  CURL_DEFS :=
endif

DEPS    := $(OBJS:.o=.d) $(BOT_OBJS:.o=.d)

all: $(NAME) $(BOT_NAME)

$(NAME): $(OBJS)
	echo "🔗 Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	echo "🎉 Build complete!"

$(BOT_NAME): $(BOT_OBJS)
	echo "🔗 Linking $(BOT_NAME)..."
	$(CXX) $(CXXFLAGS) $(CURL_CFLAGS) $(CURL_DEFS) $(BOT_OBJS) -o $@ $(CURL_LIBS)
	echo "🤖 Bot build complete $(CURL_NOTE)"
	echo "Note: $(CURL_NOTE)" 

.build/%.o: src/%.cpp | .build
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build/bot_%.o: src_bot/%.cpp | .build
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build:
	@mkdir -p .build

clean:
	echo "🧹 Cleaning..."
	@rm -rf .build

fclean: clean
	echo "🗑️ Removing $(NAME) and $(BOT_NAME)"
	@rm -f $(NAME) $(BOT_NAME)

re:
	echo "🔄 Rebuilding..."
	$(MAKE) --no-print-directory fclean
	$(MAKE) --no-print-directory all

-include $(DEPS)
.SILENT:
.PHONY: all clean fclean re
