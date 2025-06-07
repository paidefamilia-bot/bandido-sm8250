/*
 * AI Scheduler Feature Extraction System
 * Advanced feature engineering for machine learning
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/string.h>
#include <linux/ctype.h>
#include <linux/math64.h>
#include <linux/sort.h>

#include "ai_scheduler.h"

/* Feature normalization constants */
#define AI_FEATURE_SCALE_FACTOR		1000
#define AI_FEATURE_MAX_VALUE		100000
#define AI_CONFIDENCE_THRESHOLD		70

/* Gaming app patterns */
static const char *gaming_patterns[] = {
	"unity",
	"unreal",
	"game",
	"pubg",
	"fortnite",
	"minecraft",
	"roblox",
	"clash",
	"candy",
	"angry",
	"temple",
	"subway",
	"pokemon",
	"mario",
	"sonic",
	NULL
};

/* System critical patterns */
static const char *system_patterns[] = {
	"system_server",
	"surfaceflinger",
	"audioserver",
	"cameraserver",
	"mediaserver",
	"netd",
	"vold",
	"installd",
	"drmserver",
	"keystore",
	NULL
};

/* Interactive app patterns */
static const char *interactive_patterns[] = {
	"launcher",
	"keyboard",
	"camera",
	"gallery",
	"browser",
	"chrome",
	"firefox",
	"messenger",
	"whatsapp",
	"telegram",
	"instagram",
	"facebook",
	"twitter",
	"youtube",
	"spotify",
	"music",
	NULL
};

/**
 * ai_normalize_feature - Normalize a feature value to 0-100 range
 * @value: Raw feature value
 * @max_expected: Maximum expected value for this feature
 * 
 * Returns: Normalized value (0-100)
 */
static u32 ai_normalize_feature(u64 value, u64 max_expected)
{
	if (max_expected == 0)
		return 0;
	
	if (value >= max_expected)
		return 100;
	
	return div64_u64(value * 100, max_expected);
}

/**
 * ai_calculate_moving_average - Calculate moving average
 * @current: Current value
 * @previous: Previous average
 * @weight: Weight for new value (0-100)
 * 
 * Returns: New moving average
 */
static u32 ai_calculate_moving_average(u32 current, u32 previous, u32 weight)
{
	if (weight > 100)
		weight = 100;
	
	return (current * weight + previous * (100 - weight)) / 100;
}

/**
 * ai_detect_pattern_in_name - Check if task name matches patterns
 * @comm: Task command name
 * @patterns: Array of patterns to match
 * 
 * Returns: true if pattern found, false otherwise
 */
static bool ai_detect_pattern_in_name(const char *comm, const char **patterns)
{
	int i;
	char lower_comm[TASK_COMM_LEN];
	char *p;
	
	/* Convert to lowercase for case-insensitive matching */
	strncpy(lower_comm, comm, TASK_COMM_LEN - 1);
	lower_comm[TASK_COMM_LEN - 1] = '\0';
	
	for (p = lower_comm; *p; p++)
		*p = tolower(*p);
	
	/* Check each pattern */
	for (i = 0; patterns[i]; i++) {
		if (strstr(lower_comm, patterns[i]))
			return true;
	}
	
	return false;
}

/**
 * ai_is_gaming_task - Detect if task is a game
 * @task: Target task
 * 
 * Returns: true if task appears to be a game
 */
bool ai_is_gaming_task(struct task_struct *task)
{
	if (!task)
		return false;
	
	return ai_detect_pattern_in_name(task->comm, gaming_patterns);
}

/**
 * ai_is_interactive_task - Detect if task is interactive
 * @task: Target task
 * 
 * Returns: true if task appears to be interactive
 */
bool ai_is_interactive_task(struct task_struct *task)
{
	if (!task)
		return false;
	
	/* Check for interactive patterns */
	if (ai_detect_pattern_in_name(task->comm, interactive_patterns))
		return true;
	
	/* Check if it's a foreground app */
	if (task->signal && task->signal->oom_score_adj <= 0)
		return true;
	
	/* Check for Android app pattern */
	if (strstr(task->comm, "android.") || strstr(task->comm, "com."))
		return true;
	
	return false;
}

/**
 * ai_is_system_critical_task - Detect if task is system critical
 * @task: Target task
 * 
 * Returns: true if task is system critical
 */
static bool ai_is_system_critical_task(struct task_struct *task)
{
	if (!task)
		return false;
	
	return ai_detect_pattern_in_name(task->comm, system_patterns);
}

