/*
 * AI Scheduler Debug and Logging System
 * Advanced debugging, logging and performance monitoring
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/ktime.h>
#include <linux/ring_buffer.h>
#include <linux/trace.h>

#include "ai_scheduler.h"

/* Debug configuration */
#define AI_DEBUG_BUFFER_SIZE	(64 * 1024)	/* 64KB ring buffer */
#define AI_MAX_DEBUG_ENTRIES	1000
#define AI_DEBUG_ENTRY_SIZE	256

/* Debug entry types */
enum ai_debug_entry_type {
	AI_DEBUG_PREDICTION = 0,
	AI_DEBUG_MIGRATION,
	AI_DEBUG_LEARNING,
	AI_DEBUG_PERFORMANCE,
	AI_DEBUG_ERROR,
	AI_DEBUG_TYPE_MAX
};

/* Debug entry structure */
struct ai_debug_entry {
	ktime_t timestamp;
	enum ai_debug_entry_type type;
	pid_t pid;
	char comm[TASK_COMM_LEN];
	char message[AI_DEBUG_ENTRY_SIZE];
	u32 data1, data2, data3, data4;
};

/* Debug context */
struct ai_debug_context {
	struct ring_buffer *ring_buffer;
	struct dentry *debugfs_root;
	atomic_t entries_count;
	atomic64_t total_entries;
	spinlock_t lock;
	
	/* Statistics */
	u32 prediction_entries;
	u32 migration_entries;
	u32 learning_entries;
	u32 performance_entries;
	u32 error_entries;
	
	/* Performance tracking */
	u64 total_prediction_time;
	u64 total_migration_time;
	u64 total_learning_time;
	u32 avg_prediction_time;
	u32 avg_migration_time;
	u32 avg_learning_time;
};

static struct ai_debug_context ai_debug_ctx;

/* Debug level names */
static const char *debug_level_names[] = {
	"NONE",
	"ERROR",
	"WARN",
	"INFO",
	"VERBOSE"
};

/* Debug entry type names */
static const char *debug_type_names[] = {
	"PREDICTION",
	"MIGRATION",
	"LEARNING",
	"PERFORMANCE",
	"ERROR"
};

/**
 * ai_debug_add_entry - Add a debug entry
 * @type: Entry type
 * @task: Related task (can be NULL)
 * @fmt: Format string
 * @...: Format arguments
 */
static void ai_debug_add_entry(enum ai_debug_entry_type type,
			       struct task_struct *task,
			       const char *fmt, ...)
{
	struct ai_debug_entry entry;
	va_list args;
	
	if (!ai_debug_ctx.ring_buffer)
		return;
	
	/* Fill entry */
	entry.timestamp = ktime_get();
	entry.type = type;
	entry.pid = task ? task->pid : 0;
	if (task)
		strncpy(entry.comm, task->comm, TASK_COMM_LEN - 1);
	else
		strcpy(entry.comm, "kernel");
	entry.comm[TASK_COMM_LEN - 1] = '\0';
	
	/* Format message */
	va_start(args, fmt);
	vsnprintf(entry.message, AI_DEBUG_ENTRY_SIZE - 1, fmt, args);
	va_end(args);
	entry.message[AI_DEBUG_ENTRY_SIZE - 1] = '\0';
	
	/* Add to ring buffer */
	ring_buffer_write(ai_debug_ctx.ring_buffer, sizeof(entry), &entry);
	
	/* Update statistics */
	atomic_inc(&ai_debug_ctx.entries_count);
	atomic64_inc(&ai_debug_ctx.total_entries);
	
	spin_lock(&ai_debug_ctx.lock);
	switch (type) {
	case AI_DEBUG_PREDICTION:
		ai_debug_ctx.prediction_entries++;
		break;
	case AI_DEBUG_MIGRATION:
		ai_debug_ctx.migration_entries++;
		break;
	case AI_DEBUG_LEARNING:
		ai_debug_ctx.learning_entries++;
		break;
	case AI_DEBUG_PERFORMANCE:
		ai_debug_ctx.performance_entries++;
		break;
	case AI_DEBUG_ERROR:
		ai_debug_ctx.error_entries++;
		break;
	default:
		break;
	}
	spin_unlock(&ai_debug_ctx.lock);
}

