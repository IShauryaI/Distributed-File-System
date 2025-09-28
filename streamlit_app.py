import streamlit as st
import subprocess
import socket
import time
import tempfile
from pathlib import Path
import threading
import os

st.set_page_config(page_title="Distributed File System", layout="wide")

class DFSManager:
    def __init__(self):
        self.base_dir = Path.cwd()
        self.bin_dir = self.base_dir / "bin"
        self.logs_dir = self.base_dir / "logs"
        self.logs_dir.mkdir(exist_ok=True)
        
    def check_port(self, port):
        """Check if a port is in use"""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.bind(('localhost', port))
                return False
        except OSError:
            return True
    
    def start_servers(self):
        """Start all DFS servers"""
        try:
            # Start backend servers first
            subprocess.Popen([str(self.bin_dir / "S2"), "5002"], 
                           stdout=open(self.logs_dir / "s2.log", "w"),
                           stderr=subprocess.STDOUT)
            time.sleep(1)
            
            subprocess.Popen([str(self.bin_dir / "S3"), "5003"],
                           stdout=open(self.logs_dir / "s3.log", "w"), 
                           stderr=subprocess.STDOUT)
            time.sleep(1)
            
            subprocess.Popen([str(self.bin_dir / "S4"), "5004"],
                           stdout=open(self.logs_dir / "s4.log", "w"),
                           stderr=subprocess.STDOUT)
            time.sleep(1)
            
            # Start main server
            subprocess.Popen([str(self.bin_dir / "S1"), "5001", "127.0.0.1", "5002", 
                            "127.0.0.1", "5003", "127.0.0.1", "5004"],
                           stdout=open(self.logs_dir / "s1.log", "w"),
                           stderr=subprocess.STDOUT)
            time.sleep(2)
            
            return True
        except Exception as e:
            st.error(f"Failed to start servers: {e}")
            return False
    
    def stop_servers(self):
        """Stop all DFS servers"""
        try:
            subprocess.run(["pkill", "-f", "S[1-4]"], check=False)
            time.sleep(2)
            return True
        except Exception as e:
            st.error(f"Failed to stop servers: {e}")
            return False
    
    def send_command(self, command, timeout=10):
        """Send command to S1 server and get response"""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(timeout)
                s.connect(('127.0.0.1', 5001))
                s.send((command + '\n').encode())
                
                response = ""
                while True:
                    try:
                        data = s.recv(1024).decode()
                        if not data:
                            break
                        response += data
                        # Check for end markers based on the protocol
                        if any(marker in response for marker in ["LISTEND", "DONE", "OK\n", "ERR|"]):
                            break
                    except socket.timeout:
                        break
                
                return response.strip() if response.strip() else "Command completed"
        except ConnectionRefusedError:
            return "Error: Cannot connect to S1 server. Make sure servers are running."
        except Exception as e:
            return f"Error: {e}"
    
    def upload_files_to_server(self, files):
        """Handle file upload with exact protocol from S1.c"""
        if not files or len(files) > 3:
            return "Error: Upload 1-3 files only"
        
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(15)
                s.connect(('127.0.0.1', 5001))
                
                # Step 1: Send upload header: UPLOADF|<num_files>|<destination>
                # Destination is fixed as ~S1/ (client cannot choose per assignment requirements)
                num_files = len(files)
                destination = "~S1/"
                
                upload_cmd = f"UPLOADF|{num_files}|{destination}"
                s.send((upload_cmd + '\n').encode())
                
                # Step 2: For each file, send FILEMETA|<filename>|<filesize> then file bytes
                for file in files:
                    filename = file.name
                    file_content = file.getvalue()
                    file_size = len(file_content)
                    
                    # Send file metadata
                    meta_cmd = f"FILEMETA|{filename}|{file_size}"
                    s.send((meta_cmd + '\n').encode())
                    
                    # Send file bytes
                    s.sendall(file_content)
                
                # Step 3: Get response
                response = ""
                while True:
                    try:
                        data = s.recv(1024).decode()
                        if not data:
                            break
                        response += data
                        if any(marker in response for marker in ["OK", "ERR|"]):
                            break
                    except socket.timeout:
                        break
                
                if "OK" in response:
                    return f"✅ Successfully uploaded {num_files} files to {destination}"
                else:
                    return f"❌ Upload failed: {response}"
                    
        except Exception as e:
            return f"❌ Upload error: {e}"

