/*
 * AI Scheduler Task Behavior Monitoring
 * Advanced behavioral pattern analysis and learning system
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/math64.h>

#include "ai_scheduler.h"

/* Behavior monitoring configuration */
#define AI_BEHAVIOR_WINDOW_SIZE		64	/* Number of behavior samples */
#define AI_BEHAVIOR_UPDATE_INTERVAL_MS	1000	/* Update every second */
#define AI_BEHAVIOR_PATTERN_THRESHOLD	5	/* Minimum samples for pattern */
#define AI_BEHAVIOR_MAX_PATTERNS	16	/* Maximum patterns per task */

/* Behavior pattern types */
enum ai_behavior_pattern_type {
	AI_PATTERN_CPU_BURST = 0,	/* CPU usage bursts */
	AI_PATTERN_MEMORY_GROWTH,	/* Memory usage growth */
	AI_PATTERN_IO_INTENSIVE,	/* I/O intensive periods */
	AI_PATTERN_INTERACTIVE,		/* User interaction periods */
	AI_PATTERN_IDLE,		/* Idle/sleep periods */
	AI_PATTERN_PERIODIC,		/* Periodic activity */
	AI_PATTERN_GAMING,		/* Gaming behavior */
	AI_PATTERN_BACKGROUND,		/* Background processing */
	AI_PATTERN_TYPE_MAX
};

/* Behavior sample */
struct ai_behavior_sample {
	ktime_t timestamp;
	u32 cpu_usage;		/* CPU usage percentage */
	u32 memory_usage;	/* Memory usage in KB */
	u32 io_activity;	/* I/O activity level */
	u32 user_interaction;	/* User interaction level */
	u32 wakeups;		/* Number of wakeups */
	u32 context_switches;	/* Context switches */
	u8 cpu_state;		/* CPU state (running, sleeping, etc.) */
	u8 priority;		/* Task priority */
};

/* Behavior pattern */
struct ai_behavior_pattern {
	enum ai_behavior_pattern_type type;
	u32 frequency;		/* How often this pattern occurs */
	u32 duration_avg;	/* Average duration in ms */
	u32 intensity_avg;	/* Average intensity (0-100) */
	u32 confidence;		/* Pattern confidence (0-100) */
	u64 last_occurrence;	/* Last time this pattern was seen */
	u32 prediction_accuracy; /* How accurate predictions are */
	struct list_head list;
};

/* Task behavior context */
struct ai_task_behavior {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	
	/* Behavior samples ring buffer */
	struct ai_behavior_sample samples[AI_BEHAVIOR_WINDOW_SIZE];
	u32 sample_index;
	u32 sample_count;
	spinlock_t samples_lock;
	
	/* Detected patterns */
	struct list_head patterns;
	u32 pattern_count;
	spinlock_t patterns_lock;
	
	/* Current state */
	enum ai_behavior_pattern_type current_pattern;
	u32 pattern_start_time;
	u32 pattern_confidence;
	
	/* Statistics */
	u64 total_monitoring_time;
	u32 pattern_changes;
	u32 prediction_hits;
	u32 prediction_misses;
	
	/* Timing */
	u64 last_update;
	u64 last_pattern_change;
	
	struct list_head list;
};

/* Global behavior monitoring context */
struct ai_behavior_monitor_ctx {
	struct list_head task_behaviors;
	spinlock_t tasks_lock;
	u32 total_tasks;
	
	/* Monitoring thread */
	struct delayed_work monitor_work;
	struct workqueue_struct *monitor_wq;
	bool monitoring_active;
	
	/* Statistics */
	atomic64_t samples_collected;
	atomic64_t patterns_detected;
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	
	/* Configuration */
	u32 update_interval_ms;
	u32 pattern_threshold;
	bool learning_enabled;
};

static struct ai_behavior_monitor_ctx behavior_ctx;

/* Pattern type names for debugging */
static const char *pattern_names[] = {
	"CPU_BURST",
	"MEMORY_GROWTH", 
	"IO_INTENSIVE",
	"INTERACTIVE",
	"IDLE",
	"PERIODIC",
	"GAMING",
	"BACKGROUND"
};

/**
 * ai_alloc_task_behavior - Allocate task behavior context
 * @task: Target task
 * 
 * Returns: Allocated behavior context or NULL
 */
static struct ai_task_behavior *ai_alloc_task_behavior(struct task_struct *task)
{
	struct ai_task_behavior *behavior;
	
	behavior = kzalloc(sizeof(*behavior), GFP_KERNEL);
	if (!behavior)
		return NULL;
	
