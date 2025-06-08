/*
 * AI Scheduler GPU Workload Detection
 * Intelligent GPU workload classification and optimization
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/string.h>
#include <linux/hash.h>

#include "ai_scheduler.h"

/* GPU workload types */
enum ai_gpu_workload_type {
	AI_GPU_WORKLOAD_GAMING = 0,		/* Gaming workload */
	AI_GPU_WORKLOAD_COMPUTE,		/* Compute workload */
	AI_GPU_WORKLOAD_RENDERING,		/* 3D rendering */
	AI_GPU_WORKLOAD_VIDEO_DECODE,		/* Video decoding */
	AI_GPU_WORKLOAD_VIDEO_ENCODE,		/* Video encoding */
	AI_GPU_WORKLOAD_CAMERA,			/* Camera processing */
	AI_GPU_WORKLOAD_ML_INFERENCE,		/* ML inference */
	AI_GPU_WORKLOAD_ML_TRAINING,		/* ML training */
	AI_GPU_WORKLOAD_IMAGE_PROCESSING,	/* Image processing */
	AI_GPU_WORKLOAD_CRYPTO,			/* Cryptographic operations */
	AI_GPU_WORKLOAD_MIXED,			/* Mixed workload */
	AI_GPU_WORKLOAD_IDLE			/* Idle/low activity */
};

/* GPU workload characteristics */
struct ai_gpu_workload_profile {
	enum ai_gpu_workload_type type;
	u32 compute_intensity;		/* Compute intensity (0-1000) */
	u32 memory_bandwidth_usage;	/* Memory bandwidth usage (0-1000) */
	u32 texture_usage;		/* Texture unit usage (0-1000) */
	u32 shader_complexity;		/* Shader complexity (0-1000) */
	u32 frame_rate_target;		/* Target frame rate (FPS) */
	u32 power_efficiency_priority;	/* Power efficiency priority (0-1000) */
	u32 latency_sensitivity;	/* Latency sensitivity (0-1000) */
	u32 thermal_sensitivity;	/* Thermal sensitivity (0-1000) */
	u32 duration_ms;		/* Typical duration in ms */
	u32 frequency_requirement;	/* Frequency requirement (0-1000) */
};

/* GPU workload sample */
struct ai_gpu_workload_sample {
	u64 timestamp;			/* Sample timestamp */
	u32 gpu_utilization;		/* GPU utilization (0-1000) */
	u32 memory_utilization;		/* Memory utilization (0-1000) */
	u32 frequency;			/* GPU frequency */
	u32 power_consumption;		/* Power consumption */
	u32 temperature;		/* GPU temperature */
	u32 frame_rate;			/* Current frame rate */
	u32 draw_calls;			/* Number of draw calls */
	u32 compute_dispatches;		/* Number of compute dispatches */
	u32 memory_transfers;		/* Memory transfer count */
	u32 shader_instructions;	/* Shader instruction count */
	char process_name[32];		/* Process name */
	pid_t pid;			/* Process ID */
	struct list_head list;
};

/* GPU workload detection context */
struct ai_gpu_workload_ctx {
	/* Workload profiles */
	struct ai_gpu_workload_profile profiles[AI_GPU_WORKLOAD_IDLE + 1];
	
	/* Current workload state */
	enum ai_gpu_workload_type current_workload;
	enum ai_gpu_workload_type predicted_workload;
	u32 workload_confidence;	/* Detection confidence (0-1000) */
	u64 workload_start_time;	/* Current workload start time */
	u32 workload_duration;		/* Current workload duration */
	
	/* Sample history */
	struct list_head samples;
	spinlock_t samples_lock;
	u32 sample_count;
	u32 max_samples;
	
	/* Detection algorithm */
	u32 detection_window_ms;	/* Detection window size */
	u32 confidence_threshold;	/* Minimum confidence for classification */
	u32 stability_threshold;	/* Stability threshold for workload changes */
	
	/* Workload detection worker */
	struct delayed_work detection_work;
	struct workqueue_struct *detection_wq;
	bool detection_active;
	u32 detection_interval_ms;
	
	/* Statistics */
	atomic64_t workload_detections;
	atomic64_t workload_changes;
	atomic64_t samples_processed;
	u64 detection_accuracy;
	
