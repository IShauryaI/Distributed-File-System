#!/bin/bash

# Health Check Script for Distributed File System
# Usage: ./health_check.sh [check|quick|storage|resources|logs]

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
SERVERS=("S1:5001" "S2:5002" "S3:5003" "S4:5004")
STORAGE_DIRS=("$HOME/S1" "$HOME/S2" "$HOME/S3" "$HOME/S4")
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOG_DIR="$PROJECT_ROOT/logs"

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

# Check if port is listening
check_port() {
    local port=$1
    if netstat -tuln 2>/dev/null | grep -q ":$port " || ss -tuln 2>/dev/null | grep -q ":$port "; then
        return 0
    else
        return 1
    fi
}

# Check server connectivity
check_server_connectivity() {
    local host=$1
    local port=$2
    
    if timeout 5 bash -c "</dev/tcp/$host/$port" 2>/dev/null; then
        return 0
    else
        return 1
    fi
}

# Check storage directories
check_storage() {
    log "Checking storage directories..."
    
    local issues=0
    
    for dir in "${STORAGE_DIRS[@]}"; do
        if [ -d "$dir" ]; then
            if [ -r "$dir" ] && [ -w "$dir" ]; then
                local file_count=$(find "$dir" -type f 2>/dev/null | wc -l)
                success "Storage: $dir (accessible, $file_count files)"
            else
                error "Storage: $dir (permission denied)"
                issues=$((issues + 1))
            fi
        else
            error "Storage: $dir (directory not found)"
            issues=$((issues + 1))
        fi
    done
    
    return $issues
}

# Check server processes
check_processes() {
    log "Checking server processes..."
    
    local issues=0
    
    for server_info in "${SERVERS[@]}"; do
        local server=$(echo "$server_info" | cut -d: -f1)
        local port=$(echo "$server_info" | cut -d: -f2)
        
        if check_port "$port"; then
            success "Process: $server on port $port (running)"
        else
            error "Process: $server on port $port (not running)"
            issues=$((issues + 1))
        fi
    done
    
    return $issues
}

# Check network connectivity
check_network() {
    log "Checking network connectivity..."
    
    local issues=0
    
    for server_info in "${SERVERS[@]}"; do
        local server=$(echo "$server_info" | cut -d: -f1)
        local port=$(echo "$server_info" | cut -d: -f2)
        
        if check_server_connectivity "127.0.0.1" "$port"; then
            success "Network: $server on localhost:$port (accessible)"
        else
            error "Network: $server on localhost:$port (connection failed)"
            issues=$((issues + 1))
        fi
    done
    
    return $issues
}

# Check system resources
check_resources() {
    log "Checking system resources..."
    
    local issues=0
    
    # Check disk space
    local disk_usage=$(df -h . | awk 'NR==2 {print $5}' | sed 's/%//')
    if [ "$disk_usage" -lt 90 ]; then
        success "Disk space: ${disk_usage}% used (healthy)"
    else
        warning "Disk space: ${disk_usage}% used (getting high)"
        issues=$((issues + 1))
    fi
    
    # Check memory
    if command -v free &> /dev/null; then
        local mem_usage=$(free | grep Mem | awk '{printf("%.0f", $3/$2 * 100.0)}')
        if [ "$mem_usage" -lt 85 ]; then
            success "Memory usage: ${mem_usage}% (healthy)"
        else
            warning "Memory usage: ${mem_usage}% (getting high)"
        fi
    fi
    
    # Check CPU load
    if [ -f /proc/loadavg ]; then
        local load_avg=$(cat /proc/loadavg | awk '{print $1}')
        local cpu_cores=$(nproc)
        local load_percent=$(echo "$load_avg * 100 / $cpu_cores" | bc -l 2>/dev/null | cut -d. -f1 || echo "0")
        
        if [ "$load_percent" -lt 80 ]; then
            success "CPU load: ${load_avg} (${load_percent}% of capacity)"
        else
            warning "CPU load: ${load_avg} (${load_percent}% of capacity - high)"
        fi
    fi
    
    return $issues
}

