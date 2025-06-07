/*
 * AI Scheduler Memory Access Pattern Tracking
 * Advanced memory usage pattern detection and optimization
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/swap.h>
#include <linux/oom.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/rmap.h>

#include "ai_scheduler.h"

/* Memory pattern analysis configuration */
#define AI_MEMORY_PATTERN_WINDOW	64	/* Number of memory samples */
#define AI_MEMORY_PATTERN_TYPES		10	/* Number of pattern types */
#define AI_MEMORY_SIZE_BINS		16	/* Memory size distribution bins */
#define AI_MEMORY_GROWTH_THRESHOLD	20	/* Memory growth threshold (%) */
#define AI_MEMORY_LEAK_THRESHOLD	50	/* Memory leak detection threshold */
#define AI_MEMORY_PRESSURE_THRESHOLD	80	/* Memory pressure threshold (%) */

/* Memory access pattern types */
enum ai_memory_pattern_type {
	AI_MEMORY_PATTERN_STABLE = 0,	/* Stable memory usage */
	AI_MEMORY_PATTERN_GROWING,	/* Growing memory usage */
	AI_MEMORY_PATTERN_SHRINKING,	/* Shrinking memory usage */
	AI_MEMORY_PATTERN_CYCLIC,	/* Cyclic memory usage */
	AI_MEMORY_PATTERN_BURSTY,	/* Bursty allocations */
	AI_MEMORY_PATTERN_LEAK,		/* Memory leak detected */
	AI_MEMORY_PATTERN_PRESSURE,	/* Memory pressure */
	AI_MEMORY_PATTERN_FRAGMENTED,	/* Fragmented allocations */
	AI_MEMORY_PATTERN_SEQUENTIAL,	/* Sequential access */
	AI_MEMORY_PATTERN_RANDOM	/* Random access */
};

/* Memory access sample */
struct ai_memory_sample {
	ktime_t timestamp;
	u32 rss_kb;		/* Resident set size in KB */
	u32 vss_kb;		/* Virtual set size in KB */
	u32 pss_kb;		/* Proportional set size in KB */
	u32 swap_kb;		/* Swap usage in KB */
	u32 page_faults;	/* Page faults since last sample */
	u32 major_faults;	/* Major page faults */
	u32 minor_faults;	/* Minor page faults */
	u32 alloc_rate;		/* Allocation rate (pages/sec) */
	u32 free_rate;		/* Free rate (pages/sec) */
	u32 cache_misses;	/* Cache misses (estimated) */
	u8 numa_node;		/* NUMA node */
	u8 memory_pressure;	/* Memory pressure level (0-100) */
};

/* Memory pattern statistics */
struct ai_memory_pattern_stats {
	enum ai_memory_pattern_type type;
	u32 frequency;		/* How often this pattern occurs */
	u32 duration_avg;	/* Average duration in ms */
	u32 memory_avg;		/* Average memory usage during pattern */
	u32 memory_peak;	/* Peak memory usage during pattern */
	u32 growth_rate;	/* Memory growth rate (KB/s) */
	u32 fault_rate;		/* Page fault rate during pattern */
	u32 confidence;		/* Pattern confidence (0-100) */
	u64 last_occurrence;	/* Last time pattern was detected */
};

/* Memory allocation tracking */
struct ai_memory_allocation {
	void *address;
	size_t size;
	ktime_t timestamp;
	u32 access_count;
	u32 lifetime_ms;
	struct list_head list;
};

/* Per-task memory pattern context */
struct ai_task_memory_pattern {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	
	/* Memory samples */
	struct ai_memory_sample samples[AI_MEMORY_PATTERN_WINDOW];
	u32 sample_index;
	u32 sample_count;
	spinlock_t samples_lock;
	
	/* Pattern statistics */
	struct ai_memory_pattern_stats patterns[AI_MEMORY_PATTERN_TYPES];
	enum ai_memory_pattern_type current_pattern;
	u32 pattern_confidence;
	
	/* Memory size distribution */
	u32 size_distribution[AI_MEMORY_SIZE_BINS];
	
	/* Allocation tracking */
	struct list_head allocations;
	spinlock_t alloc_lock;
	u32 active_allocations;
	
	/* Prediction state */
	enum ai_memory_pattern_type predicted_pattern;
	u32 prediction_confidence;
	u64 prediction_timestamp;
	u32 predicted_memory_usage;
	