/**
 * ai_calculate_cpu_intensity - Calculate CPU intensity score
 * @features: Task features
 * 
 * Returns: CPU intensity score (0-100)
 */
static u32 ai_calculate_cpu_intensity(const struct ai_task_features *features)
{
	u32 intensity = 0;
	
	/* Base CPU usage */
	intensity += features->cpu_usage_avg;
	
	/* Burst frequency impact */
	intensity += ai_normalize_feature(features->cpu_burst_frequency, 1000) / 4;
	
	/* Context switch impact */
	intensity += ai_normalize_feature(features->context_switches, 10000) / 4;
	
	/* Involuntary switches indicate high CPU demand */
	intensity += ai_normalize_feature(features->involuntary_switches, 1000) / 2;
	
	return min(intensity, 100U);
}

/**
 * ai_calculate_memory_intensity - Calculate memory intensity score
 * @features: Task features
 * 
 * Returns: Memory intensity score (0-100)
 */
static u32 ai_calculate_memory_intensity(const struct ai_task_features *features)
{
	u32 intensity = 0;
	
	/* Memory usage (normalize to 1GB max) */
	intensity += ai_normalize_feature(features->memory_usage, 1024 * 1024);
	
	/* Page fault rate */
	intensity += ai_normalize_feature(features->page_fault_rate, 1000) / 2;
	
	/* Memory allocation rate */
	intensity += ai_normalize_feature(features->memory_allocation_rate, 10000) / 4;
	
	return min(intensity, 100U);
}

/**
 * ai_calculate_io_intensity - Calculate I/O intensity score
 * @features: Task features
 * 
 * Returns: I/O intensity score (0-100)
 */
static u32 ai_calculate_io_intensity(const struct ai_task_features *features)
{
	u32 intensity = 0;
	
	/* Read/write rates (normalize to 100MB/s max) */
	intensity += ai_normalize_feature(features->io_read_rate, 100 * 1024) / 2;
	intensity += ai_normalize_feature(features->io_write_rate, 100 * 1024) / 2;
	
	/* I/O wait time impact */
	intensity += ai_normalize_feature(features->io_wait_time, 1000000) / 2;
	
	/* Network activity */
	intensity += ai_normalize_feature(features->network_activity, 10000) / 4;
	
	return min(intensity, 100U);
}

/**
 * ai_calculate_interactivity_score - Calculate interactivity score
 * @features: Task features
 * 
 * Returns: Interactivity score (0-100)
 */
static u32 ai_calculate_interactivity_score(const struct ai_task_features *features)
{
	u32 score = 0;
	
	/* Base user interaction score */
	score += features->user_interaction_score;
	
	/* Foreground time bonus */
	if (features->foreground_time > features->background_time)
		score += 20;
	
	/* Touch events bonus */
	score += ai_normalize_feature(features->touch_events, 1000) / 5;
	
	/* Response time penalty (lower is better) */
	if (features->response_time_avg > 0) {
		u32 penalty = ai_normalize_feature(features->response_time_avg, 100000) / 4;
		score = (score > penalty) ? score - penalty : 0;
	}
	
	return min(score, 100U);
}

/**
 * ai_calculate_power_efficiency - Calculate power efficiency score
 * @features: Task features
 * 
 * Returns: Power efficiency score (0-100, higher is more efficient)
 */
static u32 ai_calculate_power_efficiency(const struct ai_task_features *features)
{
	u32 efficiency = 100;
	
	/* CPU usage penalty */
	efficiency -= features->cpu_usage_avg / 2;
	
	/* Thermal impact penalty */
	efficiency -= features->thermal_impact;
	
	/* Power consumption penalty */
	efficiency -= ai_normalize_feature(features->power_consumption, 1000) / 2;
	
	/* I/O efficiency bonus */
	if (features->io_wait_time < 1000)
		efficiency += 5;
	
	return max(efficiency, 0U);
}

/**
 * ai_extract_advanced_features - Extract advanced derived features
 * @features: Task features to enhance
 */
