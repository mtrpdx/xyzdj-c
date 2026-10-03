#!/usr/bin/env sh

# Define your board's IP address (Check your router or run 'ifconfig' on the board)
BOARD_IP="10.0.0.95"
USER="root"

# Compile your C code using the Yocto Cross-Compiler
echo "Compiling xyzdj..."
make

# Check if compilation succeeded before trying to copy
if [ $? -eq 0 ]; then
    echo "Pushing binary to ADSP-SC589..."
    # Copy the binary to the /usr/bin folder on the board
    scp xyzdj ${USER}@${BOARD_IP}:/usr/bin/

    echo "Done! You can now run 'xyzdj' on the board."
else
    echo "Compilation failed. Aborting transfer."
fi