	/* Per-workload statistics */
	struct {
		u64 total_time_ns;
		u64 detection_count;
		u32 average_duration_ms;
		u32 average_utilization;
		u32 average_power;
	} workload_stats[AI_GPU_WORKLOAD_IDLE + 1];
	
	/* Configuration */
	bool adaptive_detection_enabled;
	bool workload_prediction_enabled;
	bool thermal_aware_detection;
	bool power_aware_detection;
};

static struct ai_gpu_workload_ctx workload_ctx;

/* Workload type names */
static const char *workload_type_names[] = {
	"GAMING",
	"COMPUTE",
	"RENDERING",
	"VIDEO_DECODE",
	"VIDEO_ENCODE",
	"CAMERA",
	"ML_INFERENCE",
	"ML_TRAINING",
	"IMAGE_PROCESSING",
	"CRYPTO",
	"MIXED",
	"IDLE"
};

/**
 * ai_gpu_init_workload_profiles - Initialize workload profiles
 */
static void ai_gpu_init_workload_profiles(void)
{
	/* Gaming workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_GAMING] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_GAMING,
		.compute_intensity = 600,
		.memory_bandwidth_usage = 700,
		.texture_usage = 800,
		.shader_complexity = 700,
		.frame_rate_target = 60,
		.power_efficiency_priority = 300,
		.latency_sensitivity = 900,
		.thermal_sensitivity = 600,
		.duration_ms = 30000,		/* 30 seconds typical */
		.frequency_requirement = 800
	};
	
	/* Compute workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_COMPUTE] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_COMPUTE,
		.compute_intensity = 900,
		.memory_bandwidth_usage = 800,
		.texture_usage = 100,
		.shader_complexity = 800,
		.frame_rate_target = 0,		/* No frame rate target */
		.power_efficiency_priority = 500,
		.latency_sensitivity = 400,
		.thermal_sensitivity = 700,
		.duration_ms = 5000,		/* 5 seconds typical */
		.frequency_requirement = 900
	};
	
	/* 3D Rendering workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_RENDERING] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_RENDERING,
		.compute_intensity = 500,
		.memory_bandwidth_usage = 600,
		.texture_usage = 900,
		.shader_complexity = 800,
		.frame_rate_target = 30,
		.power_efficiency_priority = 400,
		.latency_sensitivity = 600,
		.thermal_sensitivity = 500,
		.duration_ms = 10000,		/* 10 seconds typical */
		.frequency_requirement = 700
	};
	
	/* Video decode workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_VIDEO_DECODE] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_VIDEO_DECODE,
		.compute_intensity = 300,
		.memory_bandwidth_usage = 800,
		.texture_usage = 200,
		.shader_complexity = 300,
		.frame_rate_target = 30,
		.power_efficiency_priority = 800,
		.latency_sensitivity = 700,
		.thermal_sensitivity = 400,
		.duration_ms = 60000,		/* 1 minute typical */
		.frequency_requirement = 400
	};
	
	/* Video encode workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_VIDEO_ENCODE] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_VIDEO_ENCODE,
		.compute_intensity = 700,
		.memory_bandwidth_usage = 900,
		.texture_usage = 200,
		.shader_complexity = 600,
		.frame_rate_target = 30,
		.power_efficiency_priority = 600,
		.latency_sensitivity = 500,
		.thermal_sensitivity = 800,
		.duration_ms = 120000,		/* 2 minutes typical */
		.frequency_requirement = 700
	};
	
	/* Camera processing workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_CAMERA] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_CAMERA,
		.compute_intensity = 400,
		.memory_bandwidth_usage = 700,
		.texture_usage = 300,
		.shader_complexity = 500,
		.frame_rate_target = 30,
		.power_efficiency_priority = 700,
		.latency_sensitivity = 800,
		.thermal_sensitivity = 600,
		.duration_ms = 15000,		/* 15 seconds typical */
		.frequency_requirement = 500
	};
	
	/* ML inference workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_ML_INFERENCE] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_ML_INFERENCE,
		.compute_intensity = 800,
		.memory_bandwidth_usage = 600,
		.texture_usage = 100,
		.shader_complexity = 700,
		.frame_rate_target = 0,
		.power_efficiency_priority = 600,
		.latency_sensitivity = 900,
		.thermal_sensitivity = 500,
		.duration_ms = 1000,		/* 1 second typical */
		.frequency_requirement = 800
	};
	
	/* ML training workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_ML_TRAINING] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_ML_TRAINING,
		.compute_intensity = 950,
		.memory_bandwidth_usage = 900,
		.texture_usage = 100,
		.shader_complexity = 900,
		.frame_rate_target = 0,
		.power_efficiency_priority = 300,
		.latency_sensitivity = 200,
		.thermal_sensitivity = 900,
		.duration_ms = 300000,		/* 5 minutes typical */
		.frequency_requirement = 1000
	};
	
	/* Image processing workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_IMAGE_PROCESSING] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_IMAGE_PROCESSING,
		.compute_intensity = 600,
		.memory_bandwidth_usage = 800,
		.texture_usage = 600,
		.shader_complexity = 600,
		.frame_rate_target = 0,
		.power_efficiency_priority = 500,
		.latency_sensitivity = 600,
		.thermal_sensitivity = 500,
		.duration_ms = 3000,		/* 3 seconds typical */
		.frequency_requirement = 600
	};
	
	/* Crypto workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_CRYPTO] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_CRYPTO,
		.compute_intensity = 900,
		.memory_bandwidth_usage = 400,
		.texture_usage = 100,
		.shader_complexity = 800,
		.frame_rate_target = 0,
		.power_efficiency_priority = 400,
		.latency_sensitivity = 300,
		.thermal_sensitivity = 800,
		.duration_ms = 10000,		/* 10 seconds typical */
		.frequency_requirement = 800
	};
	
	/* Mixed workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_MIXED] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_MIXED,
		.compute_intensity = 500,
		.memory_bandwidth_usage = 500,
		.texture_usage = 400,
		.shader_complexity = 500,
		.frame_rate_target = 30,
		.power_efficiency_priority = 500,
		.latency_sensitivity = 500,
		.thermal_sensitivity = 500,
		.duration_ms = 20000,		/* 20 seconds typical */
		.frequency_requirement = 600
	};
	
	/* Idle workload profile */
	workload_ctx.profiles[AI_GPU_WORKLOAD_IDLE] = (struct ai_gpu_workload_profile) {
		.type = AI_GPU_WORKLOAD_IDLE,
		.compute_intensity = 50,
		.memory_bandwidth_usage = 100,
		.texture_usage = 50,
		.shader_complexity = 100,
		.frame_rate_target = 0,
		.power_efficiency_priority = 1000,
		.latency_sensitivity = 100,
		.thermal_sensitivity = 100,
		.duration_ms = 0,		/* Variable */
		.frequency_requirement = 200
	};
}

