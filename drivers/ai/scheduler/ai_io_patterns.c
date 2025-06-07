/*
 * AI Scheduler I/O Pattern Recognition
 * Advanced I/O access pattern detection and optimization
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/bio.h>
#include <linux/blkdev.h>
#include <linux/writeback.h>
#include <linux/swap.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/sort.h>

#include "ai_scheduler.h"

/* I/O pattern analysis configuration */
#define AI_IO_PATTERN_WINDOW		64	/* Number of I/O samples */
#define AI_IO_PATTERN_TYPES		12	/* Number of pattern types */
#define AI_IO_SIZE_BINS			16	/* I/O size distribution bins */
#define AI_IO_LATENCY_BINS		10	/* I/O latency distribution bins */
#define AI_IO_SEQUENTIAL_THRESHOLD	8	/* Sequential access threshold */
#define AI_IO_RANDOM_THRESHOLD		4	/* Random access threshold */
#define AI_IO_BURST_THRESHOLD		10	/* I/O burst threshold (ops/sec) */

/* I/O access pattern types */
enum ai_io_pattern_type {
	AI_IO_PATTERN_SEQUENTIAL_READ = 0,	/* Sequential read pattern */
	AI_IO_PATTERN_SEQUENTIAL_WRITE,		/* Sequential write pattern */
	AI_IO_PATTERN_RANDOM_READ,		/* Random read pattern */
	AI_IO_PATTERN_RANDOM_WRITE,		/* Random write pattern */
	AI_IO_PATTERN_MIXED,			/* Mixed read/write pattern */
	AI_IO_PATTERN_BURSTY,			/* Bursty I/O pattern */
	AI_IO_PATTERN_STREAMING,		/* Streaming I/O pattern */
	AI_IO_PATTERN_DATABASE,			/* Database-like pattern */
	AI_IO_PATTERN_MULTIMEDIA,		/* Multimedia I/O pattern */
	AI_IO_PATTERN_NETWORK,			/* Network I/O pattern */
	AI_IO_PATTERN_IDLE,			/* Low I/O activity */
	AI_IO_PATTERN_IRREGULAR			/* Irregular pattern */
};

/* I/O operation sample */
struct ai_io_sample {
	ktime_t timestamp;
	u64 offset;		/* File/block offset */
	u32 size;		/* I/O size in bytes */
	u32 latency_us;		/* I/O latency in microseconds */
	u16 read_bytes_kb;	/* Read bytes in KB */
	u16 write_bytes_kb;	/* Write bytes in KB */
	u16 read_ops;		/* Number of read operations */
	u16 write_ops;		/* Number of write operations */
	u8 io_type;		/* I/O type (read/write/sync/async) */
	u8 priority;		/* I/O priority */
	u8 device_type;		/* Storage device type */
	u8 queue_depth;		/* I/O queue depth */
};

/* I/O pattern statistics */
struct ai_io_pattern_stats {
	enum ai_io_pattern_type type;
	u32 frequency;		/* How often this pattern occurs */
	u32 duration_avg;	/* Average duration in ms */
	u32 throughput_avg;	/* Average throughput (KB/s) */
	u32 throughput_peak;	/* Peak throughput */
	u32 latency_avg;	/* Average latency (us) */
	u32 latency_p99;	/* 99th percentile latency */
	u32 iops_avg;		/* Average IOPS */
	u32 queue_depth_avg;	/* Average queue depth */
	u32 confidence;		/* Pattern confidence (0-100) */
	u64 last_occurrence;	/* Last time pattern was detected */
};

/* I/O access tracking */
struct ai_io_access {
	u64 offset;
	u32 size;
	ktime_t timestamp;
	u8 type;		/* Read/write */
	struct list_head list;
};

/* Per-task I/O pattern context */
struct ai_task_io_pattern {
	pid_t pid;
	char comm[TASK_COMM_LEN];
	
	/* I/O samples */
	struct ai_io_sample samples[AI_IO_PATTERN_WINDOW];
	u32 sample_index;
	u32 sample_count;
	spinlock_t samples_lock;
	
	/* Pattern statistics */
	struct ai_io_pattern_stats patterns[AI_IO_PATTERN_TYPES];
	enum ai_io_pattern_type current_pattern;
	u32 pattern_confidence;
	
	/* I/O size and latency distributions */
	u32 size_distribution[AI_IO_SIZE_BINS];
	u32 latency_distribution[AI_IO_LATENCY_BINS];
	
