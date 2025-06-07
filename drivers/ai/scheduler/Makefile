# AI Scheduler Makefile
# Copyright (C) 2024 Bandido Kernel Team

obj-$(CONFIG_AI_SCHEDULER) += ai_scheduler_core.o

ai_scheduler_core-y := ai_scheduler_main.o \
		       ai_data_collection.o \
		       ai_features.o \
		       ai_learning.o \
		       ai_integration.o \
		       ai_proc.o \
		       ai_sysfs.o \
		       ai_debug.o

ai_scheduler_core-$(CONFIG_AI_SCHEDULER_DEBUG) += ai_debug.o
ai_scheduler_core-$(CONFIG_AI_SCHEDULER_STATS) += ai_statistics.o

# Compiler flags for optimization
ccflags-y += -O2 -fno-strict-aliasing
ccflags-$(CONFIG_AI_SCHEDULER_DEBUG) += -DDEBUG -g

# Enable specific optimizations for AI code
ccflags-y += -funroll-loops -ffast-math

# Disable some warnings for ML code
ccflags-y += -Wno-unused-function -Wno-sign-compare