/**
 * ai_debug_log_prediction - Log a prediction event
 * @task: Task being predicted
 * @predicted_type: Predicted task type
 * @confidence: Prediction confidence
 * @time_taken: Time taken for prediction (ns)
 */
void ai_debug_log_prediction(struct task_struct *task, 
			     enum ai_task_type predicted_type,
			     u32 confidence, u64 time_taken)
{
	if (ai_sched_ctx.debug_level < AI_DEBUG_VERBOSE)
		return;
	
	ai_debug_add_entry(AI_DEBUG_PREDICTION, task,
			   "Predicted type=%d, confidence=%u%%, time=%llu ns",
			   predicted_type, confidence, time_taken);
	
	/* Update performance statistics */
	spin_lock(&ai_debug_ctx.lock);
	ai_debug_ctx.total_prediction_time += time_taken;
	if (ai_debug_ctx.prediction_entries > 0) {
		ai_debug_ctx.avg_prediction_time = 
			ai_debug_ctx.total_prediction_time / ai_debug_ctx.prediction_entries;
	}
	spin_unlock(&ai_debug_ctx.lock);
}

/**
 * ai_debug_log_migration - Log a CPU migration event
 * @task: Task being migrated
 * @from_cpu: Source CPU
 * @to_cpu: Destination CPU
 * @efficiency: Migration efficiency score
 * @time_taken: Time taken for migration (ns)
 */
void ai_debug_log_migration(struct task_struct *task, int from_cpu, int to_cpu,
			    u32 efficiency, u64 time_taken)
{
	if (ai_sched_ctx.debug_level < AI_DEBUG_INFO)
		return;
	
	ai_debug_add_entry(AI_DEBUG_MIGRATION, task,
			   "Migrated CPU %d->%d, efficiency=%u%%, time=%llu ns",
			   from_cpu, to_cpu, efficiency, time_taken);
	
	/* Update performance statistics */
	spin_lock(&ai_debug_ctx.lock);
	ai_debug_ctx.total_migration_time += time_taken;
	if (ai_debug_ctx.migration_entries > 0) {
		ai_debug_ctx.avg_migration_time = 
			ai_debug_ctx.total_migration_time / ai_debug_ctx.migration_entries;
	}
	spin_unlock(&ai_debug_ctx.lock);
}

/**
 * ai_debug_log_learning - Log a learning event
 * @samples: Number of samples processed
 * @accuracy: Current model accuracy
 * @time_taken: Time taken for learning (ns)
 */
void ai_debug_log_learning(u32 samples, u32 accuracy, u64 time_taken)
{
	if (ai_sched_ctx.debug_level < AI_DEBUG_INFO)
		return;
	
	ai_debug_add_entry(AI_DEBUG_LEARNING, NULL,
			   "Processed %u samples, accuracy=%u%%, time=%llu ns",
			   samples, accuracy, time_taken);
	
	/* Update performance statistics */
	spin_lock(&ai_debug_ctx.lock);
	ai_debug_ctx.total_learning_time += time_taken;
	if (ai_debug_ctx.learning_entries > 0) {
		ai_debug_ctx.avg_learning_time = 
			ai_debug_ctx.total_learning_time / ai_debug_ctx.learning_entries;
	}
	spin_unlock(&ai_debug_ctx.lock);
}

/**
 * ai_debug_log_performance - Log a performance event
 * @task: Related task
 * @metric: Performance metric name
 * @value: Metric value
 * @improvement: Performance improvement percentage
 */
