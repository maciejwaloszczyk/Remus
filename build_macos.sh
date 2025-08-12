#!/bin/bash
#
# Rufus macOS Fork - Build Script
# Automates building, testing, and packaging of Rufus macOS
#

set -e  # Exit on any error

# Script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$SCRIPT_DIR"

# Load configuration
if [ -f "$PROJECT_DIR/config.conf" ]; then
    source "$PROJECT_DIR/config.conf"
else
    echo "Warning: config.conf not found, using defaults"
    PROJECT_NAME="Rufus macOS"
    VERSION="4.10-macOS"
fi

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Print banner
print_banner() {
    echo "================================================================"
    echo "  $PROJECT_NAME Build Script"
    echo "  Version: $VERSION"
    echo "  Platform: macOS"
    echo "================================================================"
}

# Check prerequisites
check_prerequisites() {
    log_info "Checking prerequisites..."
    
    # Check if we're on macOS
    if [[ "$OSTYPE" != "darwin"* ]]; then
        log_error "This script is for macOS only"
        exit 1
    fi
    
    # Check for Xcode command line tools
    if ! command -v gcc &> /dev/null; then
        log_error "Xcode command line tools not found"
        log_info "Install with: xcode-select --install"
        exit 1
    fi
    
    # Check for required frameworks
    if [ ! -d "/System/Library/Frameworks/IOKit.framework" ]; then
        log_error "IOKit framework not found"
        exit 1
    fi
    
    if [ ! -d "/System/Library/Frameworks/DiskArbitration.framework" ]; then
        log_error "DiskArbitration framework not found"
        exit 1
    fi
    
    log_success "Prerequisites check passed"
}

# Clean build artifacts
clean_build() {
    log_info "Cleaning build artifacts..."
    
    if [ -d "$PROJECT_DIR/src/macos/build" ]; then
        rm -rf "$PROJECT_DIR/src/macos/build"
        log_info "Removed build directory"
    fi
    
    if [ -f "$PROJECT_DIR/src/macos/rufus-macos" ]; then
        rm -f "$PROJECT_DIR/src/macos/rufus-macos"
        log_info "Removed executable"
    fi
    
    log_success "Clean completed"
}

# Build the application
build_application() {
    log_info "Building $PROJECT_NAME..."
    
    cd "$PROJECT_DIR/src/macos"
    
    if make; then
        log_success "Build completed successfully"
    else
        log_error "Build failed"
        exit 1
    fi
    
    # Verify executable was created
    if [ ! -f "rufus-macos" ]; then
        log_error "Executable not found after build"
        exit 1
    fi
    
    # Show file info
    log_info "Executable info:"
    ls -la rufus-macos
    file rufus-macos
}

# Run tests
run_tests() {
    log_info "Running tests..."
    
    cd "$PROJECT_DIR/src/macos"
    
    # Test help
    log_info "Testing help output..."
    if ./rufus-macos --help > /dev/null; then
        log_success "Help test passed"
    else
        log_warning "Help test failed"
    fi
    
    # Test device listing (may fail if no USB devices)
    log_info "Testing device listing..."
    ./rufus-macos --list || log_warning "Device listing test completed (may be normal if no USB devices)"
    
    log_success "Tests completed"
}

# Create distribution package
create_package() {
    log_info "Creating distribution package..."
    
    local dist_dir="$PROJECT_DIR/dist"
    local pkg_dir="$dist_dir/rufus-macos-$VERSION"
    
    # Create directories
    mkdir -p "$pkg_dir/bin"
    mkdir -p "$pkg_dir/docs"
    
    # Copy executable
    cp "$PROJECT_DIR/src/macos/rufus-macos" "$pkg_dir/bin/"
    
    # Copy documentation
    cp "$PROJECT_DIR/README_macOS.md" "$pkg_dir/docs/"
    cp "$PROJECT_DIR/LICENSE.txt" "$pkg_dir/docs/" 2>/dev/null || log_warning "LICENSE.txt not found"
    
    # Create README for package
    cat > "$pkg_dir/README.txt" << EOF
$PROJECT_NAME $VERSION

Installation:
1. Copy rufus-macos from bin/ to /usr/local/bin/ (requires sudo)
2. Make sure /usr/local/bin is in your PATH

Usage:
  rufus-macos -l              # List USB devices  
  rufus-macos -d disk2 -f FAT32  # Format device

For more information, see docs/README_macOS.md

WARNING: This software can erase data on your drives. Use with caution!
EOF
    
    # Create tarball
    cd "$dist_dir"
    tar -czf "rufus-macos-$VERSION.tar.gz" "rufus-macos-$VERSION"
    
    log_success "Package created: dist/rufus-macos-$VERSION.tar.gz"
    
    # Show package info
    ls -la "rufus-macos-$VERSION.tar.gz"
}

# Install the application
install_application() {
    log_info "Installing $PROJECT_NAME..."
    
    local install_dir="${INSTALL_BINDIR:-/usr/local/bin}"
    
    if [ ! -f "$PROJECT_DIR/src/macos/rufus-macos" ]; then
        log_error "Executable not found. Run build first."
        exit 1
    fi
    
    # Check if install directory exists
    if [ ! -d "$install_dir" ]; then
        log_info "Creating install directory: $install_dir"
        sudo mkdir -p "$install_dir"
    fi
    
    # Copy executable
    sudo cp "$PROJECT_DIR/src/macos/rufus-macos" "$install_dir/"
    sudo chmod +x "$install_dir/rufus-macos"
    
    log_success "Installed to $install_dir/rufus-macos"
    log_info "You can now run 'rufus-macos' from anywhere"
}

# Show usage
show_usage() {
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  build     - Build the application"
    echo "  clean     - Clean build artifacts"
    echo "  test      - Run tests"
    echo "  package   - Create distribution package"
    echo "  install   - Install to system"
    echo "  all       - Clean, build, test, and package"
    echo "  help      - Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build"
    echo "  $0 all"
    echo "  $0 clean build test"
}

# Main function
main() {
    print_banner
    
    if [ $# -eq 0 ]; then
        show_usage
        exit 1
    fi
    
    # Process commands
    for cmd in "$@"; do
        case $cmd in
            "prereq"|"prerequisites")
                check_prerequisites
                ;;
            "clean")
                clean_build
                ;;
            "build")
                check_prerequisites
                build_application
                ;;
            "test")
                run_tests
                ;;
            "package")
                create_package
                ;;
            "install")
                install_application
                ;;
            "all")
                check_prerequisites
                clean_build
                build_application
                run_tests
                create_package
                ;;
            "help"|"-h"|"--help")
                show_usage
                ;;
            *)
                log_error "Unknown command: $cmd"
                show_usage
                exit 1
                ;;
        esac
    done
    
    log_success "All operations completed successfully!"
}

# Run main function with all arguments
main "$@"
