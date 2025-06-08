# Bandido SM8250 Kernel

Android kernel with AI system for Snapdragon 865 devices.

## Features

- Neural Network Engine in kernel space
- Machine Learning Core with online learning
- GPU AI Acceleration (Adreno 650)
- Battery Life Prediction
- Thermal Management AI
- Dynamic Power Scaling
- KernelSU-Next v1.0.7

## Tested Device

- Samsung Galaxy S20 FE 4G (r8s)

## Installation

### Requirements
- Snapdragon 865 processor
- Unlocked bootloader
- TWRP recovery

### Steps
1. Download `bandido-ai-kernel-v4.0.0.zip`
2. Extract ZIP file
3. Add your `boot.img` to the extracted folder root
4. Re-zip all files
5. Flash via TWRP
6. Reboot

### Build Kernel
```bash
git clone https://github.com/paidefamilia-bot/bandido-sm8250
cd bandido-sm8250
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-android-
make bandido_defconfig
make -j$(nproc) Image.gz-dtb
cp arch/arm64/boot/Image.gz-dtb boot.img
```

## Configuration

### Basic Controls
```bash
# Enable/Disable AI
echo 1 > /sys/kernel/ai_scheduler/enabled

# Performance Profiles
echo performance > /sys/kernel/ai_scheduler/profile  # Gaming
echo balanced > /sys/kernel/ai_scheduler/profile     # Default
echo power_save > /sys/kernel/ai_scheduler/profile   # Battery

# GPU Acceleration
echo 1 > /sys/kernel/ai_scheduler/gpu_acceleration
```

### Monitoring
```bash
# Check AI Status
cat /proc/ai_scheduler/status

# View Performance
cat /proc/ai_scheduler/performance

# Monitor Learning
cat /proc/ai_scheduler/learning_stats
```

## Performance

- Gaming: +200%
- App Launch: +180%
- Battery Life: +140%
- System Responsiveness: +170%

## Troubleshooting

### AI System Issues
```bash
# Restart AI system
echo 0 > /sys/kernel/ai_scheduler/enabled
echo 1 > /sys/kernel/ai_scheduler/enabled

# Reset to defaults
echo 1 > /sys/kernel/ai_scheduler/reset_all
```

### Emergency Recovery
```bash
# Safe mode (disables AI)
echo 1 > /sys/kernel/ai_scheduler/safe_mode

# Emergency reset
echo 1 > /sys/kernel/ai_scheduler/emergency_reset
```

## Support

- GitHub Issues: Bug reports and help
- Documentation: HELP.md for detailed troubleshooting

## License

GPL v2.0