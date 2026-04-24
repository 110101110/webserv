NAME        = webserv

CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -I includes
DEP_FLAGS   = -MMD -MP

SRCS_DIR    = src
OBJS_DIR    = objs
INC_DIR     = includes

SRCS        = $(SRCS_DIR)/main.cpp $(SRCS_DIR)/ServerManager.cpp

OBJS        = $(SRCS:$(SRCS_DIR)/%.cpp=$(OBJS_DIR)/%.o)
DEPS        = $(OBJS:.o=.d)

GREEN       = \033[0;32m
RESET       = \033[0m

all: $(OBJS_DIR) $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "$(GREEN)Compilation de $(NAME) terminée !$(RESET)"

$(OBJS_DIR)/%.o: $(SRCS_DIR)/%.cpp
	$(CXX) $(CXXFLAGS) $(DEP_FLAGS) -c $< -o $@

$(OBJS_DIR):
	@mkdir -p $(OBJS_DIR)

clean:
	rm -rf $(OBJS_DIR)
	@echo "Objets et dépendances supprimés."

fclean: clean
	rm -f $(NAME)
	@echo "Exécutable supprimé."

re: fclean all

-include $(DEPS)

.PHONY: all clean fclean re