/**
 * ai_gpu_calculate_workload_similarity - Calculate similarity between sample and profile
 * @sample: GPU workload sample
 * @profile: Workload profile
 * 
 * Returns: Similarity score (0-1000)
 */
static u32 ai_gpu_calculate_workload_similarity(const struct ai_gpu_workload_sample *sample,
						const struct ai_gpu_workload_profile *profile)
{
	u32 utilization_score, memory_score, frequency_score, total_score;
	u32 utilization_diff, memory_diff, frequency_diff;
	
	/* Calculate utilization similarity */
	utilization_diff = abs((int)sample->gpu_utilization - (int)profile->compute_intensity);
	utilization_score = (utilization_diff < 1000) ? (1000 - utilization_diff) : 0;
	
	/* Calculate memory similarity */
	memory_diff = abs((int)sample->memory_utilization - (int)profile->memory_bandwidth_usage);
	memory_score = (memory_diff < 1000) ? (1000 - memory_diff) : 0;
	
	/* Calculate frequency similarity (normalized) */
	u32 normalized_freq = (sample->frequency * 1000) / AI_GPU_MAX_FREQUENCY;
	frequency_diff = abs((int)normalized_freq - (int)profile->frequency_requirement);
	frequency_score = (frequency_diff < 1000) ? (1000 - frequency_diff) : 0;
	
	/* Weighted average */
	total_score = (utilization_score * 40 + memory_score * 30 + frequency_score * 30) / 100;
	
	return total_score;
}

/**
 * ai_gpu_classify_workload - Classify GPU workload based on sample
 * @sample: GPU workload sample
 * @confidence: Output confidence score
 * 
 * Returns: Detected workload type
 */
