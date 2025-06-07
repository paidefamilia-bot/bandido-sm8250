#!/bin/bash

echo "🧪 KernelSU-Next Test Script"
echo "============================"

# Check if running on device
if [ ! -f "/proc/version" ]; then
    echo "❌ This script should be run on the target device"
    exit 1
fi

# Check kernel version
echo "📋 Kernel: $(cat /proc/version)"

# Check if KernelSU is loaded
if [ -d "/proc/kernelsu" ]; then
    echo "✅ KernelSU proc interface found"
    
    # Check KernelSU version
    if [ -f "/proc/kernelsu/version" ]; then
        echo "📦 KernelSU Version: $(cat /proc/kernelsu/version)"
    fi
    
    # Check if manager is working
    if command -v su >/dev/null 2>&1; then
        echo "✅ su command available"
    else
        echo "⚠️  su command not found"
    fi
    
else
    echo "❌ KernelSU proc interface not found"
    echo "KernelSU may not be properly loaded"
fi

# Check SELinux status
echo "🔒 SELinux Status: $(getenforce 2>/dev/null || echo 'Unknown')"

# Check for KernelSU manager app
if pm list packages | grep -q "me.weishu.kernelsu"; then
    echo "✅ KernelSU Manager app installed"
else
    echo "⚠️  KernelSU Manager app not found"
fi

echo "🎉 Test completed!"