	/* Access tracking for sequential detection */
	struct list_head recent_accesses;
	spinlock_t access_lock;
	u32 access_count;
	
	/* Prediction state */
	enum ai_io_pattern_type predicted_pattern;
	u32 prediction_confidence;
	u64 prediction_timestamp;
	u32 predicted_throughput;
	
	/* Performance metrics */
	u64 total_bytes_read;
	u64 total_bytes_written;
	u64 total_read_ops;
	u64 total_write_ops;
	u64 total_io_time;
	u32 avg_latency;
	u32 avg_throughput;
	u32 avg_iops;
	
	/* Statistics */
	u64 total_samples;
	u32 pattern_changes;
	u32 predictions_made;
	u32 predictions_correct;
	u32 sequential_runs;
	u32 random_runs;
	
	struct list_head list;
};

/* Global I/O pattern analysis context */
struct ai_io_pattern_ctx {
	struct list_head task_patterns;
	spinlock_t tasks_lock;
	u32 total_tasks;
	
	/* Global I/O statistics */
	atomic64_t samples_analyzed;
	atomic64_t patterns_detected;
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	atomic64_t sequential_detected;
	atomic64_t random_detected;
	
	/* System I/O state */
	u32 system_io_pressure;
	u32 storage_congestion;
	u32 network_congestion;
	
	/* Configuration */
	bool analysis_enabled;
	u32 sequential_threshold;
	u32 random_threshold;
	u32 burst_threshold;
};

static struct ai_io_pattern_ctx io_pattern_ctx;

/* Pattern type names */
static const char *io_pattern_names[] = {
	"SEQUENTIAL_READ",
	"SEQUENTIAL_WRITE",
	"RANDOM_READ",
	"RANDOM_WRITE",
	"MIXED",
	"BURSTY",
	"STREAMING",
	"DATABASE",
	"MULTIMEDIA",
	"NETWORK",
	"IDLE",
	"IRREGULAR"
};

/**
 * ai_get_io_info - Get I/O information for a task
 * @task: Target task
 * @read_bytes: Output for bytes read
 * @write_bytes: Output for bytes written
 * @read_ops: Output for read operations
 * @write_ops: Output for write operations
 */
static void ai_get_io_info(struct task_struct *task, u64 *read_bytes, u64 *write_bytes,
			   u64 *read_ops, u64 *write_ops)
{
	struct task_io_accounting *ioac = &task->ioac;
	
	*read_bytes = ioac->read_bytes;
	*write_bytes = ioac->write_bytes;
	*read_ops = ioac->syscr;
	*write_ops = ioac->syscw;
}

/**
 * ai_calculate_io_pressure - Calculate system I/O pressure
 * 
 * Returns: I/O pressure level (0-100)
 */
static u32 ai_calculate_io_pressure(void)
{
	/* Simplified I/O pressure calculation */
	/* In a real implementation, this would check:
	 * - Block device queue depths
	 * - I/O wait times
	 * - Storage device utilization
	 * - Network congestion
	 */
	
	u32 pressure = 0;
	
	/* Estimate based on system load */
	struct task_struct *p;
	u32 io_wait_tasks = 0;
	u32 total_tasks = 0;
	
	rcu_read_lock();
	for_each_process(p) {
		total_tasks++;
		if (p->state & TASK_UNINTERRUPTIBLE) {
			io_wait_tasks++;
		}
	}
	rcu_read_unlock();
	
	if (total_tasks > 0) {
		pressure = (io_wait_tasks * 100) / total_tasks;
	}
	
	io_pattern_ctx.system_io_pressure = min(pressure, 100U);
	
	return pressure;
}

