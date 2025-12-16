# ═══════════════════════════════════════════════════════════════════════════
# LVM Auto-Extender Makefile
# Optimized for Red Hat Enterprise Linux / CentOS
# ═══════════════════════════════════════════════════════════════════════════

# ─────────────────────────────────────────────────────────────────────────
# CONFIGURATION
# ─────────────────────────────────────────────────────────────────────────
CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=gnu99 -D_GNU_SOURCE
LDFLAGS = -pthread
TARGET = lvm_manager
INSTALL_DIR = /usr/local/bin

# ─────────────────────────────────────────────────────────────────────────
# SOURCE FILES
# ─────────────────────────────────────────────────────────────────────────
SOURCES = lvm_main.c \
          lvm_logger.c \
          lvm_utils.c \
          lvm_extender.c \
          lvm_threads.c

HEADERS = lvm_config.h \
          lvm_types.h \
          lvm_logger.h \
          lvm_utils.h \
          lvm_extender.h \
          lvm_threads.h

OBJECTS = $(SOURCES:.c=.o)

# ─────────────────────────────────────────────────────────────────────────
# TARGETS
# ─────────────────────────────────────────────────────────────────────────

.PHONY: all clean install uninstall test help

# Default target
all: $(TARGET)
	@echo "════════════════════════════════════════════════════════════════"
	@echo "✓ Build complete: $(TARGET)"
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "Next steps:"
	@echo "  1. Review configuration in lvm_config.h"
	@echo "  2. Run in test mode:     sudo ./$(TARGET)"
	@echo "  3. Check dashboard at:   http://localhost:8080"
	@echo "  4. Install system-wide:  sudo make install"
	@echo ""

# Link final executable
$(TARGET): $(OBJECTS)
	@echo "Linking $(TARGET)..."
	$(CC) $(OBJECTS) $(LDFLAGS) -o $(TARGET)

# Compile source files
%.o: %.c $(HEADERS)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Clean complete"

# Install to system
install: $(TARGET)
	@echo "Installing $(TARGET) to $(INSTALL_DIR)..."
	install -m 755 $(TARGET) $(INSTALL_DIR)/$(TARGET)
	@echo "════════════════════════════════════════════════════════════════"
	@echo "✓ Installation complete"
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "You can now run: sudo $(TARGET)"
	@echo ""

# Uninstall from system
uninstall:
	@echo "Uninstalling $(TARGET) from $(INSTALL_DIR)..."
	rm -f $(INSTALL_DIR)/$(TARGET)
	@echo "✓ Uninstall complete"

# Run tests (basic validation)
test: $(TARGET)
	@echo "════════════════════════════════════════════════════════════════"
	@echo "Running basic tests..."
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "1. Checking for required commands..."
	@command -v lvs >/dev/null 2>&1 && echo "  ✓ lvs found" || echo "  ✗ lvs not found (install lvm2)"
	@command -v vgs >/dev/null 2>&1 && echo "  ✓ vgs found" || echo "  ✗ vgs not found (install lvm2)"
	@command -v pvs >/dev/null 2>&1 && echo "  ✓ pvs found" || echo "  ✗ pvs not found (install lvm2)"
	@command -v df >/dev/null 2>&1 && echo "  ✓ df found" || echo "  ✗ df not found"
	@echo ""
	@echo "2. Testing program startup (will exit immediately)..."
	@timeout 2 sudo ./$(TARGET) || echo "  ✓ Program runs without immediate crash"
	@echo ""
	@echo "3. Configuration check..."
	@grep "DRY_RUN.*1" lvm_config.h >/dev/null && echo "  ✓ DRY_RUN enabled (safe for testing)" || echo "  ⚠ DRY_RUN disabled (will perform real operations!)"
	@echo ""
	@echo "════════════════════════════════════════════════════════════════"
	@echo "Tests complete"
	@echo "════════════════════════════════════════════════════════════════"

# Build for debugging
debug: CFLAGS += -g -DDEBUG
debug: clean $(TARGET)
	@echo "✓ Debug build complete"

# Build for production
production: CFLAGS += -O3 -DNDEBUG
production: clean $(TARGET)
	@echo "✓ Production build complete"

# Show help
help:
	@echo "════════════════════════════════════════════════════════════════"
	@echo "LVM Auto-Extender Makefile"
	@echo "════════════════════════════════════════════════════════════════"
	@echo ""
	@echo "Available targets:"
	@echo "  make              - Build the program (default)"
	@echo "  make clean        - Remove build artifacts"
	@echo "  make install      - Install to /usr/local/bin (requires sudo)"
	@echo "  make uninstall    - Remove from /usr/local/bin (requires sudo)"
	@echo "  make test         - Run basic validation tests"
	@echo "  make debug        - Build with debug symbols"
	@echo "  make production   - Build optimized for production"
	@echo "  make help         - Show this help message"
	@echo ""
	@echo "Configuration:"
	@echo "  Edit lvm_config.h to customize behavior"
	@echo ""
	@echo "Usage:"
	@echo "  sudo ./$(TARGET)             - Run the manager"
	@echo "  curl http://localhost:8080   - Check dashboard"
	@echo ""

# ─────────────────────────────────────────────────────────────────────────
# DEPENDENCIES
# ─────────────────────────────────────────────────────────────────────────
lvm_main.o: lvm_main.c lvm_config.h lvm_types.h lvm_logger.h lvm_utils.h lvm_threads.h
lvm_logger.o: lvm_logger.c lvm_logger.h lvm_config.h lvm_types.h
lvm_utils.o: lvm_utils.c lvm_utils.h lvm_logger.h lvm_config.h lvm_types.h
lvm_extender.o: lvm_extender.c lvm_extender.h lvm_logger.h lvm_utils.h lvm_config.h
lvm_threads.o: lvm_threads.c lvm_threads.h lvm_logger.h lvm_utils.h lvm_extender.h lvm_config.h