	behavior->pid = task->pid;
	strncpy(behavior->comm, task->comm, TASK_COMM_LEN - 1);
	behavior->comm[TASK_COMM_LEN - 1] = '\0';
	
	spin_lock_init(&behavior->samples_lock);
	spin_lock_init(&behavior->patterns_lock);
	INIT_LIST_HEAD(&behavior->patterns);
	
	behavior->current_pattern = AI_PATTERN_TYPE_MAX; /* Unknown */
	behavior->last_update = ktime_get_ns();
	
	ai_verbose("Allocated behavior context for %s[%d]", task->comm, task->pid);
	
	return behavior;
}

/**
 * ai_free_task_behavior - Free task behavior context
 * @behavior: Behavior context to free
 */
static void ai_free_task_behavior(struct ai_task_behavior *behavior)
{
	struct ai_behavior_pattern *pattern, *tmp;
	
	if (!behavior)
		return;
	
	/* Free patterns */
	spin_lock(&behavior->patterns_lock);
	list_for_each_entry_safe(pattern, tmp, &behavior->patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	spin_unlock(&behavior->patterns_lock);
	
	kfree(behavior);
}

/**
 * ai_collect_behavior_sample - Collect a behavior sample for a task
 * @task: Target task
 * @behavior: Task behavior context
 */
static void ai_collect_behavior_sample(struct task_struct *task,
				       struct ai_task_behavior *behavior)
{
	struct ai_behavior_sample *sample;
	struct ai_task_data *task_data;
	u32 index;
	
	task_data = ai_get_task_data(task);
	if (!task_data)
		return;
	
	spin_lock(&behavior->samples_lock);
	
	/* Get next sample slot */
	index = behavior->sample_index;
	sample = &behavior->samples[index];
	
	/* Fill sample data */
	sample->timestamp = ktime_get();
	sample->cpu_usage = task_data->current_features.cpu_usage_avg;
	sample->memory_usage = task_data->current_features.memory_usage;
	sample->io_activity = task_data->current_features.io_read_rate + 
			      task_data->current_features.io_write_rate;
	sample->user_interaction = task_data->current_features.user_interaction_score;
	sample->wakeups = task_data->current_features.wakeup_frequency;
	sample->context_switches = task_data->current_features.context_switches;
	sample->cpu_state = task->state;
	sample->priority = task->prio;
	
	/* Update ring buffer */
	behavior->sample_index = (index + 1) % AI_BEHAVIOR_WINDOW_SIZE;
	if (behavior->sample_count < AI_BEHAVIOR_WINDOW_SIZE)
		behavior->sample_count++;
	
	spin_unlock(&behavior->samples_lock);
	
	ai_put_task_data(task_data);
	atomic64_inc(&behavior_ctx.samples_collected);
	
	ai_verbose("Collected behavior sample for %s[%d]: cpu=%u%%, mem=%uKB, io=%u",
		   task->comm, task->pid, sample->cpu_usage, 
		   sample->memory_usage, sample->io_activity);
}

/**
 * ai_detect_cpu_burst_pattern - Detect CPU burst patterns
 * @behavior: Task behavior context
 * 
 * Returns: Confidence level (0-100) for CPU burst pattern
 */
static u32 ai_detect_cpu_burst_pattern(struct ai_task_behavior *behavior)
{
	u32 burst_count = 0;
	u32 high_cpu_samples = 0;
	u32 i, confidence = 0;
	
	spin_lock(&behavior->samples_lock);
	
	if (behavior->sample_count < AI_BEHAVIOR_PATTERN_THRESHOLD) {
		spin_unlock(&behavior->samples_lock);
		return 0;
	}
	
	/* Look for CPU usage spikes */
	for (i = 0; i < behavior->sample_count; i++) {
		struct ai_behavior_sample *sample = &behavior->samples[i];
		
		if (sample->cpu_usage > 70) {
			high_cpu_samples++;
			
			/* Check if this is a burst (high CPU followed by low CPU) */
			if (i > 0 && behavior->samples[i-1].cpu_usage < 30) {
				burst_count++;
			}
		}
	}
	
	spin_unlock(&behavior->samples_lock);
	
	/* Calculate confidence based on burst frequency */
	if (burst_count >= 3) {
		confidence = min(burst_count * 20, 100U);
	} else if (high_cpu_samples > behavior->sample_count / 2) {
		confidence = 60; /* Sustained high CPU */
	}
	
	return confidence;
}

/**
 * ai_detect_memory_growth_pattern - Detect memory growth patterns
 * @behavior: Task behavior context
 * 
 * Returns: Confidence level (0-100) for memory growth pattern
 */
static u32 ai_detect_memory_growth_pattern(struct ai_task_behavior *behavior)
{
	u32 growth_samples = 0;
	u32 prev_memory = 0;
	u32 i, confidence = 0;
	
	spin_lock(&behavior->samples_lock);
	
	if (behavior->sample_count < AI_BEHAVIOR_PATTERN_THRESHOLD) {
		spin_unlock(&behavior->samples_lock);
		return 0;
	}
	
	/* Look for consistent memory growth */
	for (i = 0; i < behavior->sample_count; i++) {
		struct ai_behavior_sample *sample = &behavior->samples[i];
		
		if (i > 0 && sample->memory_usage > prev_memory) {
			growth_samples++;
		}
		prev_memory = sample->memory_usage;
	}
	
	spin_unlock(&behavior->samples_lock);
	
	/* Calculate confidence based on growth consistency */
	if (growth_samples > behavior->sample_count * 3 / 4) {
		confidence = 90; /* Very consistent growth */
	} else if (growth_samples > behavior->sample_count / 2) {
		confidence = 60; /* Moderate growth */
	}
	
	return confidence;
}

/**
 * ai_detect_interactive_pattern - Detect interactive patterns
 * @behavior: Task behavior context
 * 
 * Returns: Confidence level (0-100) for interactive pattern
 */
static u32 ai_detect_interactive_pattern(struct ai_task_behavior *behavior)
{
	u32 interactive_samples = 0;
	u32 high_wakeup_samples = 0;
	u32 i, confidence = 0;
	
	spin_lock(&behavior->samples_lock);
	
	if (behavior->sample_count < AI_BEHAVIOR_PATTERN_THRESHOLD) {
		spin_unlock(&behavior->samples_lock);
		return 0;
	}
	
	/* Look for high user interaction and frequent wakeups */
	for (i = 0; i < behavior->sample_count; i++) {
		struct ai_behavior_sample *sample = &behavior->samples[i];
		
		if (sample->user_interaction > 60) {
			interactive_samples++;
		}
		
		if (sample->wakeups > 10) {
			high_wakeup_samples++;
		}
	}
	
	spin_unlock(&behavior->samples_lock);
	
	/* Calculate confidence */
	if (interactive_samples > behavior->sample_count * 2 / 3) {
		confidence = 80;
	} else if (high_wakeup_samples > behavior->sample_count / 2) {
		confidence = 60;
	}
	
	return confidence;
}

/**
 * ai_detect_gaming_pattern - Detect gaming patterns
 * @behavior: Task behavior context
 * 
 * Returns: Confidence level (0-100) for gaming pattern
 */
static u32 ai_detect_gaming_pattern(struct ai_task_behavior *behavior)
{
	u32 high_cpu_samples = 0;
	u32 high_interaction_samples = 0;
	u32 consistent_samples = 0;
	u32 i, confidence = 0;
	
	spin_lock(&behavior->samples_lock);
	
	if (behavior->sample_count < AI_BEHAVIOR_PATTERN_THRESHOLD) {
		spin_unlock(&behavior->samples_lock);
		return 0;
	}
	
	/* Look for sustained high CPU + high interaction */
	for (i = 0; i < behavior->sample_count; i++) {
		struct ai_behavior_sample *sample = &behavior->samples[i];
		
		if (sample->cpu_usage > 50) {
			high_cpu_samples++;
		}
		
		if (sample->user_interaction > 80) {
			high_interaction_samples++;
		}
		
		if (sample->cpu_usage > 50 && sample->user_interaction > 80) {
			consistent_samples++;
		}
	}
	
	spin_unlock(&behavior->samples_lock);
	
	/* Gaming requires both high CPU and high interaction */
	if (consistent_samples > behavior->sample_count * 2 / 3) {
		confidence = 95; /* Very likely gaming */
	} else if (high_cpu_samples > behavior->sample_count / 2 && 
		   high_interaction_samples > behavior->sample_count / 3) {
		confidence = 70; /* Likely gaming */
	}
	
	return confidence;
}

/**
 * ai_analyze_behavior_patterns - Analyze and detect behavior patterns
 * @behavior: Task behavior context
 */
static void ai_analyze_behavior_patterns(struct ai_task_behavior *behavior)
{
	enum ai_behavior_pattern_type detected_pattern = AI_PATTERN_TYPE_MAX;
	u32 max_confidence = 0;
	u32 confidence;
	
	/* Test different pattern types */
	confidence = ai_detect_cpu_burst_pattern(behavior);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_PATTERN_CPU_BURST;
	}
	
	confidence = ai_detect_memory_growth_pattern(behavior);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_PATTERN_MEMORY_GROWTH;
	}
	
	confidence = ai_detect_interactive_pattern(behavior);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_PATTERN_INTERACTIVE;
	}
	
	confidence = ai_detect_gaming_pattern(behavior);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_PATTERN_GAMING;
	}
	
	/* Update current pattern if confidence is high enough */
	if (max_confidence > 70 && detected_pattern != behavior->current_pattern) {
		enum ai_behavior_pattern_type old_pattern = behavior->current_pattern;
		
		behavior->current_pattern = detected_pattern;
		behavior->pattern_confidence = max_confidence;
		behavior->pattern_start_time = jiffies_to_msecs(jiffies);
		behavior->pattern_changes++;
		behavior->last_pattern_change = ktime_get_ns();
		
		atomic64_inc(&behavior_ctx.patterns_detected);
		
		ai_info("Pattern change for %s[%d]: %s -> %s (confidence: %u%%)",
			behavior->comm, behavior->pid,
			(old_pattern < AI_PATTERN_TYPE_MAX) ? pattern_names[old_pattern] : "UNKNOWN",
			pattern_names[detected_pattern], max_confidence);
	}
}

