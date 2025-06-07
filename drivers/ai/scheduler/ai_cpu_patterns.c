/*
 * AI Scheduler CPU Usage Pattern Analysis
 * Advanced CPU usage pattern detection and prediction
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/sort.h>

#include "ai_scheduler.h"

/* CPU pattern analysis configuration */
#define AI_CPU_PATTERN_WINDOW		128	/* Number of CPU samples */
#define AI_CPU_PATTERN_TYPES		8	/* Number of pattern types */
#define AI_CPU_FREQUENCY_BINS		16	/* Frequency distribution bins */
#define AI_CPU_UTILIZATION_BINS		10	/* Utilization distribution bins */
#define AI_CPU_BURST_THRESHOLD		70	/* CPU burst threshold (%) */
#define AI_CPU_IDLE_THRESHOLD		10	/* CPU idle threshold (%) */

/* CPU usage pattern types */
enum ai_cpu_pattern_type {
	AI_CPU_PATTERN_STEADY = 0,	/* Steady CPU usage */
	AI_CPU_PATTERN_BURSTY,		/* Bursty CPU usage */
	AI_CPU_PATTERN_PERIODIC,	/* Periodic CPU usage */
	AI_CPU_PATTERN_ASCENDING,	/* Increasing CPU usage */
	AI_CPU_PATTERN_DESCENDING,	/* Decreasing CPU usage */
	AI_CPU_PATTERN_IDLE_HEAVY,	/* Mostly idle */
	AI_CPU_PATTERN_CPU_HEAVY,	/* CPU intensive */
	AI_CPU_PATTERN_IRREGULAR	/* Irregular pattern */
};

/* CPU usage sample */
struct ai_cpu_sample {
	ktime_t timestamp;
	u32 cpu_usage;		/* CPU usage percentage (0-100) */
	u32 frequency;		/* CPU frequency in kHz */
	u32 load_avg;		/* Load average */
	u32 nr_running;		/* Number of running tasks */
	u32 context_switches;	/* Context switches per second */
	u8 cpu_id;		/* CPU ID */
	u8 cluster_id;		/* CPU cluster ID */
};

/* CPU pattern statistics */
struct ai_cpu_pattern_stats {
	enum ai_cpu_pattern_type type;
	u32 frequency;		/* How often this pattern occurs */
	u32 duration_avg;	/* Average duration in ms */
	u32 cpu_usage_avg;	/* Average CPU usage during pattern */
	u32 cpu_usage_peak;	/* Peak CPU usage during pattern */
	u32 cpu_usage_variance; /* CPU usage variance */
	u32 frequency_avg;	/* Average frequency during pattern */
	u32 confidence;		/* Pattern confidence (0-100) */
	u64 last_occurrence;	/* Last time pattern was detected */
};

/* Per-task CPU pattern context */
struct ai_task_cpu_pattern {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	
	/* CPU usage samples */
	struct ai_cpu_sample samples[AI_CPU_PATTERN_WINDOW];
	u32 sample_index;
	u32 sample_count;
	spinlock_t samples_lock;
	
	/* Pattern statistics */
	struct ai_cpu_pattern_stats patterns[AI_CPU_PATTERN_TYPES];
	enum ai_cpu_pattern_type current_pattern;
	u32 pattern_confidence;
	
	/* Frequency distribution */
	u32 freq_distribution[AI_CPU_FREQUENCY_BINS];
	u32 util_distribution[AI_CPU_UTILIZATION_BINS];
	
	/* Prediction state */
	enum ai_cpu_pattern_type predicted_pattern;
	u32 prediction_confidence;
	u64 prediction_timestamp;
	
	/* Statistics */
	u64 total_cpu_time;
	u64 total_samples;
	u32 pattern_changes;
	u32 predictions_made;
	u32 predictions_correct;
	
	struct list_head list;
};

/* Global CPU pattern analysis context */
struct ai_cpu_pattern_ctx {
	struct list_head task_patterns;
	spinlock_t tasks_lock;
	u32 total_tasks;
	
	/* Global statistics */
	atomic64_t samples_analyzed;
	atomic64_t patterns_detected;
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	
	/* Configuration */
	bool analysis_enabled;
	u32 burst_threshold;
	u32 idle_threshold;
};

static struct ai_cpu_pattern_ctx cpu_pattern_ctx;