# Initialize DFS manager
dfs = DFSManager()

# Main UI
st.title("Distributed File System")
st.markdown("C-based distributed file system with Streamlit web interface")

# Sidebar for server management
with st.sidebar:
    st.header("Server Control")
    
    # Server status
    ports = [5001, 5002, 5003, 5004]
    servers_running = all(dfs.check_port(port) for port in ports)
    
    if servers_running:
        st.success("✅ Servers Running")
    else:
        st.error("❌ Servers Stopped")
    
    # Server controls
    col1, col2 = st.columns(2)
    with col1:
        if st.button("Start Servers", disabled=servers_running):
            with st.spinner("Starting servers..."):
                if dfs.start_servers():
                    st.success("Servers started!")
                    time.sleep(2)
                    st.rerun()
    
    with col2:
        if st.button("Stop Servers", disabled=not servers_running):
            with st.spinner("Stopping servers..."):
                if dfs.stop_servers():
                    st.success("Servers stopped!")
                    time.sleep(2)
                    st.rerun()
    
    # Port status display
    st.subheader("Port Status")
    ports_info = [("S1 Main", 5001), ("S2 PDF", 5002), ("S3 TXT", 5003), ("S4 ZIP", 5004)]
    for name, port in ports_info:
        if dfs.check_port(port):
            st.success(f"✅ {name}: {port}")
        else:
            st.error(f"❌ {name}: {port}")

# Main content area
if not servers_running:
    st.warning("⚠️ Servers are not running. Please start them using the sidebar.")
    st.info("Click 'Start Servers' in the sidebar to begin.")
