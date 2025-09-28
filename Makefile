# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -pthread
DEBUGFLAGS = -g -DDEBUG -O0
RELEASEFLAGS = -O2 -DNDEBUG
ANALYZEFLAGS = -fanalyzer -static

# Directories
SRCDIR = .
BINDIR = bin
LOGDIR = logs
TESTDIR = tests
SCRIPTSDIR = scripts

# Source files (currently in root, will move to src/ later)
SERVER_SOURCES = S1.c S2.c S3.c S4.c
CLIENT_SOURCE = s25client.c
ALL_SOURCES = $(SERVER_SOURCES) $(CLIENT_SOURCE)

# Binary files
SERVER_BINS = $(BINDIR)/S1 $(BINDIR)/S2 $(BINDIR)/S3 $(BINDIR)/S4
CLIENT_BIN = $(BINDIR)/s25client
ALL_BINS = $(SERVER_BINS) $(CLIENT_BIN)

# Storage directories
STORAGE_DIRS = ~/S1 ~/S2 ~/S3 ~/S4

# Streamlit configuration (updated for existing environment)
STREAMLIT_ENV = streamlit_env
STREAMLIT_APP = streamlit_app.py

# Default target
.PHONY: all
all: setup-dirs $(ALL_BINS)

# Create necessary directories
.PHONY: setup-dirs
setup-dirs:
	@echo "Setting up project directories..."
	@mkdir -p $(BINDIR) $(LOGDIR)
	@mkdir -p $(STORAGE_DIRS) 2>/dev/null || true
	@echo "Directories created successfully."

# Build all servers
.PHONY: servers
servers: setup-dirs $(SERVER_BINS)

# Build client
.PHONY: client  
client: setup-dirs $(CLIENT_BIN)

# Individual server targets
$(BINDIR)/S1: S1.c
	@echo "Building S1 (Main Server)..."
	$(CC) $(CFLAGS) -o $@ $<

$(BINDIR)/S2: S2.c
	@echo "Building S2 (PDF Server)..."
	$(CC) $(CFLAGS) -o $@ $<

$(BINDIR)/S3: S3.c
	@echo "Building S3 (TXT Server)..."
	$(CC) $(CFLAGS) -o $@ $<

$(BINDIR)/S4: S4.c
	@echo "Building S4 (ZIP Server)..."
	$(CC) $(CFLAGS) -o $@ $<

# Client target
$(BINDIR)/s25client: s25client.c
	@echo "Building Client..."
	$(CC) $(CFLAGS) -o $@ $<

# Debug build
.PHONY: debug
debug: CFLAGS += $(DEBUGFLAGS)
debug: clean all
	@echo "Debug build completed with symbols and debugging enabled."

# Release build
.PHONY: release
release: CFLAGS += $(RELEASEFLAGS)
release: clean all
	@echo "Release build completed with optimizations enabled."

# Static analysis build
.PHONY: analyze
analyze: CFLAGS += $(ANALYZEFLAGS)
analyze: clean all
	@echo "Static analysis build completed."

# Testing targets
.PHONY: test
test: all
	@echo "Running test suite..."
	@if [ -d "$(TESTDIR)" ] && [ -f "$(TESTDIR)/test_runner.sh" ]; then \
		cd $(TESTDIR) && ./test_runner.sh all; \
	else \
		echo "Test framework not yet installed. Run 'make install-tests' first."; \
	fi

# Server management
.PHONY: start-all
start-all: all
	@echo "Starting all servers in background..."
	@if [ -f "$(SCRIPTSDLDIR)/deploy.sh" ]; then \
		$(SCRIPTSDLDIR)/deploy.sh production; \
	else \
		echo "Starting servers manually..."; \
		nohup ./$(BINDIR)/S2 5002 > $(LOGDIR)/s2.log 2>&1 & echo $$! > $(LOGDIR)/s2.pid; \
		nohup ./$(BINDIR)/S3 5003 > $(LOGDIR)/s3.log 2>&1 & echo $$! > $(LOGDIR)/s3.pid; \
		nohup ./$(BINDIR)/S4 5004 > $(LOGDIR)/s4.log 2>&1 & echo $$! > $(LOGDIR)/s4.pid; \
		sleep 2; \
		nohup ./$(BINDIR)/S1 5001 127.0.0.1 5002 127.0.0.1 5003 127.0.0.1 5004 > $(LOGDIR)/s1.log 2>&1 & echo $$! > $(LOGDIR)/s1.pid; \
		echo "Servers started. Check 'make status' for details."; \
	fi

