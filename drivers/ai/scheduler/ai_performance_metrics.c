/*
 * AI Scheduler Performance Metrics Collection
 * Comprehensive performance monitoring and analysis system
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/math64.h>
#include <linux/sort.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#include <linux/pm_qos.h>

#include "ai_scheduler.h"

/* Performance metrics configuration */
#define AI_PERF_METRICS_WINDOW		256	/* Number of metric samples */
#define AI_PERF_COLLECTION_INTERVAL_MS	500	/* Collection interval */
#define AI_PERF_CATEGORIES		16	/* Number of metric categories */
#define AI_PERF_PERCENTILES		5	/* Number of percentiles to track */
#define AI_PERF_HISTORY_SIZE		1024	/* Historical data size */

/* Performance metric categories */
enum ai_perf_metric_category {
	AI_PERF_CPU_UTILIZATION = 0,
	AI_PERF_MEMORY_USAGE,
	AI_PERF_IO_THROUGHPUT,
	AI_PERF_NETWORK_LATENCY,
	AI_PERF_POWER_CONSUMPTION,
	AI_PERF_THERMAL_STATE,
	AI_PERF_SCHEDULER_EFFICIENCY,
	AI_PERF_RESPONSE_TIME,
	AI_PERF_FRAME_RATE,
	AI_PERF_BATTERY_DRAIN,
	AI_PERF_CACHE_EFFICIENCY,
	AI_PERF_INTERRUPT_LATENCY,
	AI_PERF_CONTEXT_SWITCH_TIME,
	AI_PERF_PAGE_FAULT_RATE,
	AI_PERF_SYSTEM_LOAD,
	AI_PERF_USER_EXPERIENCE
};

/* Performance metric types */
enum ai_perf_metric_type {
	AI_METRIC_COUNTER = 0,		/* Monotonic counter */
	AI_METRIC_GAUGE,		/* Current value */
	AI_METRIC_HISTOGRAM,		/* Distribution */
	AI_METRIC_RATE,			/* Rate per second */
	AI_METRIC_LATENCY		/* Time-based metric */
};

/* Performance sample */
struct ai_perf_sample {
	ktime_t timestamp;
	enum ai_perf_metric_category category;
	enum ai_perf_metric_type type;
	u64 value;			/* Metric value */
	u32 cpu_id;			/* CPU where measured */
	u32 task_pid;			/* Associated task PID */
	char task_name[16];		/* Associated task name */
	u32 context_flags;		/* Context information */
};

/* Performance statistics */
struct ai_perf_stats {
	enum ai_perf_metric_category category;
	
	/* Basic statistics */
	u64 count;			/* Number of samples */
	u64 sum;			/* Sum of all values */
	u64 min;			/* Minimum value */
	u64 max;			/* Maximum value */
	u64 avg;			/* Average value */
	u64 variance;			/* Variance */
	
	/* Percentiles */
	u64 percentiles[AI_PERF_PERCENTILES]; /* 50th, 75th, 90th, 95th, 99th */
	
	/* Rate calculations */
	u64 rate_per_second;		/* Current rate */
	u64 peak_rate;			/* Peak rate observed */
	
	/* Trend analysis */
	s32 trend;			/* Trend direction (-100 to +100) */
	u32 stability;			/* Stability score (0-100) */
	
	/* Quality metrics */
	u32 quality_score;		/* Overall quality (0-100) */
	u32 efficiency_score;		/* Efficiency score (0-100) */
	
	/* Timing */
	u64 last_update;		/* Last update timestamp */
	u64 collection_start;		/* Collection start time */
};

/* Performance alert */
struct ai_perf_alert {
	enum ai_perf_metric_category category;
	u32 severity;			/* Alert severity (0-100) */
	u64 threshold_value;		/* Threshold that was crossed */
	u64 current_value;		/* Current metric value */
	ktime_t timestamp;		/* When alert was triggered */
	char description[64];		/* Alert description */
	bool active;			/* Alert is active */
	struct list_head list;
};