static void ai_extract_advanced_features(struct ai_task_features *features)
{
	u32 cpu_intensity, memory_intensity, io_intensity;
	u32 interactivity, power_efficiency;
	
	/* Calculate intensity scores */
	cpu_intensity = ai_calculate_cpu_intensity(features);
	memory_intensity = ai_calculate_memory_intensity(features);
	io_intensity = ai_calculate_io_intensity(features);
	interactivity = ai_calculate_interactivity_score(features);
	power_efficiency = ai_calculate_power_efficiency(features);
	
	/* Update system impact based on calculated intensities */
	features->system_load_impact = (cpu_intensity + memory_intensity + io_intensity) / 3;
	
	/* Update thermal impact */
	features->thermal_impact = (cpu_intensity * 60 + memory_intensity * 20 + 
				    io_intensity * 20) / 100;
	
	/* Update power consumption */
	features->power_consumption = (100 - power_efficiency) * 10;
	
	/* Update battery drain rate */
	features->battery_drain_rate = features->power_consumption / 10;
	
	/* Store derived features for classification */
	features->user_interaction_score = interactivity;
	
	ai_verbose("Advanced features: cpu_int=%u, mem_int=%u, io_int=%u, interact=%u, power_eff=%u",
		   cpu_intensity, memory_intensity, io_intensity, 
		   interactivity, power_efficiency);
}

/**
 * ai_classify_task_type - Classify task type based on features
 * @features: Task features
 * 
 * Returns: Predicted task type
 */
static enum ai_task_type ai_classify_task_type(const struct ai_task_features *features)
{
	u32 cpu_score = ai_calculate_cpu_intensity(features);
	u32 memory_score = ai_calculate_memory_intensity(features);
	u32 io_score = ai_calculate_io_intensity(features);
	u32 interact_score = features->user_interaction_score;
	
	/* Gaming detection */
	if (interact_score >= 80 && cpu_score >= 60) {
		return AI_TASK_GAMING;
	}
	
	/* Interactive apps */
	if (interact_score >= 60) {
		return AI_TASK_INTERACTIVE;
	}
	
	/* Compute intensive */
	if (cpu_score >= 70 && memory_score >= 50) {
		return AI_TASK_COMPUTE_INTENSIVE;
	}
	
	/* I/O intensive */
	if (io_score >= 60) {
		return AI_TASK_IO_INTENSIVE;
	}
	
	/* Multimedia */
	if (cpu_score >= 40 && memory_score >= 40 && interact_score >= 30) {
		return AI_TASK_MULTIMEDIA;
	}
	
	/* Background tasks */
	if (interact_score <= 30) {
		return AI_TASK_BACKGROUND;
	}
	
	return AI_TASK_UNKNOWN;
}

/**
 * ai_calculate_confidence - Calculate prediction confidence
 * @features: Task features
 * 
 * Returns: Confidence score (0-100)
 */
u32 ai_calculate_confidence(const struct ai_task_features *features)
{
	u32 confidence = 50;  /* Base confidence */
	
	/* Higher confidence for clear patterns */
	if (features->user_interaction_score >= 80 || 
	    features->user_interaction_score <= 20) {
		confidence += 20;
	}
	
	/* CPU usage patterns */
	if (features->cpu_usage_avg >= 70 || features->cpu_usage_avg <= 10) {
		confidence += 15;
	}
	
	/* Memory usage patterns */
	if (features->memory_usage >= 500000 || features->memory_usage <= 10000) {
		confidence += 10;
	}
	
	/* I/O patterns */
	if (features->io_read_rate + features->io_write_rate >= 10000) {
		confidence += 10;
	}
	
	/* Consistent behavior increases confidence */
	if (features->voluntary_switches > features->involuntary_switches * 2) {
		confidence += 5;
	}
	
	return min(confidence, 100U);
}

/**
 * ai_calculate_priority - Calculate task priority score
 * @features: Task features
 * 
 * Returns: Priority score (0-100, higher is more important)
 */
u32 ai_calculate_priority(const struct ai_task_features *features)
{
	u32 priority = 50;  /* Base priority */
	
	/* Interactive tasks get higher priority */
	priority += features->user_interaction_score / 4;
	
	/* Gaming tasks get highest priority */
	if (features->predicted_type == AI_TASK_GAMING) {
		priority += 30;
	}
	
	/* System critical tasks */
	if (features->predicted_type == AI_TASK_SYSTEM_CRITICAL) {
		priority += 25;
	}
	
	/* Foreground bonus */
	if (features->foreground_time > features->background_time) {
		priority += 15;
	}
	
	/* Response time penalty */
	if (features->response_time_avg > 50000) {
		priority -= 10;
	}
	
	/* Power efficiency bonus */
	if (features->power_consumption < 100) {
		priority += 5;
	}
	
	return min(priority, 100U);
}

/**
 * ai_recommend_performance_level - Recommend performance level
 * @features: Task features
 * 
 * Returns: Recommended performance level
 */