void ai_debug_log_performance(struct task_struct *task, const char *metric,
			      u32 value, s32 improvement)
{
	if (ai_sched_ctx.debug_level < AI_DEBUG_INFO)
		return;
	
	ai_debug_add_entry(AI_DEBUG_PERFORMANCE, task,
			   "%s=%u, improvement=%d%%", metric, value, improvement);
}

/**
 * ai_debug_log_error - Log an error event
 * @task: Related task (can be NULL)
 * @error_code: Error code
 * @fmt: Error message format
 * @...: Format arguments
 */
void ai_debug_log_error(struct task_struct *task, int error_code,
			const char *fmt, ...)
{
	char error_msg[AI_DEBUG_ENTRY_SIZE];
	va_list args;
	
	if (ai_sched_ctx.debug_level < AI_DEBUG_ERROR)
		return;
	
	va_start(args, fmt);
	vsnprintf(error_msg, sizeof(error_msg) - 1, fmt, args);
	va_end(args);
	error_msg[sizeof(error_msg) - 1] = '\0';
	
	ai_debug_add_entry(AI_DEBUG_ERROR, task,
			   "Error %d: %s", error_code, error_msg);
}

/**
 * ai_debugfs_log_show - Show debug log entries
 */
static int ai_debugfs_log_show(struct seq_file *m, void *v)
{
	struct ring_buffer_iter *iter;
	struct ring_buffer_event *event;
	struct ai_debug_entry *entry;
	u64 timestamp_ns;
	u32 timestamp_sec, timestamp_usec;
	int count = 0;
	
	if (!ai_debug_ctx.ring_buffer)
		return -ENODEV;
	
	seq_printf(m, "AI Scheduler Debug Log\n");
	seq_printf(m, "======================\n\n");
	seq_printf(m, "Total Entries: %llu\n", atomic64_read(&ai_debug_ctx.total_entries));
	seq_printf(m, "Current Entries: %u\n\n", atomic_read(&ai_debug_ctx.entries_count));
	
	seq_printf(m, "Timestamp\t\tType\t\tPID\tComm\t\tMessage\n");
	seq_printf(m, "---------\t\t----\t\t---\t----\t\t-------\n");
	
	/* Iterate through ring buffer */
	iter = ring_buffer_read_prepare(ai_debug_ctx.ring_buffer, 0);
	if (!iter)
		return -ENOMEM;
	
	ring_buffer_read_start(iter);
	
	while ((event = ring_buffer_read(iter, NULL)) != NULL && count < 100) {
		entry = ring_buffer_event_data(event);
		
		timestamp_ns = ktime_to_ns(entry->timestamp);
		timestamp_sec = timestamp_ns / NSEC_PER_SEC;
		timestamp_usec = (timestamp_ns % NSEC_PER_SEC) / NSEC_PER_USEC;
		
		seq_printf(m, "%u.%06u\t%s\t\t%d\t%s\t\t%s\n",
			   timestamp_sec, timestamp_usec,
			   (entry->type < AI_DEBUG_TYPE_MAX) ? 
			   debug_type_names[entry->type] : "UNKNOWN",
			   entry->pid, entry->comm, entry->message);
		
		count++;
	}
	
	ring_buffer_read_finish(iter);
	
	if (count >= 100)
		seq_printf(m, "\n... (showing last 100 entries)\n");
	
	return 0;
}

/**
 * ai_debugfs_stats_show - Show debug statistics
 */