/* Performance benchmark */
struct ai_perf_benchmark {
	char name[32];			/* Benchmark name */
	enum ai_perf_metric_category category;
	u64 target_value;		/* Target performance value */
	u64 baseline_value;		/* Baseline value */
	u64 current_value;		/* Current measured value */
	u32 score;			/* Benchmark score (0-100) */
	u32 improvement;		/* Improvement percentage */
	bool passed;			/* Benchmark passed */
	struct list_head list;
};

/* Performance metrics context */
struct ai_perf_metrics_ctx {
	/* Sample storage */
	struct ai_perf_sample samples[AI_PERF_METRICS_WINDOW];
	u32 sample_index;
	u32 sample_count;
	spinlock_t samples_lock;
	
	/* Statistics */
	struct ai_perf_stats stats[AI_PERF_CATEGORIES];
	spinlock_t stats_lock;
	
	/* Alerts */
	struct list_head alerts;
	spinlock_t alerts_lock;
	u32 active_alerts;
	
	/* Benchmarks */
	struct list_head benchmarks;
	spinlock_t benchmarks_lock;
	u32 total_benchmarks;
	u32 passed_benchmarks;
	
	/* Historical data */
	u64 history[AI_PERF_CATEGORIES][AI_PERF_HISTORY_SIZE];
	u32 history_index[AI_PERF_CATEGORIES];
	
	/* Collection state */
	struct delayed_work collection_work;
	struct workqueue_struct *collection_wq;
	bool collection_active;
	u32 collection_interval_ms;
	
	/* Global metrics */
	u64 total_samples_collected;
	u64 collection_start_time;
	u32 collection_accuracy;
	u32 system_performance_score;
	
	/* Configuration */
	bool detailed_collection;
	bool alert_enabled;
	bool benchmark_enabled;
	u32 alert_threshold_multiplier;
};

static struct ai_perf_metrics_ctx perf_metrics_ctx;

/* Metric category names */
static const char *perf_category_names[] = {
	"CPU_UTILIZATION",
	"MEMORY_USAGE",
	"IO_THROUGHPUT",
	"NETWORK_LATENCY",
	"POWER_CONSUMPTION",
	"THERMAL_STATE",
	"SCHEDULER_EFFICIENCY",
	"RESPONSE_TIME",
	"FRAME_RATE",
	"BATTERY_DRAIN",
	"CACHE_EFFICIENCY",
	"INTERRUPT_LATENCY",
	"CONTEXT_SWITCH_TIME",
	"PAGE_FAULT_RATE",
	"SYSTEM_LOAD",
	"USER_EXPERIENCE"
};

/**
 * ai_collect_cpu_metrics - Collect CPU performance metrics
 */
static void ai_collect_cpu_metrics(void)
{
	int cpu;
	u64 total_util = 0;
	u32 active_cpus = 0;
	
	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
		if (policy) {
			u32 util = (policy->cur * 100) / policy->max;
			total_util += util;
			active_cpus++;
			cpufreq_cpu_put(policy);
		}
	}
	
	if (active_cpus > 0) {
		u64 avg_util = total_util / active_cpus;
		ai_record_performance_metric(AI_PERF_CPU_UTILIZATION, AI_METRIC_GAUGE, 
					     avg_util, 0, 0, "system");
	}
}

/**
 * ai_collect_memory_metrics - Collect memory performance metrics
 */
static void ai_collect_memory_metrics(void)
{
	struct sysinfo si;
	u64 used_memory, memory_util;
	
	si_meminfo(&si);
	
	used_memory = (si.totalram - si.freeram - si.bufferram - si.cached) << (PAGE_SHIFT - 10);
	memory_util = (used_memory * 100) / (si.totalram << (PAGE_SHIFT - 10));
	
	ai_record_performance_metric(AI_PERF_MEMORY_USAGE, AI_METRIC_GAUGE, 
				     memory_util, 0, 0, "system");
}