static enum ai_gpu_workload_type ai_gpu_classify_workload(const struct ai_gpu_workload_sample *sample,
							  u32 *confidence)
{
	enum ai_gpu_workload_type best_type = AI_GPU_WORKLOAD_IDLE;
	u32 best_score = 0;
	u32 i;
	
	/* Check against all profiles */
	for (i = 0; i <= AI_GPU_WORKLOAD_IDLE; i++) {
		u32 score = ai_gpu_calculate_workload_similarity(sample, &workload_ctx.profiles[i]);
		
		if (score > best_score) {
			best_score = score;
			best_type = i;
		}
	}
	
	/* Special case detection based on process name */
	if (sample->process_name[0]) {
		if (strstr(sample->process_name, "game") || strstr(sample->process_name, "unity") ||
		    strstr(sample->process_name, "unreal")) {
			if (sample->gpu_utilization > 500) {
				best_type = AI_GPU_WORKLOAD_GAMING;
				best_score = max(best_score, 800U);
			}
		} else if (strstr(sample->process_name, "camera") || strstr(sample->process_name, "cam")) {
			best_type = AI_GPU_WORKLOAD_CAMERA;
			best_score = max(best_score, 750U);
		} else if (strstr(sample->process_name, "video") || strstr(sample->process_name, "media")) {
			if (sample->memory_utilization > 600) {
				best_type = AI_GPU_WORKLOAD_VIDEO_DECODE;
				best_score = max(best_score, 700U);
			}
		}
	}
	
	/* Low utilization detection */
	if (sample->gpu_utilization < 100 && sample->memory_utilization < 200) {
		best_type = AI_GPU_WORKLOAD_IDLE;
		best_score = 900;
	}
	
	if (confidence)
		*confidence = best_score;
	
	return best_type;
}

/**
 * ai_gpu_add_workload_sample - Add a new workload sample
 * @gpu_util: GPU utilization (0-1000)
 * @mem_util: Memory utilization (0-1000)
 * @frequency: GPU frequency in Hz
 * @power: Power consumption in mW
 * @temperature: GPU temperature in mC
 * @process_name: Process name (optional)
 * @pid: Process ID
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_gpu_add_workload_sample(u32 gpu_util, u32 mem_util, u32 frequency,
			       u32 power, u32 temperature, const char *process_name, pid_t pid)
{
	struct ai_gpu_workload_sample *sample;
	
	sample = kzalloc(sizeof(*sample), GFP_ATOMIC);
	if (!sample)
		return -ENOMEM;
	
	sample->timestamp = ktime_get_ns();
	sample->gpu_utilization = gpu_util;
	sample->memory_utilization = mem_util;
	sample->frequency = frequency;
	sample->power_consumption = power;
	sample->temperature = temperature;
	sample->pid = pid;
	
	if (process_name) {
		strncpy(sample->process_name, process_name, sizeof(sample->process_name) - 1);
		sample->process_name[sizeof(sample->process_name) - 1] = '\0';
	}
	
	/* Add to samples list */
	spin_lock(&workload_ctx.samples_lock);
	
	/* Remove oldest sample if limit reached */
	if (workload_ctx.sample_count >= workload_ctx.max_samples) {
		struct ai_gpu_workload_sample *oldest = 
			list_last_entry(&workload_ctx.samples,
					struct ai_gpu_workload_sample, list);
		list_del(&oldest->list);
		kfree(oldest);
		workload_ctx.sample_count--;
	}
	
	list_add(&sample->list, &workload_ctx.samples);
	workload_ctx.sample_count++;
	
	spin_unlock(&workload_ctx.samples_lock);
	
	atomic64_inc(&workload_ctx.samples_processed);
	
	ai_verbose("GPU workload sample added: util=%u%%, mem=%u%%, freq=%u MHz, process=%s",
		   gpu_util / 10, mem_util / 10, frequency / 1000000, 
		   process_name ? process_name : "unknown");
	
	return 0;
}

/**
 * ai_gpu_workload_detection_worker - Background workload detection worker
 * @work: Work structure
 */