static int ai_debugfs_stats_show(struct seq_file *m, void *v)
{
	seq_printf(m, "AI Scheduler Debug Statistics\n");
	seq_printf(m, "=============================\n\n");
	
	seq_printf(m, "Debug Level: %u (%s)\n", 
		   ai_sched_ctx.debug_level,
		   (ai_sched_ctx.debug_level < ARRAY_SIZE(debug_level_names)) ?
		   debug_level_names[ai_sched_ctx.debug_level] : "UNKNOWN");
	
	seq_printf(m, "\nEntry Counts:\n");
	seq_printf(m, "-------------\n");
	
	spin_lock(&ai_debug_ctx.lock);
	seq_printf(m, "Prediction Entries: %u\n", ai_debug_ctx.prediction_entries);
	seq_printf(m, "Migration Entries: %u\n", ai_debug_ctx.migration_entries);
	seq_printf(m, "Learning Entries: %u\n", ai_debug_ctx.learning_entries);
	seq_printf(m, "Performance Entries: %u\n", ai_debug_ctx.performance_entries);
	seq_printf(m, "Error Entries: %u\n", ai_debug_ctx.error_entries);
	
	seq_printf(m, "\nAverage Times:\n");
	seq_printf(m, "--------------\n");
	seq_printf(m, "Prediction Time: %u ns\n", ai_debug_ctx.avg_prediction_time);
	seq_printf(m, "Migration Time: %u ns\n", ai_debug_ctx.avg_migration_time);
	seq_printf(m, "Learning Time: %u ns\n", ai_debug_ctx.avg_learning_time);
	
	seq_printf(m, "\nTotal Times:\n");
	seq_printf(m, "------------\n");
	seq_printf(m, "Total Prediction Time: %llu ns\n", ai_debug_ctx.total_prediction_time);
	seq_printf(m, "Total Migration Time: %llu ns\n", ai_debug_ctx.total_migration_time);
	seq_printf(m, "Total Learning Time: %llu ns\n", ai_debug_ctx.total_learning_time);
	spin_unlock(&ai_debug_ctx.lock);
	
	return 0;
}

/**
 * ai_debugfs_clear_write - Clear debug log
 */
static ssize_t ai_debugfs_clear_write(struct file *file, const char __user *buffer,
				      size_t count, loff_t *pos)
{
	if (ai_debug_ctx.ring_buffer) {
		ring_buffer_reset(ai_debug_ctx.ring_buffer);
		atomic_set(&ai_debug_ctx.entries_count, 0);
		
		spin_lock(&ai_debug_ctx.lock);
		ai_debug_ctx.prediction_entries = 0;
		ai_debug_ctx.migration_entries = 0;
		ai_debug_ctx.learning_entries = 0;
		ai_debug_ctx.performance_entries = 0;
		ai_debug_ctx.error_entries = 0;
		ai_debug_ctx.total_prediction_time = 0;
		ai_debug_ctx.total_migration_time = 0;
		ai_debug_ctx.total_learning_time = 0;
		ai_debug_ctx.avg_prediction_time = 0;
		ai_debug_ctx.avg_migration_time = 0;
		ai_debug_ctx.avg_learning_time = 0;
		spin_unlock(&ai_debug_ctx.lock);
		
		ai_info("Debug log cleared");
	}
	
	return count;
}

/* DebugFS file operations */
static int ai_debugfs_log_open(struct inode *inode, struct file *file)
{
	return single_open(file, ai_debugfs_log_show, NULL);
}

static int ai_debugfs_stats_open(struct inode *inode, struct file *file)
{
	return single_open(file, ai_debugfs_stats_show, NULL);
}