/**
 * ai_collect_thermal_metrics - Collect thermal performance metrics
 */
static void ai_collect_thermal_metrics(void)
{
	/* Simplified thermal collection - would need thermal framework integration */
	u32 thermal_state = 0; /* 0-100 scale */
	
	/* Estimate thermal state based on CPU frequency throttling */
	int cpu;
	u32 throttled_cpus = 0;
	u32 total_cpus = 0;
	
	for_each_online_cpu(cpu) {
		struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
		if (policy) {
			total_cpus++;
			if (policy->cur < policy->max * 90 / 100) { /* <90% of max freq */
				throttled_cpus++;
			}
			cpufreq_cpu_put(policy);
		}
	}
	
	if (total_cpus > 0) {
		thermal_state = (throttled_cpus * 100) / total_cpus;
	}
	
	ai_record_performance_metric(AI_PERF_THERMAL_STATE, AI_METRIC_GAUGE, 
				     thermal_state, 0, 0, "system");
}

/**
 * ai_collect_scheduler_metrics - Collect scheduler efficiency metrics
 */
static void ai_collect_scheduler_metrics(void)
{
	/* Collect AI scheduler specific metrics */
	u64 predictions_made, predictions_correct;
	u32 accuracy = 0;
	
	/* Get AI scheduler statistics */
	if (ai_sched_ctx.learning_enabled) {
		predictions_made = atomic64_read(&ai_sched_ctx.predictions_made);
		predictions_correct = atomic64_read(&ai_sched_ctx.predictions_correct);
		
		if (predictions_made > 0) {
			accuracy = (predictions_correct * 100) / predictions_made;
		}
	}
	
	ai_record_performance_metric(AI_PERF_SCHEDULER_EFFICIENCY, AI_METRIC_GAUGE, 
				     accuracy, 0, 0, "ai_scheduler");
}

/**
 * ai_calculate_percentiles - Calculate percentiles for a metric
 * @category: Metric category
 * @values: Array of values
 * @count: Number of values
 */
static void ai_calculate_percentiles(enum ai_perf_metric_category category, 
				     u64 *values, u32 count)
{
	struct ai_perf_stats *stats = &perf_metrics_ctx.stats[category];
	u32 indices[] = {50, 75, 90, 95, 99}; /* Percentile indices */
	int i;
	
	if (count == 0)
		return;
	
	/* Sort values */
	sort(values, count, sizeof(u64), NULL, NULL);
	
	/* Calculate percentiles */
	for (i = 0; i < AI_PERF_PERCENTILES; i++) {
		u32 index = (indices[i] * count) / 100;
		if (index >= count)
			index = count - 1;
		stats->percentiles[i] = values[index];
	}
}

/**
 * ai_update_performance_stats - Update performance statistics
 * @category: Metric category
 * @value: New metric value
 */
static void ai_update_performance_stats(enum ai_perf_metric_category category, u64 value)
{
	struct ai_perf_stats *stats = &perf_metrics_ctx.stats[category];
	u64 old_avg;
	
	spin_lock(&perf_metrics_ctx.stats_lock);
	
	/* Update basic statistics */
	stats->count++;
	stats->sum += value;
	
	if (stats->count == 1) {
		stats->min = stats->max = stats->avg = value;
		stats->variance = 0;
	} else {
		if (value < stats->min)
			stats->min = value;
		if (value > stats->max)
			stats->max = value;
		
		old_avg = stats->avg;
		stats->avg = stats->sum / stats->count;
		
		/* Update variance (simplified) */
		u64 diff = (value > stats->avg) ? (value - stats->avg) : (stats->avg - value);
		stats->variance = (stats->variance + diff * diff) / 2;
	}
	
	/* Calculate trend */
	if (stats->count > 10) {
		if (value > old_avg * 105 / 100) {
			stats->trend = min(stats->trend + 5, 100);
		} else if (value < old_avg * 95 / 100) {
			stats->trend = max(stats->trend - 5, -100);
		}
	}
	
	/* Calculate stability (inverse of coefficient of variation) */
	if (stats->avg > 0) {
		u32 cv = (stats->variance * 100) / (stats->avg * stats->avg);
		stats->stability = max(0U, 100 - cv);
	}
	
	/* Update historical data */
	u32 hist_idx = perf_metrics_ctx.history_index[category];
	perf_metrics_ctx.history[category][hist_idx] = value;
	perf_metrics_ctx.history_index[category] = (hist_idx + 1) % AI_PERF_HISTORY_SIZE;
	
	stats->last_update = ktime_get_ns();
	
	spin_unlock(&perf_metrics_ctx.stats_lock);
}

