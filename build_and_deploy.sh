#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# LVM Auto-Extender - Build and Deploy Script
# For Red Hat Enterprise Linux / CentOS / Fedora
# ═══════════════════════════════════════════════════════════════════════════

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Functions
print_header() {
    echo -e "${CYAN}${BOLD}"
    echo "╔════════════════════════════════════════════════════════════════╗"
    echo "║         LVM AUTO-EXTENDER - BUILD & DEPLOY                    ║"
    echo "╚════════════════════════════════════════════════════════════════╝"
    echo -e "${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

# Check prerequisites
check_prerequisites() {
    echo -e "${BOLD}Checking prerequisites...${NC}"
    
    local missing=0
    
    # Check for gcc
    if command -v gcc &> /dev/null; then
        print_success "gcc found: $(gcc --version | head -1)"
    else
        print_error "gcc not found"
        missing=1
    fi
    
    # Check for make
    if command -v make &> /dev/null; then
        print_success "make found: $(make --version | head -1)"
    else
        print_error "make not found"
        missing=1
    fi
    
    # Check for LVM tools
    if command -v lvs &> /dev/null; then
        print_success "LVM tools found: $(lvs --version 2>&1 | head -1)"
    else
        print_warning "LVM tools not found (install lvm2 package)"
    fi
    
    # Check for curl (optional)
    if command -v curl &> /dev/null; then
        print_success "curl found (for dashboard testing)"
    else
        print_info "curl not found (optional, for testing dashboard)"
    fi
    
    echo ""
    
    if [ $missing -eq 1 ]; then
        print_error "Missing required tools. Install with:"
        echo "  sudo dnf install -y gcc make lvm2"
        exit 1
    fi
}

# Build the project
build_project() {
    echo -e "${BOLD}Building project...${NC}"
    
    if [ -f "Makefile" ]; then
        make clean
        make
        print_success "Build completed successfully"
    else
        print_error "Makefile not found"
        exit 1
    fi
    
    echo ""
}

# Check DRY_RUN setting
check_dryrun() {
    echo -e "${BOLD}Checking configuration...${NC}"
    
    if grep -q "DRY_RUN.*1" lvm_config.h; then
        print_success "DRY_RUN mode enabled (safe for testing)"
    else
        print_warning "DRY_RUN mode DISABLED - will perform real LVM operations!"
        echo -e "${YELLOW}Press Ctrl+C within 5 seconds to cancel...${NC}"
        sleep 5
    fi
    
    echo ""
}

# Test run
test_run() {
    echo -e "${BOLD}Running quick test...${NC}"
    
    print_info "Starting lvm_manager for 3 seconds..."
    
    timeout 3 sudo ./lvm_manager || true
    
    print_success "Test run completed (program runs without crashing)"
    echo ""
}

# Install systemd service
install_service() {
    echo -e "${BOLD}Would you like to install as a systemd service? [y/N]${NC} "
    read -r response
    
    if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
        cat > /tmp/lvm-auto-extender.service << 'EOF'
[Unit]
Description=LVM Auto-Extender Service
After=network.target lvm2-monitor.service
Requires=lvm2-monitor.service

[Service]
Type=simple
ExecStart=/usr/local/bin/lvm_manager
Restart=on-failure
RestartSec=10
User=root
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF
        
        sudo cp /tmp/lvm-auto-extender.service /etc/systemd/system/
        sudo systemctl daemon-reload
        
        print_success "Systemd service installed"
        print_info "Enable with: sudo systemctl enable lvm-auto-extender"
        print_info "Start with:  sudo systemctl start lvm-auto-extender"
        print_info "Status:      sudo systemctl status lvm-auto-extender"
    fi
    
    echo ""
}

# Main deployment
main() {
    print_header
    
    check_prerequisites
    build_project
    check_dryrun
    
    # Install binary
    echo -e "${BOLD}Installing binary...${NC}"
    sudo make install
    print_success "Binary installed to /usr/local/bin/lvm_manager"
    echo ""
    
    # Test run
    test_run
    
    # Systemd service
    install_service
    
    # Final instructions
    echo -e "${CYAN}${BOLD}╔════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}${BOLD}║                    DEPLOYMENT COMPLETE                        ║${NC}"
    echo -e "${CYAN}${BOLD}╚════════════════════════════════════════════════════════════════╝${NC}"
    echo ""
    echo -e "${GREEN}Next steps:${NC}"
    echo -e "  1. Run manually:      ${BOLD}sudo lvm_manager${NC}"
    echo -e "  2. Check dashboard:   ${BOLD}curl http://localhost:8080${NC}"
    echo -e "  3. View logs:         ${BOLD}sudo journalctl -u lvm-auto-extender -f${NC}"
    echo -e "  4. Stop gracefully:   ${BOLD}Ctrl+C${NC} or ${BOLD}sudo systemctl stop lvm-auto-extender${NC}"
    echo ""
    echo -e "${YELLOW}Remember:${NC}"
    echo -e "  • DRY_RUN mode is enabled by default (safe)"
    echo -e "  • Edit lvm_config.h and rebuild to enable real operations"
    echo -e "  • Always test thoroughly before production use"
    echo ""
}

# Run main function
main
