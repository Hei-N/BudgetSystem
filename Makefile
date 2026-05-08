CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Iinclude -Iimgui -Iimgui/backends

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3)
GLFW_LIBS   := $(shell pkg-config --libs   glfw3)
GUI_LDFLAGS  = $(GLFW_LIBS) -framework OpenGL

# ── Source groups ──────────────────────────────────────────────────────────────
COMMON_OBJ = build/BudgetManager.o build/Bill.o build/Category.o build/Date.o \
             build/Expense.o build/FinanceAssistant.o build/Income.o           \
             build/ReminderSystem.o build/Transaction.o

IMGUI_OBJ  = build/imgui/imgui.o            \
             build/imgui/imgui_draw.o        \
             build/imgui/imgui_tables.o      \
             build/imgui/imgui_widgets.o     \
             build/imgui/backends/imgui_impl_glfw.o    \
             build/imgui/backends/imgui_impl_opengl3.o

GUI_OBJ = $(COMMON_OBJ) $(IMGUI_OBJ) build/gui_main.o
CLI_OBJ = $(COMMON_OBJ) build/main.o

# ── Targets ────────────────────────────────────────────────────────────────────
.PHONY: all gui cli clean run run-cli

all: gui        # default: GUI only (main.cpp requires Database.h not yet implemented)

gui: budget_gui
cli: budget

budget_gui: $(GUI_OBJ)
	$(CXX) $(CXXFLAGS) $(GLFW_CFLAGS) -o $@ $^ $(GUI_LDFLAGS)

budget: $(CLI_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

# ── Compile rules ──────────────────────────────────────────────────────────────
build/%.o: src/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) $(GLFW_CFLAGS) -c $< -o $@

# imgui rules: always include GLFW flags (backends need GLFW/glfw3.h)
build/imgui/%.o: imgui/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(GLFW_CFLAGS) -c $< -o $@

# ── Utility ────────────────────────────────────────────────────────────────────
clean:
	rm -rf build budget budget_gui

run: gui
	./budget_gui

run-cli: cli
	./budget