# Check log files
check_logs() {
    log "Checking log files..."
    
    local issues=0
    
    if [ -d "$LOG_DIR" ]; then
        # Check for error messages in logs
        local error_count=0
        for log_file in "$LOG_DIR"/*.log; do
            if [ -f "$log_file" ]; then
                local errors=$(grep -c "ERROR\|FATAL\|CRITICAL" "$log_file" 2>/dev/null || echo "0")
                if [ "$errors" -gt 0 ]; then
                    warning "Log file $(basename "$log_file"): $errors error messages found"
                    error_count=$((error_count + errors))
                fi
            fi
        done
        
        if [ $error_count -eq 0 ]; then
            success "Log files: No critical errors found"
        else
            error "Log files: $error_count critical errors found across all logs"
            issues=$((issues + 1))
        fi
        
        # Check log file sizes
        for log_file in "$LOG_DIR"/*.log; do
            if [ -f "$log_file" ]; then
                local size=$(stat -c%s "$log_file" 2>/dev/null || stat -f%z "$log_file" 2>/dev/null || echo 0)
                local size_mb=$((size / 1024 / 1024))
                
                if [ $size_mb -gt 100 ]; then
                    warning "Log file $(basename "$log_file"): ${size_mb}MB (consider rotation)"
                fi
            fi
        done
    else
        warning "Log directory not found: $LOG_DIR"
    fi
    
    return $issues
}

# Test basic functionality
test_functionality() {
    log "Testing basic functionality..."
    
    local issues=0
    
    # Test if client can connect to S1
    if check_server_connectivity "127.0.0.1" "5001"; then
        success "Functionality: Client can connect to S1"
    else
        error "Functionality: Client cannot connect to S1"
        issues=$((issues + 1))
        return $issues
    fi
    
    # Test file operations (if client binary exists)
    if [ -f "$PROJECT_ROOT/bin/s25client" ]; then
        # Create a test file
        local test_file="/tmp/dfs_health_test.txt"
        echo "Health check test file $(date)" > "$test_file"
        
        # Attempt file list operation (safest test)
        local test_output
        test_output=$(timeout 10 echo "dispfnames ~/S1/" | "$PROJECT_ROOT/bin/s25client" 127.0.0.1 5001 2>&1 || echo "FAILED")
        
        if [[ "$test_output" != *"FAILED"* ]] && [[ "$test_output" != *"ERROR"* ]]; then
            success "Functionality: File listing works"
        else
            warning "Functionality: File listing may have issues"
        fi
        
        # Cleanup
        rm -f "$test_file"
    else
        warning "Functionality: Client binary not found, skipping functionality tests"
    fi
    
    return $issues
}

# Check Streamlit web interface
check_streamlit() {
    if check_port "8501"; then
        success "Streamlit: Web interface running on port 8501"
        log "Access URL: http://localhost:8501"
    else
        warning "Streamlit: Web interface not running"
    fi
}

# Generate health report
generate_report() {
    local total_issues=$1
    
    echo
    echo "======================================"
    echo "     DFS HEALTH CHECK REPORT"
    echo "======================================"
    echo "Timestamp: $(date)"
    echo "Host: $(hostname)"
    echo "User: $(whoami)"
    echo "Project Root: $PROJECT_ROOT"
    echo
    
    if [ $total_issues -eq 0 ]; then
        echo -e "${GREEN}✅ SYSTEM STATUS: HEALTHY${NC}"
        echo "All checks passed successfully."
    elif [ $total_issues -lt 3 ]; then
        echo -e "${YELLOW}⚠️  SYSTEM STATUS: MINOR ISSUES${NC}"
        echo "Some minor issues detected, but system is functional."
    else
        echo -e "${RED}❌ SYSTEM STATUS: CRITICAL ISSUES${NC}"
        echo "Multiple issues detected. Immediate attention required."
    fi
    
    echo
    echo "Total issues found: $total_issues"
    echo
    
    # Provide recommendations
    if [ $total_issues -gt 0 ]; then
        echo "RECOMMENDATIONS:"
        echo "- Check server logs in $LOG_DIR/"
        echo "- Verify all servers are started with: make start-all"
        echo "- Restart services if needed: scripts/deploy.sh stop && scripts/deploy.sh streamlit"
        echo "- Check system resources and free up space if needed"
    fi
    
    echo "======================================"
}

# Main health check function
main() {
    log "Starting DFS health check..."
    
    local total_issues=0
    
    # Run all checks
    check_storage
    total_issues=$((total_issues + $?))
    
    check_processes
    total_issues=$((total_issues + $?))
    
    check_network
    total_issues=$((total_issues + $?))
    
    check_resources
    total_issues=$((total_issues + $?))
    
    check_logs
    total_issues=$((total_issues + $?))
    
    test_functionality
    total_issues=$((total_issues + $?))
    
    check_streamlit
    
    # Generate final report
    generate_report $total_issues
    
    # Exit with appropriate code
    if [ $total_issues -eq 0 ]; then
        exit 0
    elif [ $total_issues -lt 3 ]; then
        exit 1
    else
        exit 2
    fi
}

# Handle command line arguments
case "${1:-check}" in
    "check"|"")
        main
        ;;
    "quick")
        log "Running quick health check..."
        check_processes
        check_network
        ;;
    "storage")
        check_storage
        ;;
    "resources")
        check_resources
        ;;
    "logs")
        check_logs
        ;;
    *)
        echo "Usage: $0 [check|quick|storage|resources|logs]"
        echo "  check     - Full health check (default)"
        echo "  quick     - Quick check of processes and network"
        echo "  storage   - Check storage directories only"
        echo "  resources - Check system resources only"
        echo "  logs      - Check log files only"
        exit 1
        ;;
esac