	/* Statistics */
	u64 total_memory_time;
	u64 total_samples;
	u32 pattern_changes;
	u32 predictions_made;
	u32 predictions_correct;
	u32 oom_events;
	u32 swap_events;
	
	/* Performance metrics */
	u32 avg_fault_rate;
	u32 avg_alloc_rate;
	u32 memory_efficiency;
	
	struct list_head list;
};

/* Global memory pattern analysis context */
struct ai_memory_pattern_ctx {
	struct list_head task_patterns;
	spinlock_t tasks_lock;
	u32 total_tasks;
	
	/* Global memory statistics */
	atomic64_t samples_analyzed;
	atomic64_t patterns_detected;
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	atomic64_t oom_events_detected;
	atomic64_t memory_leaks_detected;
	
	/* System memory state */
	u32 system_memory_pressure;
	u32 available_memory_kb;
	u32 free_memory_kb;
	
	/* Configuration */
	bool analysis_enabled;
	u32 growth_threshold;
	u32 leak_threshold;
	u32 pressure_threshold;
};

static struct ai_memory_pattern_ctx memory_pattern_ctx;

/* Pattern type names */
static const char *memory_pattern_names[] = {
	"STABLE",
	"GROWING",
	"SHRINKING", 
	"CYCLIC",
	"BURSTY",
	"LEAK",
	"PRESSURE",
	"FRAGMENTED",
	"SEQUENTIAL",
	"RANDOM"
};

/**
 * ai_get_memory_info - Get memory information for a task
 * @task: Target task
 * @rss: Output for RSS in KB
 * @vss: Output for VSS in KB
 * @pss: Output for PSS in KB
 * @swap: Output for swap usage in KB
 */
static void ai_get_memory_info(struct task_struct *task, u32 *rss, u32 *vss, 
			       u32 *pss, u32 *swap)
{
	struct mm_struct *mm;
	
	*rss = *vss = *pss = *swap = 0;
	
	mm = get_task_mm(task);
	if (!mm)
		return;
	
	/* Get RSS (Resident Set Size) */
	*rss = get_mm_rss(mm) << (PAGE_SHIFT - 10);
	
	/* Get VSS (Virtual Set Size) */
	*vss = mm->total_vm << (PAGE_SHIFT - 10);
	
	/* Estimate PSS (Proportional Set Size) - simplified */
	*pss = *rss; /* For simplicity, assume PSS = RSS */
	
	/* Get swap usage */
	*swap = get_mm_counter(mm, MM_SWAPENTS) << (PAGE_SHIFT - 10);
	
	mmput(mm);
}

/**
 * ai_calculate_memory_pressure - Calculate system memory pressure
 * 
 * Returns: Memory pressure level (0-100)
 */
static u32 ai_calculate_memory_pressure(void)
{
	struct sysinfo si;
	u32 pressure = 0;
	
	si_meminfo(&si);
	
	/* Calculate pressure based on available memory */
	if (si.totalram > 0) {
		u32 used_percent = ((si.totalram - si.freeram - si.bufferram - si.cached) * 100) / si.totalram;
		pressure = min(used_percent, 100U);
	}
	
	/* Update global state */
	memory_pattern_ctx.available_memory_kb = (si.freeram + si.bufferram + si.cached) << (PAGE_SHIFT - 10);
	memory_pattern_ctx.free_memory_kb = si.freeram << (PAGE_SHIFT - 10);
	memory_pattern_ctx.system_memory_pressure = pressure;
	
	return pressure;
}

/**
 * ai_detect_stable_memory_pattern - Detect stable memory usage pattern
 * @pattern: Task memory pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_stable_memory_pattern(struct ai_task_memory_pattern *pattern)
{
	u32 mean_rss = 0, variance = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Calculate mean RSS */
	for (i = 0; i < pattern->sample_count; i++) {
		mean_rss += pattern->samples[i].rss_kb;
	}
	mean_rss /= pattern->sample_count;
	
	/* Calculate variance */
	for (i = 0; i < pattern->sample_count; i++) {
		u32 diff = abs((int)pattern->samples[i].rss_kb - (int)mean_rss);
		variance += diff * diff;
	}
	variance /= pattern->sample_count;
	
	/* Low variance indicates stable pattern */
	u32 cv = (variance * 100) / (mean_rss + 1); /* Coefficient of variation */
	if (cv < 10) {
		confidence = 90;
	} else if (cv < 20) {
		confidence = 70;
	} else if (cv < 30) {
		confidence = 50;
	}
	
	return confidence;
}