static const struct file_operations ai_debugfs_log_ops = {
	.open = ai_debugfs_log_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations ai_debugfs_stats_ops = {
	.open = ai_debugfs_stats_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static const struct file_operations ai_debugfs_clear_ops = {
	.write = ai_debugfs_clear_write,
};

/**
 * ai_debugfs_init - Initialize debugfs interface
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_debugfs_init(void)
{
	struct dentry *entry;
	
	/* Create ring buffer for debug entries */
	ai_debug_ctx.ring_buffer = ring_buffer_alloc(AI_DEBUG_BUFFER_SIZE, 0);
	if (!ai_debug_ctx.ring_buffer) {
		ai_error("Failed to allocate debug ring buffer");
		return -ENOMEM;
	}
	
	/* Initialize debug context */
	atomic_set(&ai_debug_ctx.entries_count, 0);
	atomic64_set(&ai_debug_ctx.total_entries, 0);
	spin_lock_init(&ai_debug_ctx.lock);
	
	/* Create debugfs directory */
	ai_debug_ctx.debugfs_root = debugfs_create_dir("ai_scheduler", NULL);
	if (!ai_debug_ctx.debugfs_root) {
		ai_error("Failed to create debugfs directory");
		ring_buffer_free(ai_debug_ctx.ring_buffer);
		return -ENODEV;
	}
	
	/* Create debug log file */
	entry = debugfs_create_file("log", 0444, ai_debug_ctx.debugfs_root,
				    NULL, &ai_debugfs_log_ops);
	if (!entry) {
		ai_error("Failed to create debugfs log file");
		goto error;
	}
	
	/* Create statistics file */
	entry = debugfs_create_file("stats", 0444, ai_debug_ctx.debugfs_root,
				    NULL, &ai_debugfs_stats_ops);
	if (!entry) {
		ai_error("Failed to create debugfs stats file");
		goto error;
	}
	
	/* Create clear file */
	entry = debugfs_create_file("clear", 0200, ai_debug_ctx.debugfs_root,
				    NULL, &ai_debugfs_clear_ops);
	if (!entry) {
		ai_error("Failed to create debugfs clear file");
		goto error;
	}
	
	/* Create debug level file */
	debugfs_create_u32("debug_level", 0644, ai_debug_ctx.debugfs_root,
			   &ai_sched_ctx.debug_level);
	
	ai_info("DebugFS interface created at /sys/kernel/debug/ai_scheduler/");
	return 0;

error:
	debugfs_remove_recursive(ai_debug_ctx.debugfs_root);
	ai_debug_ctx.debugfs_root = NULL;
	ring_buffer_free(ai_debug_ctx.ring_buffer);
	ai_debug_ctx.ring_buffer = NULL;
	return -ENODEV;
}

/**
 * ai_debugfs_cleanup - Cleanup debugfs interface
 */
void ai_debugfs_cleanup(void)
{
	if (ai_debug_ctx.debugfs_root) {
		debugfs_remove_recursive(ai_debug_ctx.debugfs_root);
		ai_debug_ctx.debugfs_root = NULL;
	}
	
	if (ai_debug_ctx.ring_buffer) {
		ring_buffer_free(ai_debug_ctx.ring_buffer);
		ai_debug_ctx.ring_buffer = NULL;
	}
	
	ai_info("DebugFS interface cleaned up");
}

/**
 * ai_print_statistics - Print current AI scheduler statistics
 */
void ai_print_statistics(void)
{
	u64 decisions_made, decisions_accepted;
	u32 success_rate;
	
	ai_get_integration_stats(&decisions_made, &decisions_accepted, &success_rate);
	
	ai_info("=== AI Scheduler Statistics ===");
	ai_info("Enabled: %s", ai_scheduler_enabled() ? "Yes" : "No");
	ai_info("Learning: %s", ai_learning_enabled() ? "Yes" : "No");
	ai_info("Predictions Made: %llu", atomic64_read(&ai_sched_ctx.predictions_made));
	ai_info("Predictions Correct: %llu", atomic64_read(&ai_sched_ctx.predictions_correct));
	ai_info("Accuracy: %llu%%", ai_get_accuracy_percentage());
	ai_info("CPU Decisions: %llu (Success: %u%%)", decisions_made, success_rate);
	ai_info("Learning Samples: %llu", atomic64_read(&ai_sched_ctx.learning_samples));
	ai_info("Model Updates: %llu", atomic64_read(&ai_sched_ctx.model_updates));
	ai_info("Debug Entries: %u", atomic_read(&ai_debug_ctx.entries_count));
	ai_info("===============================");
}

/* Export symbols */
EXPORT_SYMBOL(ai_debug_log_prediction);
EXPORT_SYMBOL(ai_debug_log_migration);
EXPORT_SYMBOL(ai_debug_log_learning);
EXPORT_SYMBOL(ai_debug_log_performance);
EXPORT_SYMBOL(ai_debug_log_error);
EXPORT_SYMBOL(ai_print_statistics);