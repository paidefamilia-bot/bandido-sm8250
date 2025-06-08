# 📚 Bandido SM8250 Kernel - Help & Documentation

## 🚀 Quick Start Guide

### **Installation via TWRP (Recommended)**
1. Download the latest kernel ZIP from [Releases](https://github.com/paidefamilia-bot/bandido-sm8250/releases)
2. Boot into TWRP recovery
3. Flash the kernel ZIP file
4. Reboot system

### **Manual Installation**
```bash
# Flash kernel image
fastboot flash boot bandido-kernel.img

# Flash dtbo (if included)
fastboot flash dtbo bandido-dtbo.img

# Reboot
fastboot reboot
```

---

## 🔧 AI System Configuration

### **Basic AI Controls**
```bash
# Enable/Disable AI Scheduler
echo 1 > /sys/kernel/ai_scheduler/enabled  # Enable
echo 0 > /sys/kernel/ai_scheduler/enabled  # Disable

# Set Performance Profile
echo performance > /sys/kernel/ai_scheduler/profile    # Gaming/High Performance
echo balanced > /sys/kernel/ai_scheduler/profile       # Balanced (Default)
echo power_save > /sys/kernel/ai_scheduler/profile     # Battery Saving
echo adaptive > /sys/kernel/ai_scheduler/profile       # AI Adaptive
```

### **Advanced AI Settings**
```bash
# Learning Rate (0.0001 - 0.01)
echo 0.001 > /sys/kernel/ai_scheduler/learning_rate

# Prediction Window (100-5000ms)
echo 1000 > /sys/kernel/ai_scheduler/prediction_window_ms

# GPU Acceleration
echo 1 > /sys/kernel/ai_scheduler/gpu_acceleration

# Neural Network Layers (3-10)
echo 5 > /sys/kernel/ai_scheduler/nn_layers

# Training Iterations (100-10000)
echo 1000 > /sys/kernel/ai_scheduler/training_iterations
```

---

## 🎮 Gaming Optimization

### **Gaming Mode Setup**
```bash
# Enable Gaming Profile
echo performance > /sys/kernel/ai_scheduler/profile

# Boost GPU Frequency
echo 1 > /sys/kernel/ai_scheduler/gpu_boost

# Disable Power Saving
echo 0 > /sys/kernel/ai_scheduler/power_save_mode

# Set CPU Governor to Performance
echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
```

### **Game-Specific Optimizations**
```bash
# PUBG Mobile / Call of Duty Mobile
echo gaming_fps > /sys/kernel/ai_scheduler/game_mode

# Genshin Impact / Heavy 3D Games
echo gaming_3d > /sys/kernel/ai_scheduler/game_mode

# Emulators (PSP, PS2, etc.)
echo gaming_emulator > /sys/kernel/ai_scheduler/game_mode
```

---

## 🔋 Battery Optimization

### **Battery Saving Mode**
```bash
# Enable Power Save Profile
echo power_save > /sys/kernel/ai_scheduler/profile

# Enable Aggressive Power Saving
echo 1 > /sys/kernel/ai_scheduler/aggressive_power_save

# Reduce Background Activity
echo 1 > /sys/kernel/ai_scheduler/background_limit

# Enable Thermal Throttling
echo 1 > /sys/kernel/ai_scheduler/thermal_throttle
```

### **Battery Prediction**
```bash
# View Battery Prediction
cat /proc/ai_scheduler/battery_prediction

# Battery Usage Patterns
cat /proc/ai_scheduler/usage_patterns

# Charging Optimization
echo 1 > /sys/kernel/ai_scheduler/smart_charging
```

---

## 🌡️ Thermal Management

### **Thermal Controls**
```bash
# View Thermal Status
cat /proc/ai_scheduler/thermal_status

# Set Thermal Profile
echo conservative > /sys/kernel/ai_scheduler/thermal_profile  # Conservative
echo balanced > /sys/kernel/ai_scheduler/thermal_profile      # Balanced
echo aggressive > /sys/kernel/ai_scheduler/thermal_profile    # Aggressive

# Emergency Thermal Protection
echo 1 > /sys/kernel/ai_scheduler/emergency_thermal
```

### **Temperature Monitoring**
```bash
# CPU Temperature
cat /sys/class/thermal/thermal_zone0/temp

# GPU Temperature
cat /sys/class/thermal/thermal_zone1/temp

# Battery Temperature
cat /sys/class/power_supply/battery/temp
```

---

## 📊 Performance Monitoring

### **Real-time Statistics**
```bash
# AI System Status
cat /proc/ai_scheduler/status

# Performance Metrics
cat /proc/ai_scheduler/performance

# Learning Statistics
cat /proc/ai_scheduler/learning_stats

# GPU Acceleration Status
cat /proc/ai_scheduler/gpu_stats
```

### **Continuous Monitoring**
```bash
# Watch AI Performance (updates every second)
watch -n 1 cat /proc/ai_scheduler/performance

# Monitor Predictions
watch -n 2 cat /proc/ai_scheduler/predictions

# Track Learning Progress
watch -n 5 cat /proc/ai_scheduler/learning_progress
```

---

## 🛠️ Troubleshooting

### **Common Issues**

#### **AI System Not Working**
```bash
# Check if AI is enabled
cat /sys/kernel/ai_scheduler/enabled

# Restart AI system
echo 0 > /sys/kernel/ai_scheduler/enabled
echo 1 > /sys/kernel/ai_scheduler/enabled

# Reset AI learning data
echo 1 > /sys/kernel/ai_scheduler/reset_learning
```

#### **Poor Gaming Performance**
```bash
# Force performance mode
echo performance > /sys/kernel/ai_scheduler/profile

# Check GPU acceleration
cat /sys/kernel/ai_scheduler/gpu_acceleration

# Verify thermal throttling
cat /proc/ai_scheduler/thermal_status
```

#### **Battery Drain Issues**
```bash
# Enable power save mode
echo power_save > /sys/kernel/ai_scheduler/profile

# Check background processes
cat /proc/ai_scheduler/background_activity

# Review power consumption
cat /proc/ai_scheduler/power_consumption
```

### **Reset to Defaults**
```bash
# Reset all AI settings
echo 1 > /sys/kernel/ai_scheduler/reset_all

# Restore default profile
echo balanced > /sys/kernel/ai_scheduler/profile

# Clear learning data
echo 1 > /sys/kernel/ai_scheduler/clear_learning_data
```

---

## 🔍 Advanced Features

### **Custom AI Training**
```bash
# Start custom training session
echo 1 > /sys/kernel/ai_scheduler/start_training

# Set training data source
echo user_patterns > /sys/kernel/ai_scheduler/training_source

# Monitor training progress
cat /proc/ai_scheduler/training_progress
```

### **Pattern Recognition**
```bash
# View detected patterns
cat /proc/ai_scheduler/detected_patterns

# Force pattern learning
echo 1 > /sys/kernel/ai_scheduler/force_pattern_learning

# Export patterns
cat /proc/ai_scheduler/export_patterns > /sdcard/ai_patterns.txt
```

### **Neural Network Tuning**
```bash
# Adjust network architecture
echo 7 > /sys/kernel/ai_scheduler/hidden_layers
echo 128 > /sys/kernel/ai_scheduler/neurons_per_layer

# Set activation function
echo relu > /sys/kernel/ai_scheduler/activation_function

# Configure optimizer
echo adam > /sys/kernel/ai_scheduler/optimizer
```

---

## 📱 Device-Specific Settings

### **OnePlus 8/8 Pro**
```bash
# Optimize for OnePlus
echo oneplus > /sys/kernel/ai_scheduler/device_profile

# Enable OnePlus-specific features
echo 1 > /sys/kernel/ai_scheduler/oneplus_optimizations
```

### **Samsung Galaxy S20**
```bash
# Optimize for Samsung
echo samsung > /sys/kernel/ai_scheduler/device_profile

# Enable Samsung-specific features
echo 1 > /sys/kernel/ai_scheduler/samsung_optimizations
```

### **Xiaomi Mi 10**
```bash
# Optimize for Xiaomi
echo xiaomi > /sys/kernel/ai_scheduler/device_profile

# Enable MIUI optimizations
echo 1 > /sys/kernel/ai_scheduler/miui_optimizations
```

---

## 🆘 Emergency Commands

### **Safe Mode**
```bash
# Enter safe mode (disables AI)
echo 1 > /sys/kernel/ai_scheduler/safe_mode

# Emergency performance reset
echo 1 > /sys/kernel/ai_scheduler/emergency_reset

# Force thermal protection
echo 1 > /sys/kernel/ai_scheduler/force_thermal_protection
```

### **Recovery Commands**
```bash
# If system becomes unresponsive
echo 1 > /proc/sysrq-trigger  # Enable SysRq
echo b > /proc/sysrq-trigger  # Force reboot

# Reset AI to factory defaults
echo factory_reset > /sys/kernel/ai_scheduler/reset_mode
```

---

## 📞 Getting Help

### **Log Collection**
```bash
# Collect AI system logs
dmesg | grep ai_scheduler > /sdcard/ai_logs.txt

# Collect performance data
cat /proc/ai_scheduler/debug_info > /sdcard/ai_debug.txt

# System information
cat /proc/version > /sdcard/kernel_info.txt
```

### **Support Channels**
- **GitHub Issues**: Report bugs and request features
- **Telegram**: @BandidoKernel (community support)
- **XDA Thread**: Detailed discussions and troubleshooting
- **Discord**: Real-time community help

---

## 🔄 Updates

### **Checking for Updates**
```bash
# Check kernel version
cat /proc/version

# Check AI system version
cat /sys/kernel/ai_scheduler/version

# Check for available updates
cat /sys/kernel/ai_scheduler/update_available
```

### **Update Process**
1. Download latest kernel ZIP from releases
2. Boot into TWRP
3. Flash new kernel ZIP
4. Reboot and verify functionality

---

**Need more help? Check our [Wiki](https://github.com/paidefamilia-bot/bandido-sm8250/wiki) or join our community!**