else:
    # Create tabs for different operations
    tab1, tab2, tab3, tab4 = st.tabs(["📤 Upload Files", "💻 Send Command", "📁 File Browser", "📊 System Logs"])
    
    with tab1:
        st.header("Upload Files")
        st.markdown("Upload 1-3 files to the distributed file system")
        
        # File distribution info (read-only - client cannot choose destination)
        st.info("""
        **Automatic File Distribution:**
        - 📄 `.c` files → S1 Server (~/S1/)
        - 📋 `.pdf` files → S2 Server  
        - 📝 `.txt` files → S3 Server
        - 📦 `.zip` files → S4 Server
        
        **Note:** Files are automatically stored in `~S1/` directory. The system will route them to appropriate servers based on file extension.
        """)
        
        uploaded_files = st.file_uploader(
            "Choose files (max 3)", 
            accept_multiple_files=True,
            type=['c', 'pdf', 'txt', 'zip'],
            help="Maximum 3 files. Supported types: .c, .pdf, .txt, .zip"
        )
        
        if uploaded_files:
            if len(uploaded_files) <= 3:
                st.success(f"Selected {len(uploaded_files)} files:")
                for file in uploaded_files:
                    file_ext = Path(file.name).suffix.lower()
                    if file_ext == '.c':
                        destination = "S1 Server"
                    elif file_ext == '.pdf':
                        destination = "S2 Server"
                    elif file_ext == '.txt':
                        destination = "S3 Server"
                    elif file_ext == '.zip':
                        destination = "S4 Server"
                    else:
                        destination = "Unknown"
                    
                    st.write(f"- 📄 **{file.name}** ({file.size} bytes) → {destination}")
                
                if st.button("Upload Files", type="primary"):
                    with st.spinner("Uploading files..."):
                        result = dfs.upload_files_to_server(uploaded_files)
                        
                        if result.startswith("✅"):
                            st.success(result)
                        else:
                            st.error(result)
            else:
                st.error("❌ Maximum 3 files allowed")
    
    with tab2:
        st.header("Send Command")
        st.markdown("Send commands directly to the DFS server")
        
        # Command input with correct default
        command = st.text_input(
            "Command", 
            value="DISP|~S1/",
            help="Enter DFS commands using pipe-separated format"
        )
        
        # Common commands as buttons
        st.markdown("**Quick Commands:**")
        col1, col2, col3, col4 = st.columns(4)
        
        with col1:
            if st.button("List All Files"):
                command = "DISP|~S1/"
        
        with col2:
            if st.button("Download C File"):
                command = "DOWNLF|1|~S1/example.c"
        
        with col3:
            if st.button("Remove File"):
                command = "REMOVEF|1|~S1/example.c"
        
        with col4:
            if st.button("Download C Archive"):
                command = "DOWNTAR|.c"
        
        # Help section with correct protocol
        with st.expander("📚 Command Reference"):
            st.markdown("""
            **Upload Files:**
            - Use the Upload tab above (implements the complex UPLOADF protocol)
            
            **Download Files:**
            - `DOWNLF|1|~S1/filename.c` - Download 1 file
            - `DOWNLF|2|~S1/file1.c|~S1/file2.txt` - Download 2 files
            
            **Remove Files:**
            - `REMOVEF|1|~S1/filename.c` - Remove 1 file
            - `REMOVEF|2|~S1/file1.c|~S1/file2.txt` - Remove 2 files
            
            **List Files:**
            - `DISP|~S1/` - List all files in root directory
            - `DISP|~S1/subfolder/` - List files in subfolder
            
            **Download Archives:**
            - `DOWNTAR|.c` - Download tar of all C files
            - `DOWNTAR|.pdf` - Download tar of all PDF files  
            - `DOWNTAR|.txt` - Download tar of all TXT files
            
            **Protocol Format:** Commands use pipe-separated values: `COMMAND|param1|param2|...`
            """)
        
        # Send command
        if st.button("Send Command", type="primary"):
            if command.strip():
                with st.spinner(f"Executing: {command}"):
                    result = dfs.send_command(command)
                    
                    if "ERR|" in result:
                        st.error(f"❌ Command failed: {result}")
                    else:
                        st.success("✅ Command executed successfully!")
                        
                        # Parse and display results based on command type
                        if command.startswith("DISP|"):
                            st.subheader("📁 File Listing Results")
                            
                            lines = result.split('\n')
                            file_types = {".c": [], ".pdf": [], ".txt": [], ".zip": []}
                            
                            # Parse the response
                            in_list = False
                            for line in lines:
                                line = line.strip()
                                if line == "LISTBEGIN":
                                    in_list = True
                                elif line == "LISTEND":
                                    break
                                elif in_list and line.startswith("NAME|"):
                                    parts = line.split("|")
                                    if len(parts) >= 3:
                                        ext = parts[1]
                                        filename = parts[2]
                                        if ext in file_types:
                                            file_types[ext].append(filename)
                            
                            # Display files by type
                            cols = st.columns(4)
                            type_names = [("C Files", ".c"), ("PDF Files", ".pdf"), ("TXT Files", ".txt"), ("ZIP Files", ".zip")]
                            
                            for i, (type_name, ext) in enumerate(type_names):
                                with cols[i]:
                                    st.markdown(f"**{type_name}** ({len(file_types[ext])})")
                                    if file_types[ext]:
                                        for filename in file_types[ext]:
                                            st.write(f"📄 {filename}")
                                    else:
                                        st.write("_No files_")
                        
                        # Show raw output
                        with st.expander("Raw Command Output"):
                            st.code(result, language="text")
            else:
                st.warning("⚠️ Please enter a command")
    
    with tab3:
        st.header("File Browser")
        st.markdown("Browse files across all servers")
        
        # Path input
        browse_path = st.text_input("Directory Path", value="~S1/", placeholder="~S1/subfolder/")
        
        if st.button("Browse Directory", type="primary"):
            with st.spinner("Fetching directory contents..."):
                result = dfs.send_command(f"DISP|{browse_path}")
                
                if "ERR|" in result:
                    st.error(f"❌ Error: {result}")
                else:
                    st.success(f"✅ Listing files in: {browse_path}")
                    
                    # Parse results
                    lines = result.split('\n')
                    file_data = {'.c': [], '.pdf': [], '.txt': [], '.zip': []}
                    
                    in_list = False
                    for line in lines:
                        line = line.strip()
                        if line == "LISTBEGIN":
                            in_list = True
                        elif line == "LISTEND":
                            break
                        elif in_list and line.startswith("NAME|"):
                            parts = line.split("|")
                            if len(parts) >= 3:
                                ext = parts[1]
                                filename = parts[2]
                                if ext in file_data:
                                    file_data[ext].append(filename)
                    
                    # Display in organized columns
                    col1, col2, col3, col4 = st.columns(4)
                    columns = [col1, col2, col3, col4]
                    type_info = [
                        ("C Files", ".c", "📄"),
                        ("PDF Files", ".pdf", "📋"),
                        ("TXT Files", ".txt", "📝"),
                        ("ZIP Files", ".zip", "📦")
                    ]
                    
                    for i, (type_name, ext, icon) in enumerate(type_info):
                        with columns[i]:
                            st.subheader(f"{icon} {type_name}")
                            files = file_data[ext]
                            if files:
                                for filename in files:
                                    st.write(f"• {filename}")
                                st.caption(f"Total: {len(files)} files")
                            else:
                                st.info("No files")
    
    with tab4:
        st.header("System Logs")
        
        # Log file selector
        log_files = ["s1.log", "s2.log", "s3.log", "s4.log"]
        selected_log = st.selectbox("Select Log File", log_files)
        
        col1, col2 = st.columns([1, 4])
        with col1:
            auto_refresh = st.checkbox("Auto Refresh")
            refresh_clicked = st.button("Refresh Now")
            
        if refresh_clicked or auto_refresh:
            log_path = dfs.logs_dir / selected_log
            if log_path.exists():
                try:
                    with open(log_path, 'r') as f:
                        content = f.read()
                        if content:
                            # Show last 50 lines
                            lines = content.split('\n')
                            recent_lines = lines[-50:] if len(lines) > 50 else lines
                            with col2:
                                st.code('\n'.join(recent_lines), language="text")
                        else:
                            with col2:
                                st.info("Log file is empty")
                except Exception as e:
                    with col2:
                        st.error(f"Error reading log: {e}")
            else:
                with col2:
                    st.warning("Log file not found")
        
        # Auto refresh
        if auto_refresh:
            time.sleep(3)
            st.rerun()