.PHONY: stop-all
stop-all:
	@echo "Stopping all servers..."
	@for pidfile in $(LOGDIR)/*.pid; do \
		if [ -f "$$pidfile" ]; then \
			pid=$$(cat "$$pidfile"); \
			if kill -0 "$$pid" 2>/dev/null; then \
				echo "Stopping process $$pid..."; \
				kill "$$pid"; \
			fi; \
			rm "$$pidfile"; \
		fi; \
	done
	@pkill -f "S[1-4]" 2>/dev/null || true
	@echo "All servers stopped."

# Streamlit deployment (updated for existing environment)
.PHONY: streamlit-deploy
streamlit-deploy: all
	@echo "Starting Streamlit deployment with existing environment..."
	@if [ ! -d "$(STREAMLIT_ENV)" ]; then \
		echo "❌ Streamlit environment not found at ./$(STREAMLIT_ENV)"; \
		echo "Please create it first with:"; \
		echo "  python3 -m venv $(STREAMLIT_ENV)"; \
		echo "  source $(STREAMLIT_ENV)/bin/activate"; \
		echo "  pip install streamlit pandas plotly psutil"; \
		exit 1; \
	fi
	@echo "✅ Found Streamlit environment"
	@$(MAKE) start-all
	@sleep 3
	@echo "🚀 Starting Streamlit web interface..."
	@echo "Run the following command to start Streamlit:"
	@echo "  source $(STREAMLIT_ENV)/bin/activate && streamlit run $(STREAMLIT_APP)"
	@echo "Then access: http://localhost:8501"

.PHONY: streamlit-manual
streamlit-manual: all
	@echo "📋 Manual Streamlit startup instructions:"
	@echo "1. Activate environment: source $(STREAMLIT_ENV)/bin/activate"
	@echo "2. Start Streamlit: streamlit run $(STREAMLIT_APP)"
	@echo "3. Access at: http://localhost:8501"
	@echo ""
	@echo "Make sure servers are running with: make start-all"

.PHONY: streamlit-check
streamlit-check:
	@echo "🔍 Checking Streamlit environment..."
	@if [ -d "$(STREAMLIT_ENV)" ]; then \
		echo "✅ Streamlit environment found at ./$(STREAMLIT_ENV)"; \
	else \
		echo "❌ Streamlit environment not found"; \
		echo "Run 'make setup-streamlit-env' to create it"; \
	fi
	@if [ -f "$(STREAMLIT_APP)" ]; then \
		echo "✅ Streamlit app found at ./$(STREAMLIT_APP)"; \
	else \
		echo "❌ Streamlit app not found at ./$(STREAMLIT_APP)"; \
	fi

.PHONY: setup-streamlit-env
setup-streamlit-env:
	@echo "🔧 Setting up Streamlit environment..."
	@python3 -m venv $(STREAMLIT_ENV)
	@echo "📦 Installing Streamlit dependencies..."
	@bash -c "source $(STREAMLIT_ENV)/bin/activate && pip install --upgrade pip"
	@bash -c "source $(STREAMLIT_ENV)/bin/activate && pip install streamlit pandas plotly psutil python-dotenv watchdog"
	@echo "✅ Streamlit environment setup completed!"
	@echo "To activate: source $(STREAMLIT_ENV)/bin/activate"

# Monitoring and health checks
.PHONY: status
status:
	@echo "📊 DFS System Status:"
	@echo ""
	@echo "=== Server Status ==="
	@for port in 5001 5002 5003 5004; do \
		if netstat -tuln 2>/dev/null | grep -q ":$$port " || ss -tuln 2>/dev/null | grep -q ":$$port "; then \
			echo "Port $$port: ✅ Running"; \
		else \
			echo "Port $$port: ❌ Not running"; \
		fi; \
	done
	@echo ""
	@echo "=== Process Status ==="
	@ps aux | grep -E "(S[1-4]|s25client|streamlit)" | grep -v grep || echo "No DFS processes found"
	@echo ""
	@echo "=== Streamlit Environment ==="
	@if [ -d "$(STREAMLIT_ENV)" ]; then \
		echo "Streamlit environment: ✅ Found at ./$(STREAMLIT_ENV)"; \
	else \
		echo "Streamlit environment: ❌ Not found"; \
	fi
	@echo ""
	@echo "=== Storage Directories ==="
	@for dir in $(STORAGE_DIRS); do \
		if [ -d "$$dir" ]; then \
			files=$$(find "$$dir" -type f 2>/dev/null | wc -l); \
			echo "$$dir: ✅ $$files files"; \
		else \
			echo "$$dir: ❌ Directory not found"; \
		fi; \
	done

.PHONY: logs
logs:
	@echo "📋 Displaying recent logs..."
	@tail -n 50 $(LOGDIR)/*.log 2>/dev/null || echo "No log files found"

# Cleanup targets
.PHONY: clean
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BINDIR)
	@rm -f *.o *~
	@rm -f core
	@echo "Clean completed."

.PHONY: clean-logs
clean-logs:
	@echo "🗑️ Cleaning log files..."
	@rm -f $(LOGDIR)/*.log $(LOGDIR)/*.pid
	@echo "Logs cleaned."

.PHONY: clean-storage
clean-storage:
	@echo "⚠️ WARNING: This will delete all stored files!"
	@read -p "Are you sure? (y/N): " confirm && [ "$$confirm" = "y" ] && rm -rf $(STORAGE_DIRS)/* || echo "Cancelled."

# Help target
.PHONY: help
help:
	@echo "🚀 DFS - Available Commands:"
	@echo ""
	@echo "=== Building ==="
	@echo "  all              - Build all components (default)"
	@echo "  servers          - Build only server components"
	@echo "  client           - Build only client"
	@echo "  debug            - Build with debug symbols"
	@echo "  release          - Build optimized release"
	@echo "  analyze          - Build with static analysis"
	@echo ""
	@echo "=== Streamlit (Your Existing Environment) ==="
	@echo "  streamlit-check      - Check Streamlit environment status"
	@echo "  setup-streamlit-env  - Create new Streamlit environment"
	@echo "  streamlit-deploy     - Deploy with existing Streamlit environment"
	@echo "  streamlit-manual     - Show manual startup instructions"
	@echo ""
	@echo "=== Your Familiar Commands ==="
	@echo "  source $(STREAMLIT_ENV)/bin/activate"
	@echo "  streamlit run $(STREAMLIT_APP)"
	@echo ""
	@echo "=== Server Management ==="
	@echo "  start-all        - Start all servers"
	@echo "  stop-all         - Stop all servers"
	@echo "  status           - Show system status"
	@echo "  logs             - Display recent logs"
	@echo ""
	@echo "=== Maintenance ==="
	@echo "  clean            - Clean build artifacts"
	@echo "  clean-logs       - Clean log files"
	@echo "  clean-storage    - Clean stored files (dangerous!)"
	@echo ""
	@echo "=== Development ==="
	@echo "  test             - Run tests (when available)"

# Quick start guide
.PHONY: quickstart
quickstart:
	@echo "🚀 DFS Quick Start Guide"
	@echo "========================"
	@echo ""
	@echo "1. Check your Streamlit environment:"
	@echo "   make streamlit-check"
	@echo ""
	@echo "2. If environment doesn't exist, create it:"
	@echo "   make setup-streamlit-env"
	@echo ""
	@echo "3. Build and start servers:"
	@echo "   make all"
	@echo "   make start-all"
	@echo ""
	@echo "4. Start Streamlit (your familiar way):"
	@echo "   source $(STREAMLIT_ENV)/bin/activate"
	@echo "   streamlit run $(STREAMLIT_APP)"
	@echo ""
	@echo "5. Access web interface at: http://localhost:8501"
	@echo ""
	@echo "Alternative: Automated deployment:"
	@echo "   make streamlit-deploy"

# Prevent make from deleting intermediate files
.PRECIOUS: $(ALL_BINS)

# Mark phony targets
.PHONY: $(filter-out $(ALL_BINS),$(MAKECMDGOALS))