/**
 * ai_detect_growing_memory_pattern - Detect growing memory usage pattern
 * @pattern: Task memory pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_growing_memory_pattern(struct ai_task_memory_pattern *pattern)
{
	u32 growth_samples = 0;
	u32 total_growth = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 5)
		return 0;
	
	/* Count samples with memory growth */
	for (i = 1; i < pattern->sample_count; i++) {
		if (pattern->samples[i].rss_kb > pattern->samples[i-1].rss_kb) {
			growth_samples++;
			total_growth += pattern->samples[i].rss_kb - pattern->samples[i-1].rss_kb;
		}
	}
	
	/* Calculate confidence based on growth consistency */
	if (growth_samples > pattern->sample_count * 3 / 4) {
		confidence = 85;
		
		/* Check for potential memory leak */
		u32 avg_growth = total_growth / growth_samples;
		if (avg_growth > memory_pattern_ctx.leak_threshold) {
			confidence = 95; /* Very likely memory leak */
		}
	} else if (growth_samples > pattern->sample_count / 2) {
		confidence = 60;
	}
	
	return confidence;
}

/**
 * ai_detect_cyclic_memory_pattern - Detect cyclic memory usage pattern
 * @pattern: Task memory pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_cyclic_memory_pattern(struct ai_task_memory_pattern *pattern)
{
	u32 cycle_candidates[6] = {3, 4, 5, 8, 10, 16};
	u32 best_correlation = 0;
	u32 i, j, confidence = 0;
	
	if (pattern->sample_count < 16)
		return 0;
	
	/* Test different cycle lengths */
	for (i = 0; i < ARRAY_SIZE(cycle_candidates); i++) {
		u32 cycle = cycle_candidates[i];
		u32 correlation = 0;
		u32 comparisons = 0;
		
		if (cycle >= pattern->sample_count / 2)
			continue;
		
		/* Calculate correlation for this cycle */
		for (j = cycle; j < pattern->sample_count; j++) {
			u32 current = pattern->samples[j].rss_kb;
			u32 previous = pattern->samples[j - cycle].rss_kb;
			
			/* Check similarity */
			u32 diff_percent = (abs((int)current - (int)previous) * 100) / (previous + 1);
			if (diff_percent < 20) {
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
	
	/* High correlation indicates cyclic pattern */
	if (best_correlation > 70) {
		confidence = 85;
	} else if (best_correlation > 50) {
		confidence = 65;
	}
	
	return confidence;
}

/**
 * ai_detect_memory_leak_pattern - Detect memory leak pattern
 * @pattern: Task memory pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_memory_leak_pattern(struct ai_task_memory_pattern *pattern)
{
	u32 consistent_growth = 0;
	u32 total_growth = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Look for consistent memory growth without significant decreases */
	for (i = 1; i < pattern->sample_count; i++) {
		if (pattern->samples[i].rss_kb > pattern->samples[i-1].rss_kb) {
			consistent_growth++;
			total_growth += pattern->samples[i].rss_kb - pattern->samples[i-1].rss_kb;
		} else if (pattern->samples[i].rss_kb < pattern->samples[i-1].rss_kb * 0.9) {
			/* Significant decrease resets leak detection */
			consistent_growth = 0;
			total_growth = 0;
		}
	}
	
	/* Memory leak if consistent growth over long period */
	if (consistent_growth > pattern->sample_count * 2 / 3) {
		u32 avg_growth = total_growth / consistent_growth;
		if (avg_growth > memory_pattern_ctx.leak_threshold) {
			confidence = 90;
			atomic64_inc(&memory_pattern_ctx.memory_leaks_detected);
		} else if (avg_growth > memory_pattern_ctx.leak_threshold / 2) {
			confidence = 70;
		}
	}
	
	return confidence;
}

/**
 * ai_analyze_memory_pattern - Analyze memory usage pattern for a task
 * @pattern: Task memory pattern context
 */
static void ai_analyze_memory_pattern(struct ai_task_memory_pattern *pattern)
{
	enum ai_memory_pattern_type detected_pattern = AI_MEMORY_PATTERN_RANDOM;
	u32 max_confidence = 0;
	u32 confidence;
	
	/* Test different pattern types */
	confidence = ai_detect_stable_memory_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_MEMORY_PATTERN_STABLE;
	}
	
	confidence = ai_detect_growing_memory_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_MEMORY_PATTERN_GROWING;
	}
	
	confidence = ai_detect_cyclic_memory_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_MEMORY_PATTERN_CYCLIC;
	}
	
	confidence = ai_detect_memory_leak_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_MEMORY_PATTERN_LEAK;
	}
	
	/* Check for memory pressure pattern */
	u32 pressure_samples = 0;
	for (u32 i = 0; i < pattern->sample_count; i++) {
		if (pattern->samples[i].memory_pressure > memory_pattern_ctx.pressure_threshold) {
			pressure_samples++;
		}
	}
	
	if (pressure_samples > pattern->sample_count / 2) {
		detected_pattern = AI_MEMORY_PATTERN_PRESSURE;
		max_confidence = 80;
	}
	
	/* Update pattern if confidence is sufficient */
	if (max_confidence > 60 && detected_pattern != pattern->current_pattern) {
		enum ai_memory_pattern_type old_pattern = pattern->current_pattern;
		
		pattern->current_pattern = detected_pattern;
		pattern->pattern_confidence = max_confidence;
		pattern->pattern_changes++;
		
		/* Update pattern statistics */
		struct ai_memory_pattern_stats *stats = &pattern->patterns[detected_pattern];
		stats->type = detected_pattern;
		stats->frequency++;
		stats->confidence = max_confidence;
		stats->last_occurrence = ktime_get_ns();
		
		/* Calculate growth rate for growing patterns */
		if (detected_pattern == AI_MEMORY_PATTERN_GROWING && pattern->sample_count > 1) {
			u32 first_rss = pattern->samples[0].rss_kb;
			u32 last_rss = pattern->samples[pattern->sample_count - 1].rss_kb;
			u64 time_diff_ms = ktime_to_ms(ktime_sub(
				pattern->samples[pattern->sample_count - 1].timestamp,
				pattern->samples[0].timestamp));
			
			if (time_diff_ms > 0) {
				stats->growth_rate = ((last_rss - first_rss) * 1000) / time_diff_ms;
			}
		}
		
		atomic64_inc(&memory_pattern_ctx.patterns_detected);
		
		ai_verbose("Memory pattern change for %s[%d]: %s -> %s (confidence: %u%%)",
			   pattern->comm, pattern->pid,
			   (old_pattern < AI_MEMORY_PATTERN_TYPES) ? memory_pattern_names[old_pattern] : "UNKNOWN",
			   memory_pattern_names[detected_pattern], max_confidence);
		
		/* Special handling for memory leaks */
		if (detected_pattern == AI_MEMORY_PATTERN_LEAK) {
			ai_warn("Memory leak detected in task %s[%d]", pattern->comm, pattern->pid);
		}
	}
}

