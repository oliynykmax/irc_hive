NAME    := ircserv
CXX     := c++
CXXFLAGS:= -Wall -Wextra -Werror -std=c++20 -Iinc
SRCS    := \
		   src/main.cpp
OBJS    := $(SRCS:.cpp=.o)
DEPS    := $(OBJS:.o=.d)

all: $(NAME)

$(NAME): $(OBJS)
	@echo "🔗 Linking $(NAME)..."
	$(CXX) $(CXXFLAGS) $(OBJS) -o $@
	@echo "🎉 Build complete!"

.build/%.o: src/%.cpp | .build
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

.build:
	mkdir -p .build

clean:
	@echo "🧹 Cleaning..."
	rm -rf .build

fclean: clean
	@echo "🗑️ Removing $(NAME)"
	rm -f $(NAME)

re:
	@echo "🔄 Rebuilding..."
	$(MAKE) --no-print-directory fclean
	$(MAKE) --no-print-directory all

-include $(DEPS)

.PHONY: all clean fclean re
