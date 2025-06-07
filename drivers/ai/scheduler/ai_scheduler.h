/*
 * AI Scheduler for Bandido Kernel
 * Advanced AI-powered task scheduling system
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _AI_SCHEDULER_H
#define _AI_SCHEDULER_H

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/cpumask.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/ktime.h>

/* AI Scheduler Version */
#define AI_SCHEDULER_VERSION_MAJOR	1
#define AI_SCHEDULER_VERSION_MINOR	0
#define AI_SCHEDULER_VERSION_PATCH	0

/* Configuration Constants */
#define AI_MAX_FEATURES			32
#define AI_MAX_TASK_HISTORY		1024
#define AI_MAX_CPU_CLUSTERS		3
#define AI_LEARNING_WINDOW_MS		5000
#define AI_PREDICTION_WINDOW_MS		1000
#define AI_MODEL_UPDATE_INTERVAL_MS	10000

/* Task Classification Types */
enum ai_task_type {
	AI_TASK_UNKNOWN = 0,
	AI_TASK_INTERACTIVE,
	AI_TASK_BACKGROUND,
	AI_TASK_GAMING,
	AI_TASK_MULTIMEDIA,
	AI_TASK_COMPUTE_INTENSIVE,
	AI_TASK_IO_INTENSIVE,
	AI_TASK_NETWORK_INTENSIVE,
	AI_TASK_AI_ML_WORKLOAD,
	AI_TASK_SYSTEM_CRITICAL,
	AI_TASK_TYPE_MAX
};

/* Performance Levels */
enum ai_performance_level {
	AI_PERF_POWERSAVE = 0,
	AI_PERF_BALANCED,
	AI_PERF_PERFORMANCE,
	AI_PERF_GAMING,
	AI_PERF_MAX
};

/* Learning States */
enum ai_learning_state {
	AI_LEARNING_DISABLED = 0,
	AI_LEARNING_COLLECTING,
	AI_LEARNING_TRAINING,
	AI_LEARNING_ACTIVE,
	AI_LEARNING_ERROR
};

/* Task Features for ML */
struct ai_task_features {
	/* CPU Usage Patterns */
	u32 cpu_usage_avg;		/* Average CPU usage (0-100) */
	u32 cpu_usage_peak;		/* Peak CPU usage */
	u32 cpu_burst_frequency;	/* Frequency of CPU bursts */
	u32 cpu_idle_time;		/* Time spent idle */
	
	/* Memory Patterns */
	u32 memory_usage;		/* Current memory usage (KB) */
	u32 memory_peak;		/* Peak memory usage */
	u32 memory_allocation_rate;	/* Memory allocation frequency */
	u32 page_fault_rate;		/* Page faults per second */
	
	/* I/O Patterns */
	u32 io_read_rate;		/* Read operations per second */
	u32 io_write_rate;		/* Write operations per second */
	u32 io_wait_time;		/* Time waiting for I/O */
	u32 network_activity;		/* Network I/O activity */
	
	/* Scheduling Patterns */
	u32 context_switches;		/* Context switches per second */
	u32 voluntary_switches;		/* Voluntary context switches */
	u32 involuntary_switches;	/* Involuntary context switches */
	u32 wakeup_frequency;		/* Wakeups per second */
	
	/* Timing Patterns */
	u64 runtime_total;		/* Total runtime (ns) */
	u64 runtime_recent;		/* Recent runtime window */
	u64 sleep_time_avg;		/* Average sleep time */
	u64 response_time_avg;		/* Average response time */
	
	/* User Interaction */
	u32 user_interaction_score;	/* User interaction level */
	u32 foreground_time;		/* Time in foreground */
	u32 background_time;		/* Time in background */
	u32 touch_events;		/* Touch events handled */
	
	/* System Impact */
	u32 system_load_impact;		/* Impact on system load */
	u32 thermal_impact;		/* Impact on thermal state */
	u32 power_consumption;		/* Estimated power usage */
	u32 battery_drain_rate;		/* Battery drain contribution */
	