/**
 * ai_detect_sequential_pattern - Detect sequential I/O access pattern
 * @pattern: Task I/O pattern context
 * @is_read: true for read pattern, false for write pattern
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_sequential_pattern(struct ai_task_io_pattern *pattern, bool is_read)
{
	u32 sequential_count = 0;
	u32 total_ops = 0;
	u32 i, confidence = 0;
	u64 prev_offset = 0;
	bool first = true;
	
	if (pattern->sample_count < 5)
		return 0;
	
	/* Check for sequential access in recent samples */
	for (i = 0; i < pattern->sample_count; i++) {
		struct ai_io_sample *sample = &pattern->samples[i];
		
		/* Filter by I/O type */
		bool is_read_op = (sample->read_ops > sample->write_ops);
		if (is_read != is_read_op)
			continue;
		
		total_ops++;
		
		if (!first) {
			/* Check if this access is sequential */
			u64 expected_offset = prev_offset + pattern->samples[i-1].size;
			u64 offset_diff = abs((s64)(sample->offset - expected_offset));
			
			if (offset_diff < sample->size) {
				sequential_count++;
			}
		}
		
		prev_offset = sample->offset;
		first = false;
	}
	
	/* Calculate confidence based on sequential ratio */
	if (total_ops >= io_pattern_ctx.sequential_threshold) {
		u32 sequential_ratio = (sequential_count * 100) / total_ops;
		if (sequential_ratio > 80) {
			confidence = 90;
		} else if (sequential_ratio > 60) {
			confidence = 70;
		} else if (sequential_ratio > 40) {
			confidence = 50;
		}
	}
	
	return confidence;
}

/**
 * ai_detect_random_pattern - Detect random I/O access pattern
 * @pattern: Task I/O pattern context
 * @is_read: true for read pattern, false for write pattern
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_random_pattern(struct ai_task_io_pattern *pattern, bool is_read)
{
	u32 random_count = 0;
	u32 total_ops = 0;
	u32 i, confidence = 0;
	u64 prev_offset = 0;
	bool first = true;
	
	if (pattern->sample_count < 5)
		return 0;
	
	/* Check for random access in recent samples */
	for (i = 0; i < pattern->sample_count; i++) {
		struct ai_io_sample *sample = &pattern->samples[i];
		
		/* Filter by I/O type */
		bool is_read_op = (sample->read_ops > sample->write_ops);
		if (is_read != is_read_op)
			continue;
		
		total_ops++;
		
		if (!first) {
			/* Check if this access is random (non-sequential) */
			u64 expected_offset = prev_offset + pattern->samples[i-1].size;
			u64 offset_diff = abs((s64)(sample->offset - expected_offset));
			
			if (offset_diff > sample->size * 4) { /* Significant jump */
				random_count++;
			}
		}
		
		prev_offset = sample->offset;
		first = false;
	}
	
	/* Calculate confidence based on random ratio */
	if (total_ops >= io_pattern_ctx.random_threshold) {
		u32 random_ratio = (random_count * 100) / total_ops;
		if (random_ratio > 70) {
			confidence = 85;
		} else if (random_ratio > 50) {
			confidence = 65;
		} else if (random_ratio > 30) {
			confidence = 45;
		}
	}
	
	return confidence;
}

/**
 * ai_detect_bursty_pattern - Detect bursty I/O pattern
 * @pattern: Task I/O pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_bursty_pattern(struct ai_task_io_pattern *pattern)
{
	u32 burst_periods = 0;
	u32 quiet_periods = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Look for alternating periods of high and low I/O activity */
	for (i = 0; i < pattern->sample_count; i++) {
		struct ai_io_sample *sample = &pattern->samples[i];
		u32 total_ops = sample->read_ops + sample->write_ops;
		
		if (total_ops > io_pattern_ctx.burst_threshold) {
			burst_periods++;
		} else if (total_ops < 2) {
			quiet_periods++;
		}
	}
	
	/* Bursty pattern has both high activity and quiet periods */
	if (burst_periods > 0 && quiet_periods > 0) {
		u32 burst_ratio = (burst_periods * 100) / pattern->sample_count;
		u32 quiet_ratio = (quiet_periods * 100) / pattern->sample_count;
		
		if (burst_ratio > 20 && quiet_ratio > 20) {
			confidence = min(burst_ratio + quiet_ratio, 90U);
		}
	}
	
	return confidence;
}

/**
 * ai_detect_streaming_pattern - Detect streaming I/O pattern
 * @pattern: Task I/O pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_streaming_pattern(struct ai_task_io_pattern *pattern)
{
	u32 large_io_count = 0;
	u32 consistent_throughput = 0;
	u32 i, confidence = 0;
	u32 prev_throughput = 0;
	
	if (pattern->sample_count < 8)
		return 0;
	
	/* Look for large, consistent I/O operations */
	for (i = 0; i < pattern->sample_count; i++) {
		struct ai_io_sample *sample = &pattern->samples[i];
		u32 total_kb = sample->read_bytes_kb + sample->write_bytes_kb;
		
		/* Large I/O operations (>64KB) */
		if (sample->size > 64 * 1024) {
			large_io_count++;
		}
		
		/* Consistent throughput */
		if (i > 0 && prev_throughput > 0) {
			u32 throughput_diff = abs((int)total_kb - (int)prev_throughput);
			if (throughput_diff < prev_throughput / 4) { /* Within 25% */
				consistent_throughput++;
			}
		}
		
		prev_throughput = total_kb;
	}
	
	/* Streaming pattern has large, consistent I/O */
	if (large_io_count > pattern->sample_count / 2) {
		confidence = 70;
		
		if (consistent_throughput > pattern->sample_count / 3) {
			confidence = 85;
		}
	}
	
	return confidence;
}