static void ai_gpu_workload_detection_worker(struct work_struct *work)
{
	struct ai_gpu_workload_sample *sample;
	enum ai_gpu_workload_type detected_type;
	u32 confidence, sample_count = 0;
	u64 total_utilization = 0, total_memory = 0;
	ktime_t start_time, end_time;
	
	if (!workload_ctx.detection_active)
		return;
	
	start_time = ktime_get();
	
	/* Analyze recent samples */
	spin_lock(&workload_ctx.samples_lock);
	list_for_each_entry(sample, &workload_ctx.samples, list) {
		/* Only analyze samples from the detection window */
		u64 age_ns = ktime_get_ns() - sample->timestamp;
		if (age_ns > workload_ctx.detection_window_ms * 1000000ULL)
			break;
		
		total_utilization += sample->gpu_utilization;
		total_memory += sample->memory_utilization;
		sample_count++;
		
		if (sample_count >= 10) /* Limit analysis */
			break;
	}
	spin_unlock(&workload_ctx.samples_lock);
	
	if (sample_count == 0) {
		/* No recent samples, assume idle */
		detected_type = AI_GPU_WORKLOAD_IDLE;
		confidence = 800;
	} else {
		/* Create average sample for classification */
		struct ai_gpu_workload_sample avg_sample = {0};
		avg_sample.gpu_utilization = total_utilization / sample_count;
		avg_sample.memory_utilization = total_memory / sample_count;
		avg_sample.frequency = AI_GPU_MIN_FREQUENCY; /* Default */
		
		/* Get the most recent sample for additional context */
		spin_lock(&workload_ctx.samples_lock);
		if (!list_empty(&workload_ctx.samples)) {
			sample = list_first_entry(&workload_ctx.samples,
						  struct ai_gpu_workload_sample, list);
			avg_sample.frequency = sample->frequency;
			avg_sample.frame_rate = sample->frame_rate;
			strncpy(avg_sample.process_name, sample->process_name,
				sizeof(avg_sample.process_name) - 1);
		}
		spin_unlock(&workload_ctx.samples_lock);
		
		detected_type = ai_gpu_classify_workload(&avg_sample, &confidence);
	}
	
	/* Update workload state if confidence is high enough */
	if (confidence >= workload_ctx.confidence_threshold) {
		if (detected_type != workload_ctx.current_workload) {
			/* Workload change detected */
			enum ai_gpu_workload_type prev_workload = workload_ctx.current_workload;
			u64 current_time = ktime_get_ns();
			
			/* Update statistics for previous workload */
			if (workload_ctx.workload_start_time > 0) {
				u64 duration_ns = current_time - workload_ctx.workload_start_time;
				workload_ctx.workload_stats[prev_workload].total_time_ns += duration_ns;
				workload_ctx.workload_duration = duration_ns / 1000000; /* Convert to ms */
			}
			
			workload_ctx.current_workload = detected_type;
			workload_ctx.workload_confidence = confidence;
			workload_ctx.workload_start_time = current_time;
			workload_ctx.workload_stats[detected_type].detection_count++;
			
			atomic64_inc(&workload_ctx.workload_changes);
			
			ai_info("GPU workload changed: %s -> %s (confidence: %u%%)",
				workload_type_names[prev_workload],
				workload_type_names[detected_type],
				confidence / 10);
		} else {
			/* Same workload, update confidence */
			workload_ctx.workload_confidence = 
				(workload_ctx.workload_confidence + confidence) / 2;
		}
	}
	
	atomic64_inc(&workload_ctx.workload_detections);
	
	end_time = ktime_get();
	u64 detection_time = ktime_to_ns(ktime_sub(end_time, start_time));
	
	ai_verbose("GPU workload detection: type=%s, confidence=%u%%, samples=%u, time=%llu ns",
		   workload_type_names[workload_ctx.current_workload],
		   workload_ctx.workload_confidence / 10, sample_count, detection_time);
	
	/* Schedule next detection */
	if (workload_ctx.detection_active) {
		queue_delayed_work(workload_ctx.detection_wq, &workload_ctx.detection_work,
				   msecs_to_jiffies(workload_ctx.detection_interval_ms));
	}
}

/**
 * ai_gpu_get_current_workload - Get current GPU workload type
 * @confidence: Output confidence score (optional)
 * 
 * Returns: Current workload type
 */
enum ai_gpu_workload_type ai_gpu_get_current_workload(u32 *confidence)
{
	if (confidence)
		*confidence = workload_ctx.workload_confidence;
	
	return workload_ctx.current_workload;
}

/**
 * ai_gpu_get_workload_profile - Get workload profile for a specific type
 * @type: Workload type
 * 
 * Returns: Workload profile or NULL if invalid type
 */