# Footer with system info
st.markdown("---")
col1, col2, col3 = st.columns(3)

with col1:
    active_servers = sum(1 for port in [5001, 5002, 5003, 5004] if dfs.check_port(port))
    st.metric("Active Servers", active_servers, delta=f"{active_servers}/4")

with col2:
    st.metric("Architecture", "Distributed")

with col3:
    st.metric("Protocol", "TCP Sockets")

# Quick instructions
with st.expander("📖 Quick Start Guide"):
    st.markdown("""
    **Getting Started:**
    1. ✅ Click 'Start Servers' in the sidebar
    2. 📤 Upload files using the Upload tab (max 3 files)
    3. 💻 Use Send Command tab for file operations
    4. 📁 Browse files using the File Browser
    
    **File Storage:**
    - Files are automatically stored in `~S1/` directory
    - System routes files to appropriate servers by extension:
      - `.c` files stay on S1
      - `.pdf` files go to S2  
      - `.txt` files go to S3
      - `.zip` files go to S4
    
    **Key Commands:**
    - `DISP|~S1/` - List all files
    - `DOWNLF|1|~S1/file.c` - Download a file
    - `REMOVEF|1|~S1/file.c` - Remove a file
    - `DOWNTAR|.c` - Download archive of C files
    
    **Protocol Notes:**
    - Commands use pipe-separated format: `COMMAND|param1|param2`
    - Upload is handled automatically through the web interface
    - All file paths must start with `~S1/`
    """)