/**
 * ai_detect_database_pattern - Detect database-like I/O pattern
 * @pattern: Task I/O pattern context
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_detect_database_pattern(struct ai_task_io_pattern *pattern)
{
	u32 small_random_reads = 0;
	u32 sync_writes = 0;
	u32 mixed_operations = 0;
	u32 i, confidence = 0;
	
	if (pattern->sample_count < 10)
		return 0;
	
	/* Look for database characteristics */
	for (i = 0; i < pattern->sample_count; i++) {
		struct ai_io_sample *sample = &pattern->samples[i];
		
		/* Small random reads (typical for index lookups) */
		if (sample->read_ops > sample->write_ops && sample->size < 16 * 1024) {
			small_random_reads++;
		}
		
		/* Synchronous writes (typical for transaction logs) */
		if (sample->write_ops > 0 && sample->latency_us > 1000) { /* >1ms latency suggests sync */
			sync_writes++;
		}
		
		/* Mixed read/write operations */
		if (sample->read_ops > 0 && sample->write_ops > 0) {
			mixed_operations++;
		}
	}
	
	/* Database pattern has mix of small reads and sync writes */
	if (small_random_reads > pattern->sample_count / 4 && 
	    sync_writes > 0 && 
	    mixed_operations > pattern->sample_count / 3) {
		confidence = 75;
	} else if (small_random_reads > pattern->sample_count / 3) {
		confidence = 50;
	}
	
	return confidence;
}

/**
 * ai_analyze_io_pattern - Analyze I/O pattern for a task
 * @pattern: Task I/O pattern context
 */
static void ai_analyze_io_pattern(struct ai_task_io_pattern *pattern)
{
	enum ai_io_pattern_type detected_pattern = AI_IO_PATTERN_IRREGULAR;
	u32 max_confidence = 0;
	u32 confidence;
	
	/* Test different pattern types */
	confidence = ai_detect_sequential_pattern(pattern, true);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_SEQUENTIAL_READ;
	}
	
	confidence = ai_detect_sequential_pattern(pattern, false);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_SEQUENTIAL_WRITE;
	}
	
	confidence = ai_detect_random_pattern(pattern, true);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_RANDOM_READ;
	}
	
	confidence = ai_detect_random_pattern(pattern, false);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_RANDOM_WRITE;
	}
	
	confidence = ai_detect_bursty_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_BURSTY;
	}
	
	confidence = ai_detect_streaming_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_STREAMING;
	}
	
	confidence = ai_detect_database_pattern(pattern);
	if (confidence > max_confidence) {
		max_confidence = confidence;
		detected_pattern = AI_IO_PATTERN_DATABASE;
	}
	
	/* Check for idle pattern */
	u32 total_ops = 0;
	for (u32 i = 0; i < pattern->sample_count; i++) {
		total_ops += pattern->samples[i].read_ops + pattern->samples[i].write_ops;
	}
	
	if (total_ops < pattern->sample_count) { /* Less than 1 op per sample */
		detected_pattern = AI_IO_PATTERN_IDLE;
		max_confidence = 80;
	}
	
	/* Update pattern if confidence is sufficient */
	if (max_confidence > 60 && detected_pattern != pattern->current_pattern) {
		enum ai_io_pattern_type old_pattern = pattern->current_pattern;
		
		pattern->current_pattern = detected_pattern;
		pattern->pattern_confidence = max_confidence;
		pattern->pattern_changes++;
		
		/* Update pattern statistics */
		struct ai_io_pattern_stats *stats = &pattern->patterns[detected_pattern];
		stats->type = detected_pattern;
		stats->frequency++;
		stats->confidence = max_confidence;
		stats->last_occurrence = ktime_get_ns();
		
		/* Update sequential/random counters */
		if (detected_pattern == AI_IO_PATTERN_SEQUENTIAL_READ || 
		    detected_pattern == AI_IO_PATTERN_SEQUENTIAL_WRITE) {
			pattern->sequential_runs++;
			atomic64_inc(&io_pattern_ctx.sequential_detected);
		} else if (detected_pattern == AI_IO_PATTERN_RANDOM_READ ||
			   detected_pattern == AI_IO_PATTERN_RANDOM_WRITE) {
			pattern->random_runs++;
			atomic64_inc(&io_pattern_ctx.random_detected);
		}
		
		atomic64_inc(&io_pattern_ctx.patterns_detected);
		
		ai_verbose("I/O pattern change for %s[%d]: %s -> %s (confidence: %u%%)",
			   pattern->comm, pattern->pid,
			   (old_pattern < AI_IO_PATTERN_TYPES) ? io_pattern_names[old_pattern] : "UNKNOWN",
			   io_pattern_names[detected_pattern], max_confidence);
	}
}

