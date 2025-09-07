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
BOT_SRC  := bot_main.cpp
BOT_SRCS := $(addprefix src_bot/, $(BOT_SRC))
BOT_OBJS := $(BOT_SRCS:src_bot/%.cpp=.build/bot_%.o)

DEPS    := $(OBJS:.o=.d) $(BOT_OBJS:.o=.d)

all: $(NAME) $(BOT_NAME)

$(NAME): $(OBJS)
	echo "🔗 Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	echo "🎉 Build complete!"

$(BOT_NAME): $(BOT_OBJS)
	echo "🔗 Linking $(BOT_NAME)..."
	$(CXX) $(CXXFLAGS) $(BOT_OBJS) -o $@
	echo "🤖 Bot build complete!"

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