enum ai_performance_level ai_recommend_performance_level(
	const struct ai_task_features *features)
{
	u32 cpu_intensity = ai_calculate_cpu_intensity(features);
	u32 interact_score = features->user_interaction_score;
	
	/* Gaming performance */
	if (features->predicted_type == AI_TASK_GAMING) {
		return AI_PERF_GAMING;
	}
	
	/* High performance for intensive tasks */
	if (cpu_intensity >= 70 || interact_score >= 80) {
		return AI_PERF_PERFORMANCE;
	}
	
	/* Balanced for moderate tasks */
	if (cpu_intensity >= 30 || interact_score >= 40) {
		return AI_PERF_BALANCED;
	}
	
	/* Power save for background tasks */
	return AI_PERF_POWERSAVE;
}

/**
 * ai_enhance_features_with_history - Enhance features using historical data
 * @data: Task data with history
 * @features: Current features to enhance
 */
static void ai_enhance_features_with_history(struct ai_task_data *data,
					     struct ai_task_features *features)
{
	struct ai_task_history_entry *entry;
	u32 history_count = 0;
	u64 avg_cpu = 0, avg_memory = 0, avg_interaction = 0;
	
	if (!data || list_empty(&data->history))
		return;
	
	/* Calculate averages from history */
	spin_lock(&data->history_lock);
	list_for_each_entry(entry, &data->history, list) {
		avg_cpu += entry->features.cpu_usage_avg;
		avg_memory += entry->features.memory_usage;
		avg_interaction += entry->features.user_interaction_score;
		history_count++;
		
		if (history_count >= 10)  /* Limit to recent history */
			break;
	}
	spin_unlock(&data->history_lock);
	
	if (history_count > 0) {
		avg_cpu /= history_count;
		avg_memory /= history_count;
		avg_interaction /= history_count;
		
		/* Smooth current values with historical averages */
		features->cpu_usage_avg = ai_calculate_moving_average(
			features->cpu_usage_avg, avg_cpu, 70);
		features->memory_usage = ai_calculate_moving_average(
			features->memory_usage, avg_memory, 70);
		features->user_interaction_score = ai_calculate_moving_average(
			features->user_interaction_score, avg_interaction, 70);
		
		ai_verbose("Enhanced features with history: cpu=%u->%u, mem=%u->%u",
			   (u32)avg_cpu, features->cpu_usage_avg,
			   (u32)avg_memory, features->memory_usage);
	}
}

/**
 * ai_predict_task_type - Predict task type using enhanced features
 * @features: Task features
 * 
 * Returns: Predicted task type
 */
enum ai_task_type ai_predict_task_type(const struct ai_task_features *features)
{
	enum ai_task_type predicted_type;
	
	if (!features)
		return AI_TASK_UNKNOWN;
	
	/* Use rule-based classification for now */
	predicted_type = ai_classify_task_type(features);
	
	ai_verbose("Predicted task type: %d (confidence: %u)",
		   predicted_type, ai_calculate_confidence(features));
	
	return predicted_type;
}

/**
 * ai_enhance_task_features - Enhance features with advanced analysis
 * @data: Task data
 * @features: Features to enhance
 */
void ai_enhance_task_features(struct ai_task_data *data,
			      struct ai_task_features *features)
{
	if (!features)
		return;
	
	/* Extract advanced derived features */
	ai_extract_advanced_features(features);
	
	/* Enhance with historical data if available */
	if (data) {
		ai_enhance_features_with_history(data, features);
	}
	
	/* Classify task type */
	features->predicted_type = ai_predict_task_type(features);
	
	/* Calculate confidence and priority */
	features->confidence_score = ai_calculate_confidence(features);
	features->priority_score = ai_calculate_priority(features);
	
	/* Recommend performance level */
	features->performance_requirement = ai_recommend_performance_level(features);
	
	ai_verbose("Enhanced features: type=%d, confidence=%u, priority=%u, perf=%d",
		   features->predicted_type, features->confidence_score,
		   features->priority_score, features->performance_requirement);
}

/* Export symbols */
EXPORT_SYMBOL(ai_is_gaming_task);
EXPORT_SYMBOL(ai_is_interactive_task);
EXPORT_SYMBOL(ai_predict_task_type);
EXPORT_SYMBOL(ai_calculate_confidence);
EXPORT_SYMBOL(ai_calculate_priority);
EXPORT_SYMBOL(ai_recommend_performance_level);