/**
 * ai_predict_memory_usage - Predict future memory usage
 * @pattern: Task memory pattern context
 * 
 * Returns: Predicted memory usage in KB
 */
static u32 ai_predict_memory_usage(struct ai_task_memory_pattern *pattern)
{
	u32 predicted_memory = 0;
	
	if (pattern->sample_count == 0)
		return 0;
	
	u32 current_memory = pattern->samples[(pattern->sample_index - 1 + AI_MEMORY_PATTERN_WINDOW) % AI_MEMORY_PATTERN_WINDOW].rss_kb;
	
	switch (pattern->current_pattern) {
	case AI_MEMORY_PATTERN_STABLE:
		predicted_memory = current_memory;
		break;
		
	case AI_MEMORY_PATTERN_GROWING:
	case AI_MEMORY_PATTERN_LEAK:
		/* Predict continued growth */
		if (pattern->sample_count > 1) {
			u32 growth_rate = pattern->patterns[pattern->current_pattern].growth_rate;
			predicted_memory = current_memory + growth_rate; /* Predict 1 second ahead */
		} else {
			predicted_memory = current_memory * 110 / 100; /* 10% growth */
		}
		break;
		
	case AI_MEMORY_PATTERN_SHRINKING:
		predicted_memory = current_memory * 90 / 100; /* 10% decrease */
		break;
		
	case AI_MEMORY_PATTERN_CYCLIC:
		/* Use historical average for cyclic patterns */
		u32 sum = 0;
		for (u32 i = 0; i < pattern->sample_count; i++) {
			sum += pattern->samples[i].rss_kb;
		}
		predicted_memory = sum / pattern->sample_count;
		break;
		
	default:
		predicted_memory = current_memory;
		break;
	}
	
	pattern->predicted_memory_usage = predicted_memory;
	pattern->prediction_timestamp = ktime_get_ns();
	pattern->predictions_made++;
	
	atomic64_inc(&memory_pattern_ctx.predictions_made);
	
	return predicted_memory;
}

