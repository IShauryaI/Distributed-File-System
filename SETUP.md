# 🚀 Setup Guide - Distributed File System

This guide will help you get the **professional distributed file system** up and running with your existing **Streamlit environment**.

## 📋 Prerequisites

### System Requirements
- **OS**: Linux/Unix (Ubuntu 20.04+ recommended)
- **Compiler**: GCC 9.0+
- **Python**: 3.8+ (for Streamlit web interface)
- **Storage**: 2GB free space minimum
- **Network**: Local network access (ports 5001-5004, 8501)

### Required Packages
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install -y build-essential gcc make python3 python3-pip git

# CentOS/RHEL
sudo yum groupinstall "Development Tools"
sudo yum install python3 python3-pip
```

## 🏗️ Quick Setup (Your Existing Workflow)

Since you already have a Streamlit environment and working files, here's the quickest way to get the professional features:

### 1. Check Your Current Setup
```bash
# Your repository should have:
# ✅ streamlit_app.py
# ✅ streamlit_env/ directory
# ✅ S1.c, S2.c, S3.c, S4.c, s25client.c files
# ✅ Basic makefile

# Verify Streamlit environment
ls -la streamlit_env/
source streamlit_env/bin/activate
python --version
streamlit --version
deactivate
```

### 2. Build and Test
```bash
# Build all servers and client
make all

# Check build was successful
ls -la bin/

# Test basic functionality
make status
```

### 3. Start Your System (Your Familiar Way)
```bash
# Option A: Your existing workflow (unchanged)
source streamlit_env/bin/activate
streamlit run streamlit_app.py
# Access: http://localhost:8501

# Option B: Use new automation
make streamlit-deploy  # Builds servers + shows Streamlit command
```

## 🔧 Professional Features Added

### Enhanced Build System
```bash
# Available Make targets
make help                 # Show all commands
make all                 # Build everything
make start-all           # Start all servers automatically
make stop-all            # Stop all servers
make status             # Check system status
make streamlit-check    # Verify your Streamlit setup
make clean              # Clean build artifacts
```

### Automated Deployment
```bash
# Professional deployment options
./scripts/deploy.sh streamlit     # Use your existing Streamlit env
./scripts/deploy.sh production    # Production deployment
./scripts/deploy.sh stop          # Stop all services
./scripts/deploy.sh status        # System status
```

### Health Monitoring
```bash
# System health checks
./scripts/health_check.sh         # Complete health check
./scripts/health_check.sh quick   # Quick status check
```

## 📁 Professional Directory Structure

Your repository now has this structure:
```
distributed-file-system/
├── README.md              # Professional documentation
├── Makefile              # Enhanced build system
├── requirements.txt      # Python dependencies
├── .gitignore           # Comprehensive Git rules
├── LICENSE              # MIT License
├── streamlit_app.py     # Your existing Streamlit app
├── streamlit_env/       # Your existing Python environment
├── S1.c, S2.c, S3.c, S4.c, s25client.c  # Your C source files
├── makefile             # Your original simple makefile
├── bin/                 # Compiled binaries (auto-created)
├── logs/                # System logs (auto-created)
├── scripts/
│   ├── deploy.sh        # Automated deployment
│   └── health_check.sh  # Health monitoring
└── docs/               # Additional documentation (can be added)
```

## 🎯 Usage Examples

### Development Workflow
```bash
# Daily development
make all                 # Build after code changes
make start-all          # Start servers in background
make status             # Check everything is running

# Start Streamlit (your way)
source streamlit_env/bin/activate
streamlit run streamlit_app.py

# When done
make stop-all           # Stop servers
deactivate              # Exit Python environment
```

### Professional Demo
```bash
# One-command professional demo
make streamlit-deploy
# Then follow the displayed instructions to start Streamlit
```

### Troubleshooting
```bash
# Check system health
./scripts/health_check.sh

# View system status
make status

# View recent logs
make logs

# Clean and rebuild
make clean && make all