/**
 * ai_check_performance_alerts - Check for performance alerts
 * @category: Metric category
 * @value: Current metric value
 */
static void ai_check_performance_alerts(enum ai_perf_metric_category category, u64 value)
{
	struct ai_perf_stats *stats = &perf_metrics_ctx.stats[category];
	struct ai_perf_alert *alert;
	bool should_alert = false;
	u64 threshold = 0;
	
	if (!perf_metrics_ctx.alert_enabled || stats->count < 10)
		return;
	
	/* Define alert thresholds based on category */
	switch (category) {
	case AI_PERF_CPU_UTILIZATION:
		threshold = 90; /* 90% CPU utilization */
		should_alert = (value > threshold);
		break;
		
	case AI_PERF_MEMORY_USAGE:
		threshold = 85; /* 85% memory usage */
		should_alert = (value > threshold);
		break;
		
	case AI_PERF_THERMAL_STATE:
		threshold = 70; /* 70% thermal throttling */
		should_alert = (value > threshold);
		break;
		
	case AI_PERF_RESPONSE_TIME:
		threshold = stats->avg * 150 / 100; /* 50% above average */
		should_alert = (value > threshold);
		break;
		
	default:
		/* Use statistical threshold (2 standard deviations) */
		u64 std_dev = int_sqrt(stats->variance);
		threshold = stats->avg + (2 * std_dev);
		should_alert = (value > threshold);
		break;
	}
	
	if (!should_alert)
		return;
	
	/* Create or update alert */
	spin_lock(&perf_metrics_ctx.alerts_lock);
	
	/* Check if alert already exists */
	list_for_each_entry(alert, &perf_metrics_ctx.alerts, list) {
		if (alert->category == category && alert->active) {
			alert->current_value = value;
			alert->timestamp = ktime_get();
			spin_unlock(&perf_metrics_ctx.alerts_lock);
			return;
		}
	}
	
	/* Create new alert */
	alert = kzalloc(sizeof(*alert), GFP_ATOMIC);
	if (alert) {
		alert->category = category;
		alert->severity = min((value * 100) / threshold, 100UL);
		alert->threshold_value = threshold;
		alert->current_value = value;
		alert->timestamp = ktime_get();
		alert->active = true;
		
		snprintf(alert->description, sizeof(alert->description),
			 "%s exceeded threshold: %llu > %llu",
			 perf_category_names[category], value, threshold);
		
		list_add(&alert->list, &perf_metrics_ctx.alerts);
		perf_metrics_ctx.active_alerts++;
		
		ai_warn("Performance alert: %s", alert->description);
	}
	
	spin_unlock(&perf_metrics_ctx.alerts_lock);
}

/**
 * ai_record_performance_metric - Record a performance metric
 * @category: Metric category
 * @type: Metric type
 * @value: Metric value
 * @cpu_id: CPU ID (if applicable)
 * @task_pid: Task PID (if applicable)
 * @task_name: Task name (if applicable)
 */
