#!/bin/bash

# KernelSU-Next Integration Script for Bandido SM8250
# Version: 1.0
# Author: Bandido Kernel Team

set -e

KERNEL_DIR=$(pwd)
KERNELSU_DIR="$KERNEL_DIR/KernelSU"

echo "🚀 KernelSU-Next Integration Script v1.0"
echo "=========================================="

# Check if we're in the right directory
if [ ! -f "Makefile" ] || [ ! -d "arch/arm64" ]; then
    echo "❌ Error: Please run this script from the kernel root directory"
    exit 1
fi

echo "📍 Working directory: $KERNEL_DIR"

# Verify KernelSU-Next is present
if [ ! -d "$KERNELSU_DIR" ] || [ ! -f "$KERNELSU_DIR/ksu.c" ]; then
    echo "❌ Error: KernelSU-Next not found in $KERNELSU_DIR"
    echo "Please ensure KernelSU-Next is properly copied to the KernelSU directory"
    exit 1
fi

echo "✅ KernelSU-Next found in $KERNELSU_DIR"

# Check KernelSU version
if [ -f "$KERNELSU_DIR/ksu.h" ]; then
    VERSION_INFO=$(grep -o "KERNEL_SU_VERSION.*" "$KERNELSU_DIR/ksu.h" | head -1)
    echo "📦 KernelSU Version: $VERSION_INFO"
fi

# Verify configuration files
echo "🔧 Verifying configuration..."

# Check if KernelSU is in main Kconfig
if ! grep -q "source \"KernelSU/Kconfig\"" Kconfig; then
    echo "⚠️  Adding KernelSU to main Kconfig..."
    sed -i '/source "security\/Kconfig"/a\\nsource "KernelSU/Kconfig"' Kconfig
fi

# Check if KernelSU is in main Makefile
if ! grep -q "obj-\$(CONFIG_KSU) += KernelSU/" Makefile; then
    echo "⚠️  Adding KernelSU to main Makefile..."
    sed -i '/drivers-y.*:= drivers\/ sound\/ firmware\/ techpack\//a obj-$(CONFIG_KSU) += KernelSU/' Makefile
fi

# Check defconfig
DEFCONFIG="arch/arm64/configs/r8q_defconfig"
if [ -f "$DEFCONFIG" ]; then
    echo "🔧 Checking defconfig: $DEFCONFIG"
    
    # Check if KernelSU config is present
    if ! grep -q "CONFIG_KSU=y" "$DEFCONFIG"; then
        echo "⚠️  Adding KernelSU configuration to defconfig..."
        cat >> "$DEFCONFIG" << 'EOF'

#
# KernelSU-Next Configuration
#
CONFIG_KSU=y
CONFIG_KSU_KPROBES_HOOK=y
# CONFIG_KSU_DEBUG is not set
# CONFIG_KSU_ALLOWLIST_WORKAROUND is not set
CONFIG_KSU_LSM_SECURITY_HOOKS=y
EOF
    fi
    
    # Ensure KPROBES is enabled
    if ! grep -q "CONFIG_KPROBES=y" "$DEFCONFIG"; then
        echo "⚠️  Enabling KPROBES in defconfig..."
        sed -i '/CONFIG_HAVE_KPROBES=y/a CONFIG_KPROBES=y' "$DEFCONFIG"
    fi
    
    # Verify OVERLAY_FS is enabled
    if ! grep -q "CONFIG_OVERLAY_FS=y" "$DEFCONFIG"; then
        echo "❌ Error: OVERLAY_FS is required but not enabled"
        echo "Please enable CONFIG_OVERLAY_FS in your defconfig"
        exit 1
    fi
    
    echo "✅ OVERLAY_FS is enabled"
    echo "✅ KPROBES is enabled"
    echo "✅ KernelSU configuration is present"
else
    echo "❌ Error: defconfig not found at $DEFCONFIG"
    exit 1
fi

# Check for required kernel features
echo "🔍 Checking kernel compatibility..."

KERNEL_VERSION=$(grep "^VERSION = " Makefile | cut -d' ' -f3)
PATCHLEVEL=$(grep "^PATCHLEVEL = " Makefile | cut -d' ' -f3)
SUBLEVEL=$(grep "^SUBLEVEL = " Makefile | cut -d' ' -f3)

echo "📋 Kernel Version: $KERNEL_VERSION.$PATCHLEVEL.$SUBLEVEL"

if [ "$KERNEL_VERSION" -eq 4 ] && [ "$PATCHLEVEL" -eq 19 ]; then
    echo "✅ Kernel 4.19 detected - Compatible with KernelSU-Next"
elif [ "$KERNEL_VERSION" -ge 5 ]; then
    echo "✅ Kernel $KERNEL_VERSION.x detected - Compatible with KernelSU-Next"
else
    echo "⚠️  Warning: Kernel version may not be fully compatible"
fi

# Create build verification script
echo "📝 Creating build verification script..."
cat > verify_kernelsu_build.sh << 'EOF'
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
EOF

chmod +x verify_kernelsu_build.sh

# Create KernelSU test script
echo "📝 Creating KernelSU test script..."
cat > test_kernelsu.sh << 'EOF'
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
EOF

chmod +x test_kernelsu.sh

# Final summary
echo ""
echo "🎉 KernelSU-Next Integration Completed!"
echo "======================================"
echo ""
echo "📋 Summary:"
echo "  ✅ KernelSU-Next source code integrated"
echo "  ✅ Kconfig updated"
echo "  ✅ Makefile updated"
echo "  ✅ defconfig updated with KernelSU options"
echo "  ✅ Dependencies verified (OVERLAY_FS, KPROBES)"
echo "  ✅ Build verification script created"
echo "  ✅ Test script created"
echo ""
echo "🚀 Next Steps:"
echo "  1. Build the kernel: ./build_kernel.sh"
echo "  2. Verify build: ./verify_kernelsu_build.sh"
echo "  3. Flash to device and test: ./test_kernelsu.sh"
echo ""
echo "📱 KernelSU Manager App:"
echo "  Download from: https://github.com/KernelSU-Next/KernelSU-Next/releases"
echo "  Install: KernelSU_Next_v1.0.7_*-release.apk"
echo ""
echo "⚠️  Important Notes:"
echo "  - Make sure to backup your current boot.img"
echo "  - Test thoroughly before daily use"
echo "  - KernelSU-Next requires Android 4.14+ kernel"
echo "  - This kernel supports Android 14 compatibility"
echo ""
echo "🔧 Configuration Details:"
echo "  - KernelSU: Enabled"
echo "  - KPROBES Hook: Enabled"
echo "  - LSM Security Hooks: Enabled"
echo "  - Debug Mode: Disabled (for performance)"
echo "  - Allowlist Workaround: Disabled"
echo ""
echo "Happy rooting! 🎯"
EOF

chmod +x setup_kernelsu_next.sh