const struct ai_gpu_workload_profile *ai_gpu_get_workload_profile(enum ai_gpu_workload_type type)
{
	if (type > AI_GPU_WORKLOAD_IDLE)
		return NULL;
	
	return &workload_ctx.profiles[type];
}

/**
 * ai_gpu_workload_detection_init - Initialize GPU workload detection
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_gpu_workload_detection_init(void)
{
	/* Initialize context */
	memset(&workload_ctx, 0, sizeof(workload_ctx));
	
	INIT_LIST_HEAD(&workload_ctx.samples);
	spin_lock_init(&workload_ctx.samples_lock);
	
	workload_ctx.current_workload = AI_GPU_WORKLOAD_IDLE;
	workload_ctx.predicted_workload = AI_GPU_WORKLOAD_IDLE;
	workload_ctx.workload_confidence = 1000;
	workload_ctx.max_samples = 100;
	workload_ctx.detection_window_ms = 2000;	/* 2 second window */
	workload_ctx.confidence_threshold = 600;	/* 60% confidence */
	workload_ctx.stability_threshold = 800;		/* 80% stability */
	workload_ctx.detection_interval_ms = 1000;	/* 1 second interval */
	
	/* Initialize workload profiles */
	ai_gpu_init_workload_profiles();
	
	/* Create detection work queue */
	workload_ctx.detection_wq = create_singlethread_workqueue("ai_gpu_workload_detection");
	if (!workload_ctx.detection_wq) {
		ai_error("Failed to create GPU workload detection work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&workload_ctx.detection_work, ai_gpu_workload_detection_worker);
	
	/* Configuration */
	workload_ctx.detection_active = true;
	workload_ctx.adaptive_detection_enabled = true;
	workload_ctx.workload_prediction_enabled = true;
	workload_ctx.thermal_aware_detection = true;
	workload_ctx.power_aware_detection = true;
	
	/* Initialize statistics */
	atomic64_set(&workload_ctx.workload_detections, 0);
	atomic64_set(&workload_ctx.workload_changes, 0);
	atomic64_set(&workload_ctx.samples_processed, 0);
	
	/* Start workload detection */
	queue_delayed_work(workload_ctx.detection_wq, &workload_ctx.detection_work,
			   msecs_to_jiffies(workload_ctx.detection_interval_ms));
	
	ai_info("GPU workload detection initialized with %d workload types",
		AI_GPU_WORKLOAD_IDLE + 1);
	
	return 0;
}

/**
 * ai_gpu_workload_detection_exit - Cleanup GPU workload detection
 */
void ai_gpu_workload_detection_exit(void)
{
	struct ai_gpu_workload_sample *sample, *tmp;
	
	workload_ctx.detection_active = false;
	
	/* Stop detection work queue */
	if (workload_ctx.detection_wq) {
		cancel_delayed_work_sync(&workload_ctx.detection_work);
		destroy_workqueue(workload_ctx.detection_wq);
		workload_ctx.detection_wq = NULL;
	}
	
	/* Free samples */
	spin_lock(&workload_ctx.samples_lock);
	list_for_each_entry_safe(sample, tmp, &workload_ctx.samples, list) {
		list_del(&sample->list);
		kfree(sample);
	}
	spin_unlock(&workload_ctx.samples_lock);
	
	ai_info("GPU workload detection cleaned up");
}

/**
 * ai_gpu_workload_detection_get_statistics - Get workload detection statistics
 */
void ai_gpu_workload_detection_get_statistics(u64 *detections, u64 *changes, u64 *samples,
					      enum ai_gpu_workload_type *current_workload,
					      u32 *confidence)
{
	if (detections)
		*detections = atomic64_read(&workload_ctx.workload_detections);
	
	if (changes)
		*changes = atomic64_read(&workload_ctx.workload_changes);
	
	if (samples)
		*samples = atomic64_read(&workload_ctx.samples_processed);
	
	if (current_workload)
		*current_workload = workload_ctx.current_workload;
	
	if (confidence)
		*confidence = workload_ctx.workload_confidence;
}

/* Export symbols */
EXPORT_SYMBOL(ai_gpu_add_workload_sample);
EXPORT_SYMBOL(ai_gpu_get_current_workload);
EXPORT_SYMBOL(ai_gpu_get_workload_profile);
EXPORT_SYMBOL(ai_gpu_workload_detection_get_statistics);