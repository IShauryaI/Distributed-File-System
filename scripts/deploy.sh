#!/bin/bash

# Distributed File System Deployment Script
# Updated for existing Streamlit environment
# Usage: ./deploy.sh [streamlit|production|development|stop|status]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging function
log() {
    echo -e "${BLUE}[$(date +'%Y-%m-%d %H:%M:%S')]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Configuration
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_DIR="$PROJECT_ROOT/bin"
LOG_DIR="$PROJECT_ROOT/logs"
STORAGE_DIRS=("$HOME/S1" "$HOME/S2" "$HOME/S3" "$HOME/S4")

# Streamlit environment configuration
STREAMLIT_ENV_PATH="$PROJECT_ROOT/streamlit_env"
STREAMLIT_APP="streamlit_app.py"

# Check if streamlit environment exists
check_streamlit_env() {
    if [ -d "$STREAMLIT_ENV_PATH" ]; then
        log "Found existing Streamlit environment at: $STREAMLIT_ENV_PATH"
        return 0
    else
        warning "Streamlit environment not found at: $STREAMLIT_ENV_PATH"
        return 1
    fi
}

# Check dependencies
check_dependencies() {
    log "Checking dependencies..."
    
    # Check GCC
    if ! command -v gcc &> /dev/null; then
        error "GCC compiler not found. Please install build-essential."
        exit 1
    fi
    
    # Check if streamlit_app.py exists
    if [ ! -f "$STREAMLIT_APP" ]; then
        warning "streamlit_app.py not found in current directory"
        if [ ! -f "$PROJECT_ROOT/$STREAMLIT_APP" ]; then
            error "No Streamlit app file found. Please ensure streamlit_app.py exists."
            exit 1
        fi
    fi
    
    success "Dependencies check completed."
}

# Setup storage directories
setup_storage() {
    log "Setting up storage directories..."
    
    for dir in "${STORAGE_DIRS[@]}"; do
        if [ ! -d "$dir" ]; then
            mkdir -p "$dir"
            log "Created directory: $dir"
        else
            log "Directory already exists: $dir"
        fi
        
        # Set proper permissions
        chmod 755 "$dir"
    done
    
    success "Storage directories configured."
}

# Build the project
build_project() {
    log "Building DFS project..."
    
    cd "$PROJECT_ROOT"
    
    # Clean previous builds
    make clean 2>/dev/null || true
    
    # Build all components
    if make all; then
        success "Project built successfully."
    else
        error "Build failed. Check the error messages above."
        exit 1
    fi
}

# Start servers in background
start_servers_background() {
    log "Starting DFS servers in background..."
    
    # Ensure log directory exists
    mkdir -p "$LOG_DIR"
    
    # Start S2, S3, S4 first
    for server in S2 S3 S4; do
        port=$((5000 + ${server#S}))
        log "Starting $server on port $port..."
        
        nohup "$BIN_DIR/$server" $port > "$LOG_DIR/${server,,}.log" 2>&1 &
        echo $! > "$LOG_DIR/${server,,}.pid"
        
        sleep 1
    done
    
    # Start S1 (main server)
    log "Starting S1 (main server) on port 5001..."
    nohup "$BIN_DIR/S1" 5001 127.0.0.1 5002 127.0.0.1 5003 127.0.0.1 5004 > "$LOG_DIR/s1.log" 2>&1 &
    echo $! > "$LOG_DIR/s1.pid"
    
    sleep 2
    
    # Verify servers are running
    verify_servers
}

# Verify servers are running
verify_servers() {
    log "Verifying server status..."
    
    local failed=0
    for port in 5001 5002 5003 5004; do
        if netstat -tuln 2>/dev/null | grep -q ":$port " || ss -tuln 2>/dev/null | grep -q ":$port "; then
            success "Server on port $port: Running"
        else
            error "Server on port $port: Not running"
            failed=1
        fi
    done
    
    if [ $failed -eq 0 ]; then
        success "All servers are running successfully!"
    else
        error "Some servers failed to start. Check logs in $LOG_DIR/"
        return 1
    fi
}

# Streamlit deployment with existing environment
deploy_streamlit() {
    log "Starting Streamlit deployment with existing environment..."
    
    # Check if streamlit environment exists
    if ! check_streamlit_env; then
        error "Streamlit environment not found. Please create it first:"
        echo "  python3 -m venv streamlit_env"
        echo "  source streamlit_env/bin/activate"
        echo "  pip install streamlit pandas plotly psutil"
        exit 1
    fi
    
    # Start servers in background
    start_servers_background
    
    success "Streamlit deployment setup completed!"
    log "Your servers are now running in the background."
    log ""
    log "To start Streamlit web interface, run:"
    log "  cd $PROJECT_ROOT"
    log "  source streamlit_env/bin/activate"
    log "  streamlit run $STREAMLIT_APP"
    log ""
    log "Then access the web interface at: http://localhost:8501"
}

# Production deployment
deploy_production() {
    log "Starting production deployment..."
    
    # Build optimized version
    cd "$PROJECT_ROOT"
    make release
    
    # Start servers with production settings
    start_servers_background
    
    success "Production deployment completed!"
    log "Servers are running in background. Check status with: make status"
}

# Development deployment
deploy_development() {
    log "Starting development deployment..."
    
    # Build debug version
    cd "$PROJECT_ROOT"
    make debug
    
    log "Development build completed!"
    log "To start servers manually:"
    log "  Terminal 1: ./bin/S2 5002"
    log "  Terminal 2: ./bin/S3 5003"
    log "  Terminal 3: ./bin/S4 5004"
    log "  Terminal 4: ./bin/S1 5001 127.0.0.1 5002 127.0.0.1 5003 127.0.0.1 5004"
    log "  Terminal 5: ./bin/s25client 127.0.0.1 5001"
}

# Stop all services
stop_services() {
    log "Stopping all DFS services..."
    
    # Stop servers using PID files
    for pidfile in "$LOG_DIR"/*.pid; do
        if [ -f "$pidfile" ]; then
            pid=$(cat "$pidfile")
            if kill -0 "$pid" 2>/dev/null; then
                log "Stopping process $pid..."
                kill "$pid"
                rm "$pidfile"
            fi
        fi
    done
    
    # Additional cleanup
    pkill -f "S[1-4]" 2>/dev/null || true
    pkill -f "s25client" 2>/dev/null || true
    pkill -f "streamlit" 2>/dev/null || true
    
    success "All services stopped."
}

# Show status
show_status() {
    log "DFS System Status:"
    
    echo
    echo "=== Server Status ==="
    for port in 5001 5002 5003 5004; do
        if netstat -tuln 2>/dev/null | grep -q ":$port " || ss -tuln 2>/dev/null | grep -q ":$port "; then
            echo "Port $port: ✅ Running"
        else
            echo "Port $port: ❌ Not running"
        fi
    done
    
    echo
    echo "=== Process Status ==="
    ps aux | grep -E "(S[1-4]|s25client|streamlit)" | grep -v grep || echo "No DFS processes found"
    
    echo
    echo "=== Streamlit Environment ==="
    if [ -d "$STREAMLIT_ENV_PATH" ]; then
        echo "Streamlit environment: ✅ Found at $STREAMLIT_ENV_PATH"
        if [ -f "$STREAMLIT_APP" ]; then
            echo "Streamlit app: ✅ Found at $STREAMLIT_APP"
        else
            echo "Streamlit app: ❌ Not found at $STREAMLIT_APP"
        fi
    else
        echo "Streamlit environment: ❌ Not found"
    fi
    
    echo
    echo "=== Storage Directories ==="
    for dir in "${STORAGE_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            files=$(find "$dir" -type f 2>/dev/null | wc -l)
            echo "$dir: ✅ $files files"
        else
            echo "$dir: ❌ Directory not found"
        fi
    done
    
    echo
    echo "=== Quick Start Commands ==="
    echo "Start Streamlit: source streamlit_env/bin/activate && streamlit run $STREAMLIT_APP"
    echo "Check health: ./scripts/health_check.sh (when available)"
    echo "Stop all: ./scripts/deploy.sh stop"
}

# Main deployment logic
main() {
    local deployment_type="${1:-streamlit}"
    
    log "Starting DFS deployment (type: $deployment_type)"
    
    # Initial setup
    check_dependencies
    setup_storage
    build_project
    
    # Deployment type specific actions
    case "$deployment_type" in
        "streamlit")
            deploy_streamlit
            ;;
        "production")
            deploy_production
            ;;
        "development")
            deploy_development
            ;;
        "stop")
            stop_services
            exit 0
            ;;
        "status")
            show_status
            exit 0
            ;;
        *)
            error "Unknown deployment type: $deployment_type"
            echo "Usage: $0 [streamlit|production|development|stop|status]"
            exit 1
            ;;
    esac
    
    # Show final status
    sleep 3
    show_status
    
    log "Deployment completed successfully!"
}

# Handle script termination
cleanup() {
    log "Cleanup requested..."
    stop_services
    exit 0
}

trap cleanup SIGINT SIGTERM

# Run main function
main "$@"