/**
 * ai_predict_io_performance - Predict I/O performance
 * @pattern: Task I/O pattern context
 * 
 * Returns: Predicted throughput in KB/s
 */
static u32 ai_predict_io_performance(struct ai_task_io_pattern *pattern)
{
	u32 predicted_throughput = 0;
	
	/* Base prediction on current pattern and historical performance */
	switch (pattern->current_pattern) {
	case AI_IO_PATTERN_SEQUENTIAL_READ:
	case AI_IO_PATTERN_SEQUENTIAL_WRITE:
		/* Sequential I/O typically has high throughput */
		predicted_throughput = pattern->avg_throughput * 110 / 100; /* 10% boost */
		break;
		
	case AI_IO_PATTERN_RANDOM_READ:
	case AI_IO_PATTERN_RANDOM_WRITE:
		/* Random I/O typically has lower throughput */
		predicted_throughput = pattern->avg_throughput * 80 / 100; /* 20% reduction */
		break;
		
	case AI_IO_PATTERN_STREAMING:
		/* Streaming I/O should maintain consistent throughput */
		predicted_throughput = pattern->avg_throughput;
		break;
		
	case AI_IO_PATTERN_BURSTY:
		/* Bursty I/O has variable performance */
		predicted_throughput = pattern->avg_throughput * 90 / 100; /* Slight reduction */
		break;
		
	case AI_IO_PATTERN_IDLE:
		predicted_throughput = 0;
		break;
		
	default:
		predicted_throughput = pattern->avg_throughput;
		break;
	}
	
	/* Adjust for system I/O pressure */
	if (io_pattern_ctx.system_io_pressure > 50) {
		predicted_throughput = predicted_throughput * (100 - io_pattern_ctx.system_io_pressure) / 100;
	}
	
	pattern->predicted_throughput = predicted_throughput;
	pattern->prediction_timestamp = ktime_get_ns();
	pattern->predictions_made++;
	
	atomic64_inc(&io_pattern_ctx.predictions_made);
	
	return predicted_throughput;
}

/**
 * ai_collect_io_sample - Collect I/O sample for a task
 * @task: Target task
 * @pattern: Task I/O pattern context
 */
static void ai_collect_io_sample(struct task_struct *task,
				 struct ai_task_io_pattern *pattern)
{
	struct ai_io_sample *sample;
	u64 read_bytes, write_bytes, read_ops, write_ops;
	u32 index;
	
	ai_get_io_info(task, &read_bytes, &write_bytes, &read_ops, &write_ops);
	
	spin_lock(&pattern->samples_lock);
	
	/* Get next sample slot */
	index = pattern->sample_index;
	sample = &pattern->samples[index];
	
	/* Fill sample data */
	sample->timestamp = ktime_get();
	