/* Pattern type names */
static const char *cpu_pattern_names[] = {
	"STEADY",
	"BURSTY", 
	"PERIODIC",
	"ASCENDING",
	"DESCENDING",
	"IDLE_HEAVY",
	"CPU_HEAVY",
	"IRREGULAR"
};

/**
 * ai_calculate_cpu_variance - Calculate CPU usage variance
 * @samples: Array of CPU samples
 * @count: Number of samples
 * @mean: Mean CPU usage
 * 
 * Returns: Variance value
 */
static u32 ai_calculate_cpu_variance(struct ai_cpu_sample *samples, u32 count, u32 mean)
{
	u64 variance_sum = 0;
	u32 i;
	
	if (count == 0)
		return 0;
	
	for (i = 0; i < count; i++) {
		s32 diff = samples[i].cpu_usage - mean;
		variance_sum += diff * diff;
	}
	
	return div64_u64(variance_sum, count);
}

/**
 * ai_detect_steady_pattern - Detect steady CPU usage pattern
 * @pattern: Task CPU pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_steady_pattern(struct ai_task_cpu_pattern *pattern)
{
	u32 mean = 0, variance, confidence = 0;
	u32 i;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Calculate mean */
	for (i = 0; i < pattern->sample_count; i++) {
		mean += pattern->samples[i].cpu_usage;
	}
	mean /= pattern->sample_count;
	
	/* Calculate variance */
	variance = ai_calculate_cpu_variance(pattern->samples, pattern->sample_count, mean);
	
	/* Low variance indicates steady pattern */
	if (variance < 100) {
		confidence = 90;
	} else if (variance < 300) {
		confidence = 70;
	} else if (variance < 500) {
		confidence = 50;
	}
	
	return confidence;
}

/**
 * ai_detect_bursty_pattern - Detect bursty CPU usage pattern
 * @pattern: Task CPU pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_bursty_pattern(struct ai_task_cpu_pattern *pattern)
{
	u32 burst_count = 0, idle_count = 0;
	u32 transitions = 0;
	u32 i, confidence = 0;
	bool in_burst = false;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Count bursts and transitions */
	for (i = 0; i < pattern->sample_count; i++) {
		u32 cpu_usage = pattern->samples[i].cpu_usage;
		
		if (cpu_usage > cpu_pattern_ctx.burst_threshold) {
			burst_count++;
			if (!in_burst) {
				transitions++;
				in_burst = true;
			}
		} else if (cpu_usage < cpu_pattern_ctx.idle_threshold) {
			idle_count++;
			if (in_burst) {
				transitions++;
				in_burst = false;
			}
		}
	}
	
	/* High number of transitions indicates bursty pattern */
	if (transitions > pattern->sample_count / 4) {
		confidence = min(transitions * 10, 95U);
	} else if (burst_count > 0 && idle_count > 0) {
		confidence = 60;
	}
	
	return confidence;
}

/**
 * ai_detect_periodic_pattern - Detect periodic CPU usage pattern
 * @pattern: Task CPU pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_periodic_pattern(struct ai_task_cpu_pattern *pattern)
{
	u32 period_candidates[8] = {2, 3, 4, 5, 8, 10, 16, 20};
	u32 best_correlation = 0;
	u32 i, j, confidence = 0;
	
	if (pattern->sample_count < 20)
		return 0;
	
	/* Test different period lengths */
	for (i = 0; i < ARRAY_SIZE(period_candidates); i++) {
		u32 period = period_candidates[i];
		u32 correlation = 0;
		u32 comparisons = 0;
		
		if (period >= pattern->sample_count / 2)
			continue;
		
		/* Calculate correlation for this period */
		for (j = period; j < pattern->sample_count; j++) {
			u32 current = pattern->samples[j].cpu_usage;
			u32 previous = pattern->samples[j - period].cpu_usage;
			
			/* Simple correlation: how similar are values separated by period */
			u32 diff = abs((int)current - (int)previous);
			if (diff < 20) {
				correlation++;
			}
			comparisons++;
		}
		
		if (comparisons > 0) {
			u32 correlation_percent = (correlation * 100) / comparisons;
			if (correlation_percent > best_correlation) {
				best_correlation = correlation_percent;
			}
		}
	}
	
	/* High correlation indicates periodic pattern */
	if (best_correlation > 80) {
		confidence = 90;
	} else if (best_correlation > 60) {
		confidence = 70;
	} else if (best_correlation > 40) {
		confidence = 50;
	}
	
	return confidence;
}