# Stop everything and restart
make stop-all
make start-all
```

## 🌟 Key Benefits You Get

### ✅ Zero Disruption
- Your existing `source streamlit_env/bin/activate` still works
- Your existing `streamlit run streamlit_app.py` still works
- All your current development workflow is preserved

### ✅ Professional Features
- **Automated server management**: `make start-all`, `make stop-all`
- **Health monitoring**: `./scripts/health_check.sh`
- **Professional documentation**: Complete README, setup guides
- **Build automation**: 20+ make targets for development needs
- **Production deployment**: `./scripts/deploy.sh production`

### ✅ Enhanced Development
- **Comprehensive .gitignore**: Proper version control
- **Professional structure**: Industry-standard repository layout
- **MIT License**: Open source ready
- **Logging system**: Structured logs with rotation
- **Status monitoring**: Real-time system status

## 🔍 Testing Your Setup

### 1. Build Test
```bash
# Test the build system
make clean
make all

# Should see:
# ✅ Building S1 (Main Server)...
# ✅ Building S2 (PDF Server)...
# ✅ Building S3 (TXT Server)...
# ✅ Building S4 (ZIP Server)...
# ✅ Building Client...
```

### 2. Server Test
```bash
# Test server automation
make start-all
make status

# Should see all ports 5001-5004 running
# ✅ Port 5001: Running
# ✅ Port 5002: Running
# ✅ Port 5003: Running
# ✅ Port 5004: Running
```

### 3. Streamlit Test
```bash
# Test your Streamlit environment
make streamlit-check

# Should see:
# ✅ Streamlit environment found
# ✅ Streamlit app found

# Then start as usual:
source streamlit_env/bin/activate
streamlit run streamlit_app.py
```

### 4. Health Test
```bash
# Test health monitoring
./scripts/health_check.sh

# Should see comprehensive system report
# ✅ SYSTEM STATUS: HEALTHY
```

## ❓ Troubleshooting

### Issue: "make: command not found"
```bash
# Install make
sudo apt install make  # Ubuntu/Debian
sudo yum install make  # CentOS/RHEL
```

### Issue: "gcc: command not found"
```bash
# Install build tools
sudo apt install build-essential  # Ubuntu/Debian
sudo yum groupinstall "Development Tools"  # CentOS/RHEL
```

### Issue: "Permission denied" on scripts
```bash
# Make scripts executable
chmod +x scripts/*.sh
```

### Issue: "Port already in use"
```bash
# Stop existing processes
make stop-all
# Or manually:
sudo lsof -i :5001
sudo kill -9 <PID>
```

### Issue: "Streamlit environment not found"
```bash
# Verify your environment exists
ls -la streamlit_env/

# If missing, recreate:
make setup-streamlit-env
```

### Issue: Servers won't start
```bash
# Check system health
./scripts/health_check.sh

# Verify storage directories
ls -la ~/S1 ~/S2 ~/S3 ~/S4

# Create if missing:
mkdir -p ~/S1 ~/S2 ~/S3 ~/S4
```

## 🎓 Next Steps

### For Continued Development
1. **Explore the enhanced Makefile**: `make help`
2. **Use health monitoring**: `./scripts/health_check.sh`
3. **Check logs regularly**: `make logs`
4. **Try production deployment**: `./scripts/deploy.sh production`

### For Production Use
1. **Review security settings** in deployment scripts
2. **Set up log rotation** for long-running systems
3. **Configure firewall rules** for network access
4. **Set up monitoring** for production environments

### For Portfolio/Demo
1. **Use the professional README** to showcase your work
2. **Demonstrate the web interface** at http://localhost:8501
3. **Show the automated deployment** with `make streamlit-deploy`
4. **Highlight the comprehensive testing** with health checks

## 💡 Tips

- **Keep your workflow**: Your existing commands still work exactly the same
- **Use make targets**: They provide consistent, reliable operations
- **Check status regularly**: `make status` gives you quick system overview
- **Monitor health**: `./scripts/health_check.sh` catches issues early
- **Read the logs**: `make logs` helps debug any issues

---

**🎉 Congratulations!** You now have a professional, production-ready distributed file system that preserves your existing workflow while adding enterprise-grade features.

Your academic project is now a showcase-ready professional system! 🚀