	/* Classification Results */
	enum ai_task_type predicted_type;
	u32 confidence_score;		/* Prediction confidence (0-100) */
	u32 priority_score;		/* Calculated priority */
	u32 performance_requirement;	/* Required performance level */
};

/* Historical Data Point */
struct ai_task_history_entry {
	ktime_t timestamp;
	struct ai_task_features features;
	u32 actual_performance;
	u32 user_satisfaction;		/* Inferred from behavior */
	u32 energy_efficiency;
	struct list_head list;
};

/* Per-Task AI Data */
struct ai_task_data {
	/* Task Identification */
	pid_t pid;
	pid_t tgid;
	char comm[TASK_COMM_LEN];
	u32 task_hash;			/* Hash for quick lookup */
	
	/* Current State */
	struct ai_task_features current_features;
	enum ai_task_type current_type;
	enum ai_performance_level required_perf;
	
	/* Historical Data */
	struct list_head history;
	u32 history_count;
	spinlock_t history_lock;
	
	/* Learning Data */
	u32 learning_samples;
	u32 prediction_accuracy;
	u64 last_update_time;
	
	/* Performance Tracking */
	u32 performance_score;
	u32 energy_score;
	u32 user_satisfaction_score;
	
	/* Scheduling Hints */
	cpumask_t preferred_cpus;
	u32 preferred_frequency;
	u32 memory_priority;
	u32 io_priority;
	
	/* Reference Counting */
	atomic_t refcount;
	struct rcu_head rcu;
};

/* CPU Cluster Information */
struct ai_cpu_cluster {
	cpumask_t cpus;
	u32 max_frequency;
	u32 min_frequency;
	u32 current_frequency;
	u32 power_efficiency;
	u32 performance_capability;
	enum ai_performance_level current_level;
};

/* Neural Network Weights (Simplified) */
struct ai_neural_weights {
	/* Input Layer Weights */
	s32 input_weights[AI_MAX_FEATURES][16];
	s32 input_bias[16];
	
	/* Hidden Layer Weights */
	s32 hidden_weights[16][8];
	s32 hidden_bias[8];
	
	/* Output Layer Weights */
	s32 output_weights[8][AI_TASK_TYPE_MAX];
	s32 output_bias[AI_TASK_TYPE_MAX];
	
	/* Learning Parameters */
	u32 learning_rate;
	u32 momentum;
	u64 training_iterations;
	u32 accuracy;
};

/* Main AI Scheduler Context */
struct ai_scheduler_context {
	/* Global State */
	enum ai_learning_state learning_state;
	atomic_t enabled;
	atomic_t learning_enabled;
	
	/* Task Management */
	struct hash_table *task_table;
	spinlock_t task_table_lock;
	u32 total_tasks;
	
	/* CPU Cluster Information */
	struct ai_cpu_cluster clusters[AI_MAX_CPU_CLUSTERS];
	u32 num_clusters;
	
	/* Neural Network */
	struct ai_neural_weights *model;
	struct mutex model_lock;
	
	/* Statistics */
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	atomic64_t learning_samples;
	atomic64_t model_updates;
	
	/* Performance Metrics */
	u64 total_energy_saved;
	u64 total_performance_gained;
	u32 average_accuracy;
	u32 system_responsiveness;
	
	/* Work Queues */
	struct workqueue_struct *learning_wq;
	struct workqueue_struct *prediction_wq;
	struct delayed_work learning_work;
	struct delayed_work model_update_work;
	
	/* Timers */
	struct timer_list prediction_timer;
	struct timer_list statistics_timer;
	
	/* Debugging */
	u32 debug_level;
	struct dentry *debugfs_root;
	
	/* Configuration */
	u32 learning_window_ms;
	u32 prediction_window_ms;
	u32 model_update_interval_ms;
	u32 max_history_entries;
	
	/* System Integration */
	struct notifier_block cpu_notifier;
	struct notifier_block thermal_notifier;
	struct notifier_block power_notifier;
};

/* Global AI Scheduler Instance */
extern struct ai_scheduler_context ai_sched_ctx;

