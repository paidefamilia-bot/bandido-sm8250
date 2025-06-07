#!/bin/bash

echo "🔍 Verifying KernelSU-Next build..."

# Check if KernelSU objects are built
if [ -f "out/KernelSU/ksu.o" ]; then
    echo "✅ KernelSU core object built successfully"
else
    echo "❌ KernelSU core object not found"
    exit 1
fi

# Check if KernelSU is included in vmlinux
if [ -f "out/vmlinux" ]; then
    if objdump -t out/vmlinux | grep -q "ksu_"; then
        echo "✅ KernelSU symbols found in vmlinux"
    else
        echo "⚠️  KernelSU symbols not found in vmlinux"
    fi
fi

# Check kernel image
if [ -f "out/arch/arm64/boot/Image.gz" ]; then
    echo "✅ Kernel image built successfully"
    echo "📦 Image size: $(du -h out/arch/arm64/boot/Image.gz | cut -f1)"
else
    echo "❌ Kernel image not found"
    exit 1
fi

echo "🎉 Build verification completed!"