/**
 * ai_behavior_monitor_worker - Background behavior monitoring worker
 * @work: Work structure
 */
static void ai_behavior_monitor_worker(struct work_struct *work)
{
	struct ai_task_behavior *behavior, *tmp;
	struct task_struct *task;
	
	if (!behavior_ctx.monitoring_active)
		return;
	
	spin_lock(&behavior_ctx.tasks_lock);
	list_for_each_entry_safe(behavior, tmp, &behavior_ctx.task_behaviors, list) {
		spin_unlock(&behavior_ctx.tasks_lock);
		
		/* Find the task */
		rcu_read_lock();
		task = find_task_by_vpid(behavior->pid);
		if (task && !strcmp(task->comm, behavior->comm)) {
			get_task_struct(task);
			rcu_read_unlock();
			
			/* Collect behavior sample */
			ai_collect_behavior_sample(task, behavior);
			
			/* Analyze patterns */
			ai_analyze_behavior_patterns(behavior);
			
			behavior->last_update = ktime_get_ns();
			behavior->total_monitoring_time += behavior_ctx.update_interval_ms;
			
			put_task_struct(task);
		} else {
			rcu_read_unlock();
			
			/* Task no longer exists, remove behavior context */
			spin_lock(&behavior_ctx.tasks_lock);
			list_del(&behavior->list);
			behavior_ctx.total_tasks--;
			spin_unlock(&behavior_ctx.tasks_lock);
			
			ai_free_task_behavior(behavior);
			
			ai_verbose("Removed behavior context for dead task %s[%d]",
				   behavior->comm, behavior->pid);
		}
		
		spin_lock(&behavior_ctx.tasks_lock);
	}
	spin_unlock(&behavior_ctx.tasks_lock);
	
	/* Schedule next monitoring cycle */
	if (behavior_ctx.monitoring_active) {
		queue_delayed_work(behavior_ctx.monitor_wq,
				   &behavior_ctx.monitor_work,
				   msecs_to_jiffies(behavior_ctx.update_interval_ms));
	}
}