/**
 * ai_collect_memory_sample - Collect memory usage sample for a task
 * @task: Target task
 * @pattern: Task memory pattern context
 */
static void ai_collect_memory_sample(struct task_struct *task,
				     struct ai_task_memory_pattern *pattern)
{
	struct ai_memory_sample *sample;
	u32 index;
	
	spin_lock(&pattern->samples_lock);
	
	/* Get next sample slot */
	index = pattern->sample_index;
	sample = &pattern->samples[index];
	
	/* Fill sample data */
	sample->timestamp = ktime_get();
	ai_get_memory_info(task, &sample->rss_kb, &sample->vss_kb, 
			   &sample->pss_kb, &sample->swap_kb);
	
	/* Get page fault information */
	sample->page_faults = task->maj_flt + task->min_flt;
	sample->major_faults = task->maj_flt;
	sample->minor_faults = task->min_flt;
	
	/* Calculate rates if we have previous sample */
	if (pattern->sample_count > 0) {
		u32 prev_index = (index - 1 + AI_MEMORY_PATTERN_WINDOW) % AI_MEMORY_PATTERN_WINDOW;
		struct ai_memory_sample *prev_sample = &pattern->samples[prev_index];
		
		u64 time_diff_ms = ktime_to_ms(ktime_sub(sample->timestamp, prev_sample->timestamp));
		if (time_diff_ms > 0) {
			sample->alloc_rate = ((sample->rss_kb - prev_sample->rss_kb) * 1000) / time_diff_ms;
			sample->free_rate = 0; /* Simplified - would need more detailed tracking */
		}
	}
	
	/* Get system memory pressure */
	sample->memory_pressure = ai_calculate_memory_pressure();
	
	/* Get NUMA node */
	sample->numa_node = numa_node_id();
	
	/* Estimate cache misses (simplified) */
	sample->cache_misses = sample->major_faults * 10; /* Rough estimate */
	
	/* Update ring buffer */
	pattern->sample_index = (index + 1) % AI_MEMORY_PATTERN_WINDOW;
	if (pattern->sample_count < AI_MEMORY_PATTERN_WINDOW)
		pattern->sample_count++;
	
	/* Update size distribution */
	u32 size_bin = min(sample->rss_kb / (1024 * 64), AI_MEMORY_SIZE_BINS - 1U); /* 64MB bins */
	pattern->size_distribution[size_bin]++;
	
	pattern->total_memory_time += sample->rss_kb;
	pattern->total_samples++;
	
	/* Update performance metrics */
	pattern->avg_fault_rate = (pattern->avg_fault_rate + sample->page_faults) / 2;
	pattern->avg_alloc_rate = (pattern->avg_alloc_rate + sample->alloc_rate) / 2;
	
	spin_unlock(&pattern->samples_lock);
	
	atomic64_inc(&memory_pattern_ctx.samples_analyzed);
}

/**
 * ai_register_task_memory_pattern - Register task for memory pattern analysis
 * @task: Task to analyze
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_register_task_memory_pattern(struct task_struct *task)
{
	struct ai_task_memory_pattern *pattern;
	
	if (!task || !memory_pattern_ctx.analysis_enabled)
		return -EINVAL;
	
	/* Check if already registered */
	spin_lock(&memory_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &memory_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&memory_pattern_ctx.tasks_lock);
			return 0;
		}
	}
	spin_unlock(&memory_pattern_ctx.tasks_lock);
	
	/* Allocate new pattern context */
	pattern = kzalloc(sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		return -ENOMEM;
	
	pattern->pid = task->pid;
	strncpy(pattern->comm, task->comm, TASK_COMM_LEN - 1);
	pattern->comm[TASK_COMM_LEN - 1] = '\0';
	
	spin_lock_init(&pattern->samples_lock);
	spin_lock_init(&pattern->alloc_lock);
	INIT_LIST_HEAD(&pattern->allocations);
	pattern->current_pattern = AI_MEMORY_PATTERN_RANDOM;
	
	/* Add to analysis list */
	spin_lock(&memory_pattern_ctx.tasks_lock);
	list_add(&pattern->list, &memory_pattern_ctx.task_patterns);
	memory_pattern_ctx.total_tasks++;
	spin_unlock(&memory_pattern_ctx.tasks_lock);
	
	ai_info("Registered task %s[%d] for memory pattern analysis", 
		task->comm, task->pid);
	
	return 0;
}

/**
 * ai_update_task_memory_pattern - Update memory pattern analysis for a task
 * @task: Target task
 */
