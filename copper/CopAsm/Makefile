# Declaration of variables
CC = clang++
CC_FLAGS = -g -std=c++20 -O2 -Wall -Wextra -Weverything -Wno-poison-system-directories -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-c++98-c++11-compat-binary-literal -Wno-padded -Wno-exit-time-destructors -Wno-global-constructors -Wno-gnu-zero-variadic-macro-arguments -Wno-covered-switch-default -Wno-unreachable-code-break -Wno-switch-enum
#CC_FLAGS += -DNDEBUG

# File names
EXEC = copasm
BINDIR = bin
OBJDIR = obj

SOURCES = $(wildcard *.cpp)
OBJECTS = $(addprefix $(OBJDIR)/,$(SOURCES:.cpp=.o))

all: $(BINDIR)/$(EXEC) test

test: $(BINDIR)/$(EXEC)
	$(BINDIR)/$(EXEC) -l Tests/cop_diagonal.casm -o $(OBJDIR)/cop_diagonal.h
	$(BINDIR)/$(EXEC) -l Tests/copy_table.casm -o $(OBJDIR)/copy_table.h

# Main target
$(BINDIR)/$(EXEC): $(OBJECTS) $(MAKEFILE_LIST)
	@mkdir -p $(BINDIR)
	$(CC) $(CC_FLAGS) $(OBJECTS) -o $(BINDIR)/$(EXEC)

# To obtain object files
$(OBJDIR)/%.o: %.cpp $(MAKEFILE_LIST)
	@mkdir -p $(OBJDIR)
	$(CC) -c $(CC_FLAGS) -MMD $< -o $@

# To remove generated files
clean:
	rm -f $(BINDIR)/* $(OBJDIR)/*

-include $(OBJDIR)/*.d