	/* Calculate deltas if we have previous sample */
	if (pattern->sample_count > 0) {
		u32 prev_index = (index - 1 + AI_IO_PATTERN_WINDOW) % AI_IO_PATTERN_WINDOW;
		struct ai_io_sample *prev_sample = &pattern->samples[prev_index];
		
		sample->read_bytes_kb = (read_bytes - pattern->total_bytes_read) >> 10;
		sample->write_bytes_kb = (write_bytes - pattern->total_bytes_written) >> 10;
		sample->read_ops = read_ops - pattern->total_read_ops;
		sample->write_ops = write_ops - pattern->total_write_ops;
		
		/* Estimate latency based on I/O wait time */
		if (task->delays) {
			sample->latency_us = task->delays->blkio_delay / 1000; /* Convert to microseconds */
		} else {
			sample->latency_us = 1000; /* Default 1ms */
		}
		
		/* Calculate throughput */
		u64 time_diff_ms = ktime_to_ms(ktime_sub(sample->timestamp, prev_sample->timestamp));
		if (time_diff_ms > 0) {
			u32 total_kb = sample->read_bytes_kb + sample->write_bytes_kb;
			pattern->avg_throughput = (pattern->avg_throughput + (total_kb * 1000 / time_diff_ms)) / 2;
		}
	} else {
		sample->read_bytes_kb = read_bytes >> 10;
		sample->write_bytes_kb = write_bytes >> 10;
		sample->read_ops = read_ops;
		sample->write_ops = write_ops;
		sample->latency_us = 1000;
	}
	
	/* Update totals */
	pattern->total_bytes_read = read_bytes;
	pattern->total_bytes_written = write_bytes;
	pattern->total_read_ops = read_ops;
	pattern->total_write_ops = write_ops;
	
	/* Estimate other fields */
	sample->size = (sample->read_bytes_kb + sample->write_bytes_kb) * 1024;
	sample->offset = 0; /* Would need file system integration for real offset */
	sample->io_type = (sample->read_ops > sample->write_ops) ? 0 : 1; /* 0=read, 1=write */
	sample->priority = task->prio;
	sample->device_type = 0; /* Would need block layer integration */
	sample->queue_depth = 1; /* Simplified */
	
	/* Update ring buffer */
	pattern->sample_index = (index + 1) % AI_IO_PATTERN_WINDOW;
	if (pattern->sample_count < AI_IO_PATTERN_WINDOW)
		pattern->sample_count++;
	
	/* Update distributions */
	u32 size_bin = min(sample->size / (64 * 1024), AI_IO_SIZE_BINS - 1U); /* 64KB bins */
	u32 latency_bin = min(sample->latency_us / 1000, AI_IO_LATENCY_BINS - 1U); /* 1ms bins */
	pattern->size_distribution[size_bin]++;
	pattern->latency_distribution[latency_bin]++;
	
	/* Update performance metrics */
	pattern->avg_latency = (pattern->avg_latency + sample->latency_us) / 2;
	pattern->avg_iops = (pattern->avg_iops + sample->read_ops + sample->write_ops) / 2;
	pattern->total_samples++;
	
	spin_unlock(&pattern->samples_lock);
	
	atomic64_inc(&io_pattern_ctx.samples_analyzed);
}

/**
 * ai_register_task_io_pattern - Register task for I/O pattern analysis
 * @task: Task to analyze
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_register_task_io_pattern(struct task_struct *task)
{
	struct ai_task_io_pattern *pattern;
	
	if (!task || !io_pattern_ctx.analysis_enabled)
		return -EINVAL;
	
	/* Check if already registered */
	spin_lock(&io_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &io_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&io_pattern_ctx.tasks_lock);
			return 0;
		}
	}
	spin_unlock(&io_pattern_ctx.tasks_lock);
	
	/* Allocate new pattern context */
	pattern = kzalloc(sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		return -ENOMEM;
	
	pattern->pid = task->pid;
	strncpy(pattern->comm, task->comm, TASK_COMM_LEN - 1);
	pattern->comm[TASK_COMM_LEN - 1] = '\0';
	
	spin_lock_init(&pattern->samples_lock);
	spin_lock_init(&pattern->access_lock);
	INIT_LIST_HEAD(&pattern->recent_accesses);
	pattern->current_pattern = AI_IO_PATTERN_IRREGULAR;
	
	/* Add to analysis list */
	spin_lock(&io_pattern_ctx.tasks_lock);
	list_add(&pattern->list, &io_pattern_ctx.task_patterns);
	io_pattern_ctx.total_tasks++;
	spin_unlock(&io_pattern_ctx.tasks_lock);
	
	ai_info("Registered task %s[%d] for I/O pattern analysis", 
		task->comm, task->pid);
	
	return 0;
}

/**
 * ai_update_task_io_pattern - Update I/O pattern analysis for a task
 * @task: Target task
 */