void ai_record_performance_metric(enum ai_perf_metric_category category,
				  enum ai_perf_metric_type type,
				  u64 value, u32 cpu_id, u32 task_pid,
				  const char *task_name)
{
	struct ai_perf_sample *sample;
	u32 index;
	
	if (!perf_metrics_ctx.collection_active || category >= AI_PERF_CATEGORIES)
		return;
	
	spin_lock(&perf_metrics_ctx.samples_lock);
	
	/* Get next sample slot */
	index = perf_metrics_ctx.sample_index;
	sample = &perf_metrics_ctx.samples[index];
	
	/* Fill sample data */
	sample->timestamp = ktime_get();
	sample->category = category;
	sample->type = type;
	sample->value = value;
	sample->cpu_id = cpu_id;
	sample->task_pid = task_pid;
	
	if (task_name) {
		strncpy(sample->task_name, task_name, 15);
		sample->task_name[15] = '\0';
	} else {
		sample->task_name[0] = '\0';
	}
	
	/* Update ring buffer */
	perf_metrics_ctx.sample_index = (index + 1) % AI_PERF_METRICS_WINDOW;
	if (perf_metrics_ctx.sample_count < AI_PERF_METRICS_WINDOW)
		perf_metrics_ctx.sample_count++;
	
	perf_metrics_ctx.total_samples_collected++;
	
	spin_unlock(&perf_metrics_ctx.samples_lock);
	
	/* Update statistics */
	ai_update_performance_stats(category, value);
	
	/* Check for alerts */
	ai_check_performance_alerts(category, value);
}

/**
 * ai_performance_collection_worker - Background performance collection worker
 * @work: Work structure
 */
static void ai_performance_collection_worker(struct work_struct *work)
{
	if (!perf_metrics_ctx.collection_active)
		return;
	
	/* Collect system-wide metrics */
	ai_collect_cpu_metrics();
	ai_collect_memory_metrics();
	ai_collect_thermal_metrics();
	ai_collect_scheduler_metrics();
	
	/* Calculate system performance score */
	u32 cpu_score = 100 - min(perf_metrics_ctx.stats[AI_PERF_CPU_UTILIZATION].avg, 100UL);
	u32 mem_score = 100 - min(perf_metrics_ctx.stats[AI_PERF_MEMORY_USAGE].avg, 100UL);
	u32 thermal_score = 100 - min(perf_metrics_ctx.stats[AI_PERF_THERMAL_STATE].avg, 100UL);
	u32 scheduler_score = perf_metrics_ctx.stats[AI_PERF_SCHEDULER_EFFICIENCY].avg;
	
	perf_metrics_ctx.system_performance_score = 
		(cpu_score + mem_score + thermal_score + scheduler_score) / 4;
	
	/* Schedule next collection */
	if (perf_metrics_ctx.collection_active) {
		queue_delayed_work(perf_metrics_ctx.collection_wq,
				   &perf_metrics_ctx.collection_work,
				   msecs_to_jiffies(perf_metrics_ctx.collection_interval_ms));
	}
}

/**
 * ai_get_performance_stats - Get performance statistics
 * @category: Metric category
 * 
 * Returns: Pointer to performance statistics
 */
const struct ai_perf_stats *ai_get_performance_stats(enum ai_perf_metric_category category)
{
	if (category >= AI_PERF_CATEGORIES)
		return NULL;
	
	return &perf_metrics_ctx.stats[category];
}

/**
 * ai_get_system_performance_score - Get overall system performance score
 * 
 * Returns: Performance score (0-100)
 */
u32 ai_get_system_performance_score(void)
{
	return perf_metrics_ctx.system_performance_score;
}