/**
 * ai_register_task_behavior - Register a task for behavior monitoring
 * @task: Task to monitor
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_register_task_behavior(struct task_struct *task)
{
	struct ai_task_behavior *behavior;
	
	if (!task || !behavior_ctx.monitoring_active)
		return -EINVAL;
	
	/* Check if already registered */
	spin_lock(&behavior_ctx.tasks_lock);
	list_for_each_entry(behavior, &behavior_ctx.task_behaviors, list) {
		if (behavior->pid == task->pid && 
		    !strcmp(behavior->comm, task->comm)) {
			spin_unlock(&behavior_ctx.tasks_lock);
			return 0; /* Already registered */
		}
	}
	spin_unlock(&behavior_ctx.tasks_lock);
	
	/* Allocate new behavior context */
	behavior = ai_alloc_task_behavior(task);
	if (!behavior)
		return -ENOMEM;
	
	/* Add to monitoring list */
	spin_lock(&behavior_ctx.tasks_lock);
	list_add(&behavior->list, &behavior_ctx.task_behaviors);
	behavior_ctx.total_tasks++;
	spin_unlock(&behavior_ctx.tasks_lock);
	
	ai_info("Registered task %s[%d] for behavior monitoring", 
		task->comm, task->pid);
	
	return 0;
}

