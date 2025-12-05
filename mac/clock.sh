#!/bin/bash
# ESP8266 Clock - Interactive CLI Tool

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color
BOLD='\033[1m'

# Check if platformio is installed
check_pio() {
    if ! command -v pio &> /dev/null; then
        echo -e "${RED}Error: PlatformIO CLI (pio) is not installed or not in PATH${NC}"
        echo "Please install it: https://platformio.org/install/cli"
        exit 1
    fi
}

# Display header
show_header() {
    clear
    echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
    echo -e "${CYAN}║${NC}   ${BOLD}ESP8266 Clock - PlatformIO CLI${NC}   ${CYAN}║${NC}"
    echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"
    echo ""
}

# Display menu
show_menu() {
    local selected=$1

    show_header

    echo -e "${YELLOW}Use arrow keys (↑/↓) to navigate, Enter to select:${NC}"
    echo ""

    if [ $selected -eq 0 ]; then
        echo -e "${GREEN}► [1] Build${NC}"
    else
        echo -e "  [1] Build"
    fi

    if [ $selected -eq 1 ]; then
        echo -e "${GREEN}► [2] Upload${NC}"
    else
        echo -e "  [2] Upload"
    fi

    if [ $selected -eq 2 ]; then
        echo -e "${GREEN}► [3] Monitor${NC}"
    else
        echo -e "  [3] Monitor"
    fi

    if [ $selected -eq 3 ]; then
        echo -e "${GREEN}► [4] Build + Upload + Monitor${NC}"
    else
        echo -e "  [4] Build + Upload + Monitor"
    fi

    if [ $selected -eq 4 ]; then
        echo -e "${GREEN}► [5] Clean${NC}"
    else
        echo -e "  [5] Clean"
    fi

    if [ $selected -eq 5 ]; then
        echo -e "${GREEN}► [6] Device Info${NC}"
    else
        echo -e "  [6] Device Info"
    fi

    if [ $selected -eq 6 ]; then
        echo -e "${GREEN}► [7] Erase Flash${NC}"
    else
        echo -e "  [7] Erase Flash"
    fi

    if [ $selected -eq 7 ]; then
        echo -e "${GREEN}► [8] Exit${NC}"
    else
        echo -e "  [8] Exit"
    fi

    echo ""
}

# Execute build
do_build() {
    show_header
    echo -e "${BLUE}Building project...${NC}"
    echo ""
    pio run

    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ Build completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Build failed!${NC}"
    fi

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Execute upload
do_upload() {
    show_header
    echo -e "${BLUE}Uploading firmware...${NC}"
    echo ""
    pio run --target upload

    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ Upload completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Upload failed!${NC}"
    fi

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Execute monitor
do_monitor() {
    show_header
    echo -e "${BLUE}Starting serial monitor (Ctrl+C to exit)...${NC}"
    echo -e "Baud rate: ${CYAN}115200${NC}"
    echo ""
    sleep 1
    pio device monitor --baud 115200

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Execute build + upload + monitor
do_run() {
    show_header

    echo -e "${BLUE}[1/3] Building project...${NC}"
    echo ""
    pio run
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "${RED}✗ Build failed! Aborting.${NC}"
        echo ""
        echo -e "${YELLOW}Press Enter to continue...${NC}"
        read
        return
    fi

    echo ""
    echo -e "${GREEN}✓ Build successful${NC}"
    echo ""
    echo -e "${BLUE}[2/3] Uploading firmware...${NC}"
    echo ""
    pio run --target upload
    if [ $? -ne 0 ]; then
        echo ""
        echo -e "${RED}✗ Upload failed! Aborting.${NC}"
        echo ""
        echo -e "${YELLOW}Press Enter to continue...${NC}"
        read
        return
    fi

    echo ""
    echo -e "${GREEN}✓ Upload successful${NC}"
    echo ""
    echo -e "${BLUE}[3/3] Starting serial monitor (Ctrl+C to exit)...${NC}"
    echo -e "Baud rate: ${CYAN}115200${NC}"
    echo ""
    sleep 2
    pio device monitor --baud 115200

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Execute clean
do_clean() {
    show_header
    echo -e "${BLUE}Cleaning build files...${NC}"
    echo ""
    pio run --target clean

    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ Clean completed successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Clean failed!${NC}"
    fi

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Show device info
do_device_info() {
    show_header
    echo -e "${BLUE}Connected devices:${NC}"
    echo ""
    pio device list

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Erase flash
do_erase_flash() {
    show_header
    echo -e "${RED}${BOLD}WARNING: This will completely erase the flash memory!${NC}"
    echo -e "${YELLOW}All data on the device will be lost.${NC}"
    echo ""
    echo -e "${YELLOW}Are you sure you want to continue? (y/N): ${NC}"
    read -r confirm

    if [[ ! $confirm =~ ^[Yy]$ ]]; then
        echo ""
        echo -e "${BLUE}Operation cancelled.${NC}"
        echo ""
        echo -e "${YELLOW}Press Enter to continue...${NC}"
        read
        return
    fi

    echo ""
    echo -e "${BLUE}Erasing flash memory...${NC}"
    echo ""
    pio run --target erase

    if [ $? -eq 0 ]; then
        echo ""
        echo -e "${GREEN}✓ Flash erased successfully!${NC}"
    else
        echo ""
        echo -e "${RED}✗ Flash erase failed!${NC}"
    fi

    echo ""
    echo -e "${YELLOW}Press Enter to continue...${NC}"
    read
}

# Main menu loop
main() {
    check_pio

    local selected=0
    local max_items=7

    while true; do
        show_menu $selected

        # Read single character
        read -rsn1 input

        # Handle arrow keys (escape sequences)
        if [[ $input == $'\x1b' ]]; then
            read -rsn2 input
            case $input in
                '[A') # Up arrow
                    ((selected--))
                    if [ $selected -lt 0 ]; then
                        selected=$max_items
                    fi
                    ;;
                '[B') # Down arrow
                    ((selected++))
                    if [ $selected -gt $max_items ]; then
                        selected=0
                    fi
                    ;;
            esac
        elif [[ $input == "" ]]; then
            # Enter key
            case $selected in
                0) do_build ;;
                1) do_upload ;;
                2) do_monitor ;;
                3) do_run ;;
                4) do_clean ;;
                5) do_device_info ;;
                6) do_erase_flash ;;
                7)
                    show_header
                    echo -e "${GREEN}Goodbye!${NC}"
                    echo ""
                    exit 0
                    ;;
            esac
        elif [[ $input =~ ^[1-8]$ ]]; then
            # Number key pressed
            case $input in
                1) do_build ;;
                2) do_upload ;;
                3) do_monitor ;;
                4) do_run ;;
                5) do_clean ;;
                6) do_device_info ;;
                7) do_erase_flash ;;
                8)
                    show_header
                    echo -e "${GREEN}Goodbye!${NC}"
                    echo ""
                    exit 0
                    ;;
            esac
        fi
    done
}

# Run main function
main