/**
 * ai_performance_metrics_init - Initialize performance metrics collection
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_performance_metrics_init(void)
{
	int i;
	
	/* Initialize context */
	spin_lock_init(&perf_metrics_ctx.samples_lock);
	spin_lock_init(&perf_metrics_ctx.stats_lock);
	spin_lock_init(&perf_metrics_ctx.alerts_lock);
	spin_lock_init(&perf_metrics_ctx.benchmarks_lock);
	
	INIT_LIST_HEAD(&perf_metrics_ctx.alerts);
	INIT_LIST_HEAD(&perf_metrics_ctx.benchmarks);
	
	/* Initialize statistics */
	for (i = 0; i < AI_PERF_CATEGORIES; i++) {
		struct ai_perf_stats *stats = &perf_metrics_ctx.stats[i];
		stats->category = i;
		stats->min = ULLONG_MAX;
		stats->collection_start = ktime_get_ns();
	}
	
	/* Create work queue */
	perf_metrics_ctx.collection_wq = create_singlethread_workqueue("ai_perf_metrics");
	if (!perf_metrics_ctx.collection_wq) {
		ai_error("Failed to create performance metrics work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&perf_metrics_ctx.collection_work, 
			  ai_performance_collection_worker);
	
	/* Configuration */
	perf_metrics_ctx.collection_active = true;
	perf_metrics_ctx.detailed_collection = true;
	perf_metrics_ctx.alert_enabled = true;
	perf_metrics_ctx.benchmark_enabled = true;
	perf_metrics_ctx.collection_interval_ms = AI_PERF_COLLECTION_INTERVAL_MS;
	perf_metrics_ctx.alert_threshold_multiplier = 150; /* 50% above normal */
	
	perf_metrics_ctx.collection_start_time = ktime_get_ns();
	
	/* Start collection */
	queue_delayed_work(perf_metrics_ctx.collection_wq,
			   &perf_metrics_ctx.collection_work,
			   msecs_to_jiffies(perf_metrics_ctx.collection_interval_ms));
	
	ai_info("Performance metrics collection initialized");
	
	return 0;
}

/**
 * ai_performance_metrics_exit - Cleanup performance metrics collection
 */
void ai_performance_metrics_exit(void)
{
	struct ai_perf_alert *alert, *tmp_alert;
	struct ai_perf_benchmark *benchmark, *tmp_benchmark;
	
	perf_metrics_ctx.collection_active = false;
	
	/* Stop work queue */
	if (perf_metrics_ctx.collection_wq) {
		cancel_delayed_work_sync(&perf_metrics_ctx.collection_work);
		destroy_workqueue(perf_metrics_ctx.collection_wq);
		perf_metrics_ctx.collection_wq = NULL;
	}
	
	/* Clean up alerts */
	spin_lock(&perf_metrics_ctx.alerts_lock);
	list_for_each_entry_safe(alert, tmp_alert, &perf_metrics_ctx.alerts, list) {
		list_del(&alert->list);
		kfree(alert);
	}
	perf_metrics_ctx.active_alerts = 0;
	spin_unlock(&perf_metrics_ctx.alerts_lock);
	
	/* Clean up benchmarks */
	spin_lock(&perf_metrics_ctx.benchmarks_lock);
	list_for_each_entry_safe(benchmark, tmp_benchmark, &perf_metrics_ctx.benchmarks, list) {
		list_del(&benchmark->list);
		kfree(benchmark);
	}
	perf_metrics_ctx.total_benchmarks = 0;
	spin_unlock(&perf_metrics_ctx.benchmarks_lock);
	
	ai_info("Performance metrics collection cleaned up");
}

/**
 * ai_performance_metrics_get_statistics - Get collection statistics
 */
void ai_performance_metrics_get_statistics(u64 *samples, u32 *alerts, u32 *score,
					   u64 *collection_time)
{
	if (samples)
		*samples = perf_metrics_ctx.total_samples_collected;
	
	if (alerts)
		*alerts = perf_metrics_ctx.active_alerts;
	
	if (score)
		*score = perf_metrics_ctx.system_performance_score;
	
	if (collection_time) {
		*collection_time = ktime_to_ms(ktime_sub(ktime_get(), 
							 perf_metrics_ctx.collection_start_time));
	}
}

/* Export symbols */
EXPORT_SYMBOL(ai_record_performance_metric);
EXPORT_SYMBOL(ai_get_performance_stats);
EXPORT_SYMBOL(ai_get_system_performance_score);
EXPORT_SYMBOL(ai_performance_metrics_get_statistics);