/**
 * ai_detect_trend_pattern - Detect ascending/descending trends
 * @pattern: Task CPU pattern context
 * @ascending: Output - true if ascending trend
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_trend_pattern(struct ai_task_cpu_pattern *pattern, bool *ascending)
{
	s32 trend_sum = 0;
	u32 trend_count = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Calculate trend by comparing adjacent samples */
	for (i = 1; i < pattern->sample_count; i++) {
		s32 diff = pattern->samples[i].cpu_usage - pattern->samples[i-1].cpu_usage;
		trend_sum += diff;
		trend_count++;
	}
	
	if (trend_count == 0)
		return 0;
	
	s32 avg_trend = trend_sum / trend_count;
	
	/* Determine trend direction and confidence */
	if (abs(avg_trend) > 2) {
		*ascending = (avg_trend > 0);
		confidence = min(abs(avg_trend) * 10, 90U);
	}
	
	return confidence;
}

/**
 * ai_analyze_cpu_pattern - Analyze CPU usage pattern for a task
 * @pattern: Task CPU pattern context
 */
static void ai_analyze_cpu_pattern(struct ai_task_cpu_pattern *pattern)
{
	enum ai_cpu_pattern_type detected_pattern = AI_CPU_PATTERN_IRREGULAR;
	u32 max_confidence = 0;
	u32 confidence;
	bool ascending;
	
	/* Test different pattern types */
	confidence = ai_detect_steady_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_CPU_PATTERN_STEADY;
	}
	
	confidence = ai_detect_bursty_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_CPU_PATTERN_BURSTY;
	}
	
	confidence = ai_detect_periodic_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_CPU_PATTERN_PERIODIC;
	}
	
	confidence = ai_detect_trend_pattern(pattern, &ascending);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = ascending ? AI_CPU_PATTERN_ASCENDING : AI_CPU_PATTERN_DESCENDING;
	}
	
	/* Check for CPU-heavy or idle-heavy patterns */
	u32 high_cpu_samples = 0, low_cpu_samples = 0;
	for (u32 i = 0; i < pattern->sample_count; i++) {
		if (pattern->samples[i].cpu_usage > 70)
			high_cpu_samples++;
		else if (pattern->samples[i].cpu_usage < 20)
			low_cpu_samples++;
	}
	
	if (high_cpu_samples > pattern->sample_count * 3 / 4) {
		detected_pattern = AI_CPU_PATTERN_CPU_HEAVY;
		max_confidence = 85;
	} else if (low_cpu_samples > pattern->sample_count * 3 / 4) {
		detected_pattern = AI_CPU_PATTERN_IDLE_HEAVY;
		max_confidence = 85;
	}
	
	/* Update pattern if confidence is sufficient */
	if (max_confidence > 60 && detected_pattern != pattern->current_pattern) {
		enum ai_cpu_pattern_type old_pattern = pattern->current_pattern;
		
		pattern->current_pattern = detected_pattern;
		pattern->pattern_confidence = max_confidence;
		pattern->pattern_changes++;
		
		/* Update pattern statistics */
		struct ai_cpu_pattern_stats *stats = &pattern->patterns[detected_pattern];
		stats->type = detected_pattern;
		stats->frequency++;
		stats->confidence = max_confidence;
		stats->last_occurrence = ktime_get_ns();
		
		atomic64_inc(&cpu_pattern_ctx.patterns_detected);
		
		ai_verbose("CPU pattern change for %s[%d]: %s -> %s (confidence: %u%%)",
			   pattern->comm, pattern->pid,
			   (old_pattern < AI_CPU_PATTERN_TYPES) ? cpu_pattern_names[old_pattern] : "UNKNOWN",
			   cpu_pattern_names[detected_pattern], max_confidence);
	}
}

/**
 * ai_predict_cpu_pattern - Predict next CPU usage pattern
 * @pattern: Task CPU pattern context
 * 
 * Returns: Predicted pattern type
 */
static enum ai_cpu_pattern_type ai_predict_cpu_pattern(struct ai_task_cpu_pattern *pattern)
{
	enum ai_cpu_pattern_type predicted = AI_CPU_PATTERN_IRREGULAR;
	u32 max_frequency = 0;
	u32 i;
	
	/* Simple prediction: most frequent pattern */
	for (i = 0; i < AI_CPU_PATTERN_TYPES; i++) {
		if (pattern->patterns[i].frequency > max_frequency) {
			max_frequency = pattern->patterns[i].frequency;
			predicted = i;
		}
	}
	