/* Core Functions */
int ai_scheduler_init(void);
void ai_scheduler_exit(void);
int ai_scheduler_enable(void);
void ai_scheduler_disable(void);

/* Task Management */
struct ai_task_data *ai_get_task_data(struct task_struct *task);
void ai_put_task_data(struct ai_task_data *data);
int ai_register_task(struct task_struct *task);
void ai_unregister_task(struct task_struct *task);

/* Feature Extraction */
int ai_extract_task_features(struct task_struct *task, 
			     struct ai_task_features *features);
void ai_update_task_features(struct ai_task_data *data);

/* Prediction and Classification */
enum ai_task_type ai_predict_task_type(const struct ai_task_features *features);
u32 ai_calculate_priority(const struct ai_task_features *features);
enum ai_performance_level ai_recommend_performance_level(
	const struct ai_task_features *features);

/* Learning Functions */
int ai_start_learning(void);
void ai_stop_learning(void);
int ai_add_training_sample(struct ai_task_data *data);
int ai_update_model(void);

/* Scheduling Integration */
int ai_select_cpu(struct task_struct *task, int prev_cpu);
void ai_task_tick(struct task_struct *task);
void ai_enqueue_task(struct task_struct *task);
void ai_dequeue_task(struct task_struct *task);

/* Performance Optimization */
void ai_boost_task_performance(struct task_struct *task);
void ai_optimize_cpu_frequency(int cpu);
void ai_balance_system_load(void);

/* Utility Functions */
u32 ai_hash_task(struct task_struct *task);
bool ai_is_gaming_task(struct task_struct *task);
bool ai_is_interactive_task(struct task_struct *task);
u32 ai_calculate_confidence(const struct ai_task_features *features);

/* Debug and Statistics */
void ai_print_statistics(void);
void ai_dump_task_data(struct ai_task_data *data);
int ai_debugfs_init(void);
void ai_debugfs_cleanup(void);

/* Sysfs Interface */
int ai_sysfs_init(void);
void ai_sysfs_cleanup(void);

/* Proc Interface */
int ai_proc_init(void);
void ai_proc_cleanup(void);

/* Inline Helper Functions */
static inline bool ai_scheduler_enabled(void)
{
	return atomic_read(&ai_sched_ctx.enabled);
}

static inline bool ai_learning_enabled(void)
{
	return atomic_read(&ai_sched_ctx.learning_enabled);
}

static inline u64 ai_get_accuracy_percentage(void)
{
	u64 total = atomic64_read(&ai_sched_ctx.predictions_made);
	u64 correct = atomic64_read(&ai_sched_ctx.predictions_correct);
	
	if (total == 0)
		return 0;
	
	return (correct * 100) / total;
}

static inline void ai_inc_prediction_stats(bool correct)
{
	atomic64_inc(&ai_sched_ctx.predictions_made);
	if (correct)
		atomic64_inc(&ai_sched_ctx.predictions_correct);
}

/* Debug Macros */
#define AI_DEBUG_NONE		0
#define AI_DEBUG_ERROR		1
#define AI_DEBUG_WARN		2
#define AI_DEBUG_INFO		3
#define AI_DEBUG_VERBOSE	4

#define ai_debug(level, fmt, args...) \
	do { \
		if (ai_sched_ctx.debug_level >= level) \
			pr_info("AI_SCHED: " fmt, ##args); \
	} while (0)

#define ai_error(fmt, args...) \
	ai_debug(AI_DEBUG_ERROR, "ERROR: " fmt, ##args)

#define ai_warn(fmt, args...) \
	ai_debug(AI_DEBUG_WARN, "WARN: " fmt, ##args)

#define ai_info(fmt, args...) \
	ai_debug(AI_DEBUG_INFO, "INFO: " fmt, ##args)

#define ai_verbose(fmt, args...) \
	ai_debug(AI_DEBUG_VERBOSE, "VERBOSE: " fmt, ##args)

#endif /* _AI_SCHEDULER_H */