void ai_update_task_io_pattern(struct task_struct *task)
{
	struct ai_task_io_pattern *pattern;
	
	if (!task || !io_pattern_ctx.analysis_enabled)
		return;
	
	spin_lock(&io_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &io_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			spin_unlock(&io_pattern_ctx.tasks_lock);
			
			/* Collect sample and analyze */
			ai_collect_io_sample(task, pattern);
			ai_analyze_io_pattern(pattern);
			ai_predict_io_performance(pattern);
			
			return;
		}
	}
	spin_unlock(&io_pattern_ctx.tasks_lock);
	
	/* Task not registered, register it now */
	ai_register_task_io_pattern(task);
}

/**
 * ai_get_task_io_pattern - Get I/O pattern for a task
 * @task: Target task
 * @confidence: Output for pattern confidence
 * 
 * Returns: Current I/O pattern type
 */
enum ai_io_pattern_type ai_get_task_io_pattern(struct task_struct *task, u32 *confidence)
{
	struct ai_task_io_pattern *pattern;
	enum ai_io_pattern_type type = AI_IO_PATTERN_IRREGULAR;
	
	if (!task)
		return AI_IO_PATTERN_IRREGULAR;
	
	spin_lock(&io_pattern_ctx.tasks_lock);
	list_for_each_entry(pattern, &io_pattern_ctx.task_patterns, list) {
		if (pattern->pid == task->pid && !strcmp(pattern->comm, task->comm)) {
			type = pattern->current_pattern;
			if (confidence)
				*confidence = pattern->pattern_confidence;
			break;
		}
	}
	spin_unlock(&io_pattern_ctx.tasks_lock);
	
	return type;
}

/**
 * ai_io_pattern_init - Initialize I/O pattern analysis system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_io_pattern_init(void)
{
	INIT_LIST_HEAD(&io_pattern_ctx.task_patterns);
	spin_lock_init(&io_pattern_ctx.tasks_lock);
	
	io_pattern_ctx.analysis_enabled = true;
	io_pattern_ctx.sequential_threshold = AI_IO_SEQUENTIAL_THRESHOLD;
	io_pattern_ctx.random_threshold = AI_IO_RANDOM_THRESHOLD;
	io_pattern_ctx.burst_threshold = AI_IO_BURST_THRESHOLD;
	
	atomic64_set(&io_pattern_ctx.samples_analyzed, 0);
	atomic64_set(&io_pattern_ctx.patterns_detected, 0);
	atomic64_set(&io_pattern_ctx.predictions_made, 0);
	atomic64_set(&io_pattern_ctx.predictions_correct, 0);
	atomic64_set(&io_pattern_ctx.sequential_detected, 0);
	atomic64_set(&io_pattern_ctx.random_detected, 0);
	
	ai_info("I/O pattern analysis system initialized");
	
	return 0;
}

/**
 * ai_io_pattern_exit - Cleanup I/O pattern analysis system
 */
void ai_io_pattern_exit(void)
{
	struct ai_task_io_pattern *pattern, *tmp;
	
	io_pattern_ctx.analysis_enabled = false;
	
	spin_lock(&io_pattern_ctx.tasks_lock);
	list_for_each_entry_safe(pattern, tmp, &io_pattern_ctx.task_patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	io_pattern_ctx.total_tasks = 0;
	spin_unlock(&io_pattern_ctx.tasks_lock);
	
	ai_info("I/O pattern analysis system cleaned up");
}

/**
 * ai_io_pattern_get_statistics - Get I/O pattern analysis statistics
 */
void ai_io_pattern_get_statistics(u64 *samples, u64 *patterns, u64 *predictions,
				  u64 *sequential, u64 *random, u32 *tasks)
{
	if (samples)
		*samples = atomic64_read(&io_pattern_ctx.samples_analyzed);
	
	if (patterns)
		*patterns = atomic64_read(&io_pattern_ctx.patterns_detected);
	
	if (predictions)
		*predictions = atomic64_read(&io_pattern_ctx.predictions_made);
	
	if (sequential)
		*sequential = atomic64_read(&io_pattern_ctx.sequential_detected);
	
	if (random)
		*random = atomic64_read(&io_pattern_ctx.random_detected);
	
	if (tasks)
		*tasks = io_pattern_ctx.total_tasks;
}

/* Export symbols */
EXPORT_SYMBOL(ai_register_task_io_pattern);
EXPORT_SYMBOL(ai_update_task_io_pattern);
EXPORT_SYMBOL(ai_get_task_io_pattern);
EXPORT_SYMBOL(ai_io_pattern_get_statistics);