	/* Consider recent pattern changes */
	if (pattern->current_pattern != AI_CPU_PATTERN_IRREGULAR) {
		/* If current pattern is stable, predict it will continue */
		if (pattern->pattern_confidence > 80) {
			predicted = pattern->current_pattern;
		}
	}
	
	pattern->predicted_pattern = predicted;
	pattern->prediction_confidence = min(max_frequency * 10, 90U);
	pattern->prediction_timestamp = ktime_get_ns();
	pattern->predictions_made++;
	
	atomic64_inc(&cpu_pattern_ctx.predictions_made);
	
	return predicted;
}

/**
 * ai_collect_cpu_sample - Collect CPU usage sample for a task
 * @task: Target task
 * @pattern: Task CPU pattern context
 */
static void ai_collect_cpu_sample(struct task_struct *task,
				  struct ai_task_cpu_pattern *pattern)
{
	struct ai_cpu_sample *sample;
	struct ai_task_data *task_data;
	u32 index;
	int cpu;
	
	task_data = ai_get_task_data(task);
	if (!task_data)
		return;
	
	cpu = task_cpu(task);
	
	spin_lock(&pattern->samples_lock);
	
	/* Get next sample slot */
	index = pattern->sample_index;
	sample = &pattern->samples[index];
	
	/* Fill sample data */
	sample->timestamp = ktime_get();
	sample->cpu_usage = task_data->current_features.cpu_usage_avg;
	sample->load_avg = task->se.avg.load_avg;
	sample->context_switches = task_data->current_features.context_switches;
	sample->cpu_id = cpu;
	
	/* Get CPU frequency */
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	if (policy) {
		sample->frequency = policy->cur;
		cpufreq_cpu_put(policy);
	} else {
		sample->frequency = 0;
	}
	
	/* Find cluster ID */
	sample->cluster_id = 0;
	for (int i = 0; i < ai_sched_ctx.num_clusters; i++) {
		if (cpumask_test_cpu(cpu, &ai_sched_ctx.clusters[i].cpus)) {
			sample->cluster_id = i;
			break;
		}
	}
	
	/* Update ring buffer */
	pattern->sample_index = (index + 1) % AI_CPU_PATTERN_WINDOW;
	if (pattern->sample_count < AI_CPU_PATTERN_WINDOW)
		pattern->sample_count++;
	
	/* Update distributions */
	u32 freq_bin = min(sample->frequency / 200000, AI_CPU_FREQUENCY_BINS - 1U);
	u32 util_bin = min(sample->cpu_usage / 10, AI_CPU_UTILIZATION_BINS - 1U);
	pattern->freq_distribution[freq_bin]++;
	pattern->util_distribution[util_bin]++;
	
	pattern->total_cpu_time += sample->cpu_usage;
	pattern->total_samples++;
	
	spin_unlock(&pattern->samples_lock);
	
	ai_put_task_data(task_data);
	atomic64_inc(&cpu_pattern_ctx.samples_analyzed);
}

/**
 * ai_register_task_cpu_pattern - Register task for CPU pattern analysis
 * @task: Task to analyze
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_register_task_cpu_pattern(struct task_struct *task)
{
	struct ai_task_cpu_pattern *pattern;
	
	if (!task || !cpu_pattern_ctx.analysis_enabled)
		return -EINVAL;
	
	/* Check if already registered */
	spin_lock(&cpu_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &cpu_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&cpu_pattern_ctx.tasks_lock);
			return 0;
		}
	}
	spin_unlock(&cpu_pattern_ctx.tasks_lock);
	
	/* Allocate new pattern context */
	pattern = kzalloc(sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		return -ENOMEM;
	
	pattern->pid = task->pid;
	strncpy(pattern->comm, task->comm, TASK_COMM_LEN - 1);
	pattern->comm[TASK_COMM_LEN - 1] = '\0';
	
	spin_lock_init(&pattern->samples_lock);
	pattern->current_pattern = AI_CPU_PATTERN_IRREGULAR;
	
	/* Add to analysis list */
	spin_lock(&cpu_pattern_ctx.tasks_lock);
	list_add(&pattern->list, &cpu_pattern_ctx.task_patterns);
	cpu_pattern_ctx.total_tasks++;
	spin_unlock(&cpu_pattern_ctx.tasks_lock);
	
	ai_info("Registered task %s[%d] for CPU pattern analysis", 
		task->comm, task->pid);
	
	return 0;
}