/**
 * ai_get_task_behavior_pattern - Get current behavior pattern for a task
 * @task: Target task
 * @confidence: Output for pattern confidence
 * 
 * Returns: Current behavior pattern type
 */
enum ai_behavior_pattern_type ai_get_task_behavior_pattern(struct task_struct *task,
							   u32 *confidence)
{
	struct ai_task_behavior *behavior;
	enum ai_behavior_pattern_type pattern = AI_PATTERN_TYPE_MAX;
	
	if (!task)
		return AI_PATTERN_TYPE_MAX;
	
	spin_lock(&behavior_ctx.tasks_lock);
	list_for_each_entry(behavior, &behavior_ctx.task_behaviors, list) {
		if (behavior->pid == task->pid && 
		    !strcmp(behavior->comm, task->comm)) {
			pattern = behavior->current_pattern;
			if (confidence)
				*confidence = behavior->pattern_confidence;
			break;
		}
	}
	spin_unlock(&behavior_ctx.tasks_lock);
	
	return pattern;
}

/**
 * ai_behavior_monitor_init - Initialize behavior monitoring system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_behavior_monitor_init(void)
{
	/* Initialize context */
	INIT_LIST_HEAD(&behavior_ctx.task_behaviors);
	spin_lock_init(&behavior_ctx.tasks_lock);
	
	behavior_ctx.update_interval_ms = AI_BEHAVIOR_UPDATE_INTERVAL_MS;
	behavior_ctx.pattern_threshold = AI_BEHAVIOR_PATTERN_THRESHOLD;
	behavior_ctx.learning_enabled = true;
	
	atomic64_set(&behavior_ctx.samples_collected, 0);
	atomic64_set(&behavior_ctx.patterns_detected, 0);
	atomic64_set(&behavior_ctx.predictions_made, 0);
	atomic64_set(&behavior_ctx.predictions_correct, 0);
	
	/* Create work queue */
	behavior_ctx.monitor_wq = create_singlethread_workqueue("ai_behavior");
	if (!behavior_ctx.monitor_wq) {
		ai_error("Failed to create behavior monitoring work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&behavior_ctx.monitor_work, ai_behavior_monitor_worker);
	
	/* Start monitoring */
	behavior_ctx.monitoring_active = true;
	queue_delayed_work(behavior_ctx.monitor_wq,
			   &behavior_ctx.monitor_work,
			   msecs_to_jiffies(behavior_ctx.update_interval_ms));
	
	ai_info("Behavior monitoring system initialized");
	
	return 0;
}

/**
 * ai_behavior_monitor_exit - Cleanup behavior monitoring system
 */
void ai_behavior_monitor_exit(void)
{
	struct ai_task_behavior *behavior, *tmp;
	
	/* Stop monitoring */
	behavior_ctx.monitoring_active = false;
	
	if (behavior_ctx.monitor_wq) {
		cancel_delayed_work_sync(&behavior_ctx.monitor_work);
		destroy_workqueue(behavior_ctx.monitor_wq);
		behavior_ctx.monitor_wq = NULL;
	}
	
	/* Free all behavior contexts */
	spin_lock(&behavior_ctx.tasks_lock);
	list_for_each_entry_safe(behavior, tmp, &behavior_ctx.task_behaviors, list) {
		list_del(&behavior->list);
		ai_free_task_behavior(behavior);
	}
	behavior_ctx.total_tasks = 0;
	spin_unlock(&behavior_ctx.tasks_lock);
	
	ai_info("Behavior monitoring system cleaned up");
}

/**
 * ai_behavior_get_statistics - Get behavior monitoring statistics
 * @samples: Output for samples collected
 * @patterns: Output for patterns detected
 * @tasks: Output for monitored tasks
 */
void ai_behavior_get_statistics(u64 *samples, u64 *patterns, u32 *tasks)
{
	if (samples)
		*samples = atomic64_read(&behavior_ctx.samples_collected);
	
	if (patterns)
		*patterns = atomic64_read(&behavior_ctx.patterns_detected);
	
	if (tasks)
		*tasks = behavior_ctx.total_tasks;
}

/* Export symbols */
EXPORT_SYMBOL(ai_register_task_behavior);
EXPORT_SYMBOL(ai_get_task_behavior_pattern);
EXPORT_SYMBOL(ai_behavior_get_statistics);