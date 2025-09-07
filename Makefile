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
DEPS    := $(OBJS:.o=.d)
BOT_SRC := bot/main_bot.cpp
BOT_BIN := mini_bot

all: $(NAME) $(BOT_BIN)

$(NAME): $(OBJS)
	echo "🔗 Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	echo "🎉 Build complete!"
	echo "🤖 Build bot ($(BOT_BIN)) with: make $(BOT_BIN)"

$(BOT_BIN): $(BOT_SRC)
	$(CXX) $(CXXFLAGS) $(BOT_SRC) -o $(BOT_BIN)

.build/%.o: src/%.cpp | .build
	@$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build:
	@mkdir -p .build

clean:
	echo "🧹 Cleaning..."
	@rm -rf .build

fclean: clean
	echo "🗑️ Removing $(NAME) and $(BOT_BIN)"
	@rm -f $(NAME) $(BOT_BIN)

re:
	echo "🔄 Rebuilding..."
	$(MAKE) --no-print-directory fclean
	$(MAKE) --no-print-directory all

-include $(DEPS)
.SILENT:
.PHONY: all clean fclean re