/**
 * ai_update_task_cpu_pattern - Update CPU pattern analysis for a task
 * @task: Target task
 */
void ai_update_task_cpu_pattern(struct task_struct *task)
{
	struct ai_task_cpu_pattern *pattern;
	
	if (!task || !cpu_pattern_ctx.analysis_enabled)
		return;
	
	spin_lock(&cpu_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &cpu_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&cpu_pattern_ctx.tasks_lock);
			
			/* Collect sample and analyze */
			ai_collect_cpu_sample(task, pattern);
			ai_analyze_cpu_pattern(pattern);
			ai_predict_cpu_pattern(pattern);
			
			return;
		}
	}
	spin_unlock(&cpu_pattern_ctx.tasks_lock);
	
	/* Task not registered, register it now */
	ai_register_task_cpu_pattern(task);
}

/**
 * ai_get_task_cpu_pattern - Get CPU pattern for a task
 * @task: Target task
 * @confidence: Output for pattern confidence
 * 
 * Returns: Current CPU pattern type
 */
enum ai_cpu_pattern_type ai_get_task_cpu_pattern(struct task_struct *task, u32 *confidence)
{
	struct ai_task_cpu_pattern *pattern;
	enum ai_cpu_pattern_type type = AI_CPU_PATTERN_IRREGULAR;
	
	if (!task)
		return AI_CPU_PATTERN_IRREGULAR;
	
	spin_lock(&cpu_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &cpu_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			type = pattern->current_pattern;
			if (confidence)
				*confidence = pattern->pattern_confidence;
			break;
		}
	}
	spin_unlock(&cpu_pattern_ctx.tasks_lock);
	
	return type;
}

/**
 * ai_cpu_pattern_init - Initialize CPU pattern analysis system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_cpu_pattern_init(void)
{
	INIT_LIST_HEAD(&cpu_pattern_ctx.task_patterns);
	spin_lock_init(&cpu_pattern_ctx.tasks_lock);
	
	cpu_pattern_ctx.analysis_enabled = true;
	cpu_pattern_ctx.burst_threshold = AI_CPU_BURST_THRESHOLD;
	cpu_pattern_ctx.idle_threshold = AI_CPU_IDLE_THRESHOLD;
	
	atomic64_set(&cpu_pattern_ctx.samples_analyzed, 0);
	atomic64_set(&cpu_pattern_ctx.patterns_detected, 0);
	atomic64_set(&cpu_pattern_ctx.predictions_made, 0);
	atomic64_set(&cpu_pattern_ctx.predictions_correct, 0);
	
	ai_info("CPU pattern analysis system initialized");
	
	return 0;
}

/**
 * ai_cpu_pattern_exit - Cleanup CPU pattern analysis system
 */
void ai_cpu_pattern_exit(void)
{
	struct ai_task_cpu_pattern *pattern, *tmp;
	
	cpu_pattern_ctx.analysis_enabled = false;
	
	spin_lock(&cpu_pattern_ctx.tasks_lock);
	list_for_each_entry_safe(pattern, tmp, &cpu_pattern_ctx.task_patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	cpu_pattern_ctx.total_tasks = 0;
	spin_unlock(&cpu_pattern_ctx.tasks_lock);
	
	ai_info("CPU pattern analysis system cleaned up");
}

/**
 * ai_cpu_pattern_get_statistics - Get CPU pattern analysis statistics
 */
void ai_cpu_pattern_get_statistics(u64 *samples, u64 *patterns, u64 *predictions, u32 *tasks)
{
	if (samples)
		*samples = atomic64_read(&cpu_pattern_ctx.samples_analyzed);
	
	if (patterns)
		*patterns = atomic64_read(&cpu_pattern_ctx.patterns_detected);
	
	if (predictions)
		*predictions = atomic64_read(&cpu_pattern_ctx.predictions_made);
	
	if (tasks)
		*tasks = cpu_pattern_ctx.total_tasks;
}

/* Export symbols */
EXPORT_SYMBOL(ai_register_task_cpu_pattern);
EXPORT_SYMBOL(ai_update_task_cpu_pattern);
EXPORT_SYMBOL(ai_get_task_cpu_pattern);
EXPORT_SYMBOL(ai_cpu_pattern_get_statistics);