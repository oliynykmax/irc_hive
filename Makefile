NAME    := ircserv
BOT_NAME := ircbot
CXX     := c++
CXXFLAGS:= -Wall -Wextra -Werror -std=c++20 -Iinc
SRC    := \
		main.cpp \
		Server.cpp \
		Channel.cpp \
		User.cpp \
		RecvParser.cpp \
		Client.cpp \
		Command.cpp \
		CommandDispatcher.cpp \
		Handler.cpp
SRCS	:= $(addprefix src/, $(SRC))
OBJS    := $(SRCS:src/%.cpp=.build/%.o)
DEPS    := $(OBJS:.o=.d)

BOT_SRC := bot/MainBot.cpp
BOT_SRCS := $(BOT_SRC) src/RecvParser.cpp
BOT_OBJS := $(BOT_SRC:bot/%.cpp=.build/bot_%.o) .build/RecvParser.o

all: $(NAME)

bot: $(BOT_NAME)

$(NAME): $(OBJS)
	echo "🔗 Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	echo "🎉 Build complete!"

$(BOT_NAME): $(BOT_OBJS)
	echo "🔗 Linking $(BOT_NAME)..."
	$(CXX) $(CXXFLAGS) $(BOT_OBJS) -o $@
	echo "🎉 Bot build complete!"

.build/%.o: src/%.cpp | .build
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build/bot_%.o: bot/%.cpp | .build
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build:
	@mkdir -p .build

clean:
	echo "🧹 Cleaning..."
	@rm -rf .build

fclean: clean
	echo "🗑️ Removing $(NAME)"
	@rm -f $(NAME) $(BOT_NAME)

re:
	echo "🔄 Rebuilding..."
	$(MAKE) --no-print-directory fclean
	$(MAKE) --no-print-directory all

-include $(DEPS)
.SILENT:
.PHONY: all clean fclean re bot