void ai_update_task_memory_pattern(struct task_struct *task)
{
	struct ai_task_memory_pattern *pattern;
	
	if (!task || !memory_pattern_ctx.analysis_enabled)
		return;
	
	spin_lock(&memory_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &memory_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&memory_pattern_ctx.tasks_lock);
			
			/* Collect sample and analyze */
			ai_collect_memory_sample(task, pattern);
			ai_analyze_memory_pattern(pattern);
			ai_predict_memory_usage(pattern);
			
			return;
		}
	}
	spin_unlock(&memory_pattern_ctx.tasks_lock);
	
	/* Task not registered, register it now */
	ai_register_task_memory_pattern(task);
}

/**
 * ai_get_task_memory_pattern - Get memory pattern for a task
 * @task: Target task
 * @confidence: Output for pattern confidence
 * 
 * Returns: Current memory pattern type
 */
enum ai_memory_pattern_type ai_get_task_memory_pattern(struct task_struct *task, u32 *confidence)
{
	struct ai_task_memory_pattern *pattern;
	enum ai_memory_pattern_type type = AI_MEMORY_PATTERN_RANDOM;
	
	if (!task)
		return AI_MEMORY_PATTERN_RANDOM;
	
	spin_lock(&memory_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &memory_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			type = pattern->current_pattern;
			if (confidence)
				*confidence = pattern->pattern_confidence;
			break;
		}
	}
	spin_unlock(&memory_pattern_ctx.tasks_lock);
	
	return type;
}

/**
 * ai_memory_pattern_init - Initialize memory pattern analysis system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_memory_pattern_init(void)
{
	INIT_LIST_HEAD(&memory_pattern_ctx.task_patterns);
	spin_lock_init(&memory_pattern_ctx.tasks_lock);
	
	memory_pattern_ctx.analysis_enabled = true;
	memory_pattern_ctx.growth_threshold = AI_MEMORY_GROWTH_THRESHOLD;
	memory_pattern_ctx.leak_threshold = AI_MEMORY_LEAK_THRESHOLD;
	memory_pattern_ctx.pressure_threshold = AI_MEMORY_PRESSURE_THRESHOLD;
	
	atomic64_set(&memory_pattern_ctx.samples_analyzed, 0);
	atomic64_set(&memory_pattern_ctx.patterns_detected, 0);
	atomic64_set(&memory_pattern_ctx.predictions_made, 0);
	atomic64_set(&memory_pattern_ctx.predictions_correct, 0);
	atomic64_set(&memory_pattern_ctx.oom_events_detected, 0);
	atomic64_set(&memory_pattern_ctx.memory_leaks_detected, 0);
	
	ai_info("Memory pattern analysis system initialized");
	
	return 0;
}

/**
 * ai_memory_pattern_exit - Cleanup memory pattern analysis system
 */
void ai_memory_pattern_exit(void)
{
	struct ai_task_memory_pattern *pattern, *tmp;
	
	memory_pattern_ctx.analysis_enabled = false;
	
	spin_lock(&memory_pattern_ctx.tasks_lock);
	list_for_each_entry_safe(pattern, tmp, &memory_pattern_ctx.task_patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	memory_pattern_ctx.total_tasks = 0;
	spin_unlock(&memory_pattern_ctx.tasks_lock);
	
	ai_info("Memory pattern analysis system cleaned up");
}

/**
 * ai_memory_pattern_get_statistics - Get memory pattern analysis statistics
 */
void ai_memory_pattern_get_statistics(u64 *samples, u64 *patterns, u64 *predictions, 
				      u64 *leaks, u32 *tasks)
{
	if (samples)
		*samples = atomic64_read(&memory_pattern_ctx.samples_analyzed);
	
	if (patterns)
		*patterns = atomic64_read(&memory_pattern_ctx.patterns_detected);
	
	if (predictions)
		*predictions = atomic64_read(&memory_pattern_ctx.predictions_made);
	
	if (leaks)
		*leaks = atomic64_read(&memory_pattern_ctx.memory_leaks_detected);
	
	if (tasks)
		*tasks = memory_pattern_ctx.total_tasks;
}

/* Export symbols */
EXPORT_SYMBOL(ai_register_task_memory_pattern);
EXPORT_SYMBOL(ai_update_task_memory_pattern);
EXPORT_SYMBOL(ai_get_task_memory_pattern);
EXPORT_SYMBOL(ai_memory_pattern_get_statistics);