/*
 * AI Scheduler /proc Interface
 * Provides user-space control and monitoring interface
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/ctype.h>

#include "ai_scheduler.h"

/* Proc directory */
static struct proc_dir_entry *ai_proc_dir;

/* Forward declarations */
extern void ai_get_integration_stats(u64 *decisions_made, u64 *decisions_accepted, u32 *success_rate);
extern int ai_integration_init(void);
extern void ai_integration_exit(void);

/**
 * ai_proc_status_show - Show AI scheduler status
 */
static int ai_proc_status_show(struct seq_file *m, void *v)
{
	u64 decisions_made, decisions_accepted;
	u32 success_rate;
	
	seq_printf(m, "AI Scheduler Status\n");
	seq_printf(m, "===================\n\n");
	
	/* Basic status */
	seq_printf(m, "Enabled: %s\n", ai_scheduler_enabled() ? "Yes" : "No");
	seq_printf(m, "Learning: %s\n", ai_learning_enabled() ? "Yes" : "No");
	seq_printf(m, "Learning State: ");
	
	switch (ai_sched_ctx.learning_state) {
	case AI_LEARNING_DISABLED:
		seq_printf(m, "Disabled\n");
		break;
	case AI_LEARNING_COLLECTING:
		seq_printf(m, "Collecting Data\n");
		break;
	case AI_LEARNING_TRAINING:
		seq_printf(m, "Training Model\n");
		break;
	case AI_LEARNING_ACTIVE:
		seq_printf(m, "Active\n");
		break;
	case AI_LEARNING_ERROR:
		seq_printf(m, "Error\n");
		break;
	default:
		seq_printf(m, "Unknown\n");
		break;
	}
	
	/* Statistics */
	seq_printf(m, "\nStatistics:\n");
	seq_printf(m, "-----------\n");
	seq_printf(m, "Predictions Made: %llu\n", atomic64_read(&ai_sched_ctx.predictions_made));
	seq_printf(m, "Predictions Correct: %llu\n", atomic64_read(&ai_sched_ctx.predictions_correct));
	seq_printf(m, "Accuracy: %llu%%\n", ai_get_accuracy_percentage());
	seq_printf(m, "Learning Samples: %llu\n", atomic64_read(&ai_sched_ctx.learning_samples));
	seq_printf(m, "Model Updates: %llu\n", atomic64_read(&ai_sched_ctx.model_updates));
	
	/* Integration statistics */
	ai_get_integration_stats(&decisions_made, &decisions_accepted, &success_rate);
	seq_printf(m, "CPU Decisions Made: %llu\n", decisions_made);
	seq_printf(m, "CPU Decisions Accepted: %llu\n", decisions_accepted);
	seq_printf(m, "Decision Success Rate: %u%%\n", success_rate);
	
	/* Configuration */
	seq_printf(m, "\nConfiguration:\n");
	seq_printf(m, "--------------\n");
	seq_printf(m, "Debug Level: %u\n", ai_sched_ctx.debug_level);
	seq_printf(m, "Learning Window: %u ms\n", ai_sched_ctx.learning_window_ms);
	seq_printf(m, "Prediction Window: %u ms\n", ai_sched_ctx.prediction_window_ms);
	seq_printf(m, "Model Update Interval: %u ms\n", ai_sched_ctx.model_update_interval_ms);
	seq_printf(m, "Max History Entries: %u\n", ai_sched_ctx.max_history_entries);
	
	/* CPU Clusters */
	seq_printf(m, "\nCPU Clusters:\n");
	seq_printf(m, "-------------\n");
	for (int i = 0; i < ai_sched_ctx.num_clusters; i++) {
		struct ai_cpu_cluster *cluster = &ai_sched_ctx.clusters[i];
		seq_printf(m, "Cluster %d: CPUs %*pbl\n", i, cpumask_pr_args(&cluster->cpus));
		seq_printf(m, "  Max Freq: %u kHz\n", cluster->max_frequency);
		seq_printf(m, "  Min Freq: %u kHz\n", cluster->min_frequency);
		seq_printf(m, "  Current Freq: %u kHz\n", cluster->current_frequency);
		seq_printf(m, "  Performance: %u%%\n", cluster->performance_capability);
		seq_printf(m, "  Efficiency: %u%%\n", cluster->power_efficiency);
		seq_printf(m, "  Level: ");
		
		switch (cluster->current_level) {
		case AI_PERF_POWERSAVE:
			seq_printf(m, "Power Save\n");
			break;
		case AI_PERF_BALANCED:
			seq_printf(m, "Balanced\n");
			break;
		case AI_PERF_PERFORMANCE:
			seq_printf(m, "Performance\n");
			break;
		case AI_PERF_GAMING:
			seq_printf(m, "Gaming\n");
			break;
		default:
			seq_printf(m, "Unknown\n");
			break;
		}
		seq_printf(m, "\n");
	}
	
	return 0;
}

/**
 * ai_proc_tasks_show - Show monitored tasks
 */
static int ai_proc_tasks_show(struct seq_file *m, void *v)
{
	/* This would iterate through the task hash table and show task info */
	seq_printf(m, "AI Monitored Tasks\n");
	seq_printf(m, "==================\n\n");
	seq_printf(m, "PID\tTGID\tComm\t\tType\t\tCPU%%\tMem(KB)\tInteract\tPerf\n");
	seq_printf(m, "---\t----\t----\t\t----\t\t----\t------\t--------\t----\n");
	
	/* Note: In a real implementation, we would iterate through ai_task_hash_table
	 * and display information about each monitored task. For now, we show the format.
	 */
	seq_printf(m, "(Task iteration would be implemented here)\n");
	seq_printf(m, "\nUse 'echo <pid> > /proc/ai_scheduler/task_detail' to see detailed info\n");
	
	return 0;
}

/**
 * ai_proc_control_show - Show control interface help
 */
static int ai_proc_control_show(struct seq_file *m, void *v)
{
	seq_printf(m, "AI Scheduler Control Interface\n");
	seq_printf(m, "==============================\n\n");
	seq_printf(m, "Available commands:\n");
	seq_printf(m, "  enable          - Enable AI scheduler\n");
	seq_printf(m, "  disable         - Disable AI scheduler\n");
	seq_printf(m, "  start_learning  - Start learning process\n");
	seq_printf(m, "  stop_learning   - Stop learning process\n");
	seq_printf(m, "  update_model    - Force model update\n");
	seq_printf(m, "  reset_stats     - Reset statistics\n");
	seq_printf(m, "  debug <level>   - Set debug level (0-4)\n");
	seq_printf(m, "  balance_load    - Force load balancing\n");
	seq_printf(m, "\nExample usage:\n");
	seq_printf(m, "  echo 'enable' > /proc/ai_scheduler/control\n");
	seq_printf(m, "  echo 'debug 3' > /proc/ai_scheduler/control\n");
	
	return 0;
}

/**
 * ai_proc_control_write - Handle control commands
 */
static ssize_t ai_proc_control_write(struct file *file, const char __user *buffer,
				     size_t count, loff_t *pos)
{
	char *cmd, *arg;
	char *kbuf;
	int ret = count;
	
	if (count > PAGE_SIZE)
		return -EINVAL;
	
	kbuf = kzalloc(count + 1, GFP_KERNEL);
	if (!kbuf)
		return -ENOMEM;
	
	if (copy_from_user(kbuf, buffer, count)) {
		kfree(kbuf);
		return -EFAULT;
	}
	
	kbuf[count] = '\0';
	
	/* Remove trailing newline */
	if (count > 0 && kbuf[count - 1] == '\n')
		kbuf[count - 1] = '\0';
	
	/* Parse command */
	cmd = kbuf;
	arg = strchr(kbuf, ' ');
	if (arg) {
		*arg = '\0';
		arg++;
		/* Skip whitespace */
		while (*arg && isspace(*arg))
			arg++;
	}
	
	/* Process commands */
	if (strcmp(cmd, "enable") == 0) {
		if (ai_scheduler_enable() == 0)
			ai_info("AI scheduler enabled via proc");
		else
			ai_error("Failed to enable AI scheduler");
			
	} else if (strcmp(cmd, "disable") == 0) {
		ai_scheduler_disable();
		ai_info("AI scheduler disabled via proc");
		
	} else if (strcmp(cmd, "start_learning") == 0) {
		if (ai_start_learning() == 0)
			ai_info("Learning started via proc");
		else
			ai_error("Failed to start learning");
			
	} else if (strcmp(cmd, "stop_learning") == 0) {
		ai_stop_learning();
		ai_info("Learning stopped via proc");
		
	} else if (strcmp(cmd, "update_model") == 0) {
		if (ai_update_model() == 0)
			ai_info("Model updated via proc");
		else
			ai_error("Failed to update model");
			
	} else if (strcmp(cmd, "reset_stats") == 0) {
		atomic64_set(&ai_sched_ctx.predictions_made, 0);
		atomic64_set(&ai_sched_ctx.predictions_correct, 0);
		atomic64_set(&ai_sched_ctx.learning_samples, 0);
		atomic64_set(&ai_sched_ctx.model_updates, 0);
		ai_info("Statistics reset via proc");
		
	} else if (strcmp(cmd, "debug") == 0 && arg) {
		unsigned long level;
		if (kstrtoul(arg, 10, &level) == 0 && level <= 4) {
			ai_sched_ctx.debug_level = level;
			ai_info("Debug level set to %lu via proc", level);
		} else {
			ai_error("Invalid debug level: %s", arg);
			ret = -EINVAL;
		}
		
	} else if (strcmp(cmd, "balance_load") == 0) {
		ai_balance_system_load();
		ai_info("Load balancing triggered via proc");
		
	} else {
		ai_error("Unknown command: %s", cmd);
		ret = -EINVAL;
	}
	
	kfree(kbuf);
	return ret;
}

/**
 * ai_proc_task_detail_write - Show detailed task information
 */
static ssize_t ai_proc_task_detail_write(struct file *file, const char __user *buffer,
					 size_t count, loff_t *pos)
{
	char kbuf[32];
	pid_t pid;
	struct task_struct *task;
	struct ai_task_data *data;
	
	if (count >= sizeof(kbuf))
		return -EINVAL;
	
	if (copy_from_user(kbuf, buffer, count))
		return -EFAULT;
	
	kbuf[count] = '\0';
	
	if (kstrtoint(kbuf, 10, &pid) != 0)
		return -EINVAL;
	
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (!task) {
		rcu_read_unlock();
		return -ESRCH;
	}
	get_task_struct(task);
	rcu_read_unlock();
	
	data = ai_get_task_data(task);
	if (data) {
		ai_dump_task_data(data);
		ai_put_task_data(data);
	} else {
		ai_info("Task %d (%s) not monitored by AI scheduler", pid, task->comm);
	}
	
	put_task_struct(task);
	return count;
}

/* Proc file operations */
static int ai_proc_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, ai_proc_status_show, NULL);
}

static int ai_proc_tasks_open(struct inode *inode, struct file *file)
{
	return single_open(file, ai_proc_tasks_show, NULL);
}

static int ai_proc_control_open(struct inode *inode, struct file *file)
{
	return single_open(file, ai_proc_control_show, NULL);
}

static const struct proc_ops ai_proc_status_ops = {
	.proc_open = ai_proc_status_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops ai_proc_tasks_ops = {
	.proc_open = ai_proc_tasks_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops ai_proc_control_ops = {
	.proc_open = ai_proc_control_open,
	.proc_read = seq_read,
	.proc_write = ai_proc_control_write,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};

static const struct proc_ops ai_proc_task_detail_ops = {
	.proc_write = ai_proc_task_detail_write,
};

/**
 * ai_proc_init - Initialize /proc interface
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_proc_init(void)
{
	struct proc_dir_entry *entry;
	
	/* Create main directory */
	ai_proc_dir = proc_mkdir("ai_scheduler", NULL);
	if (!ai_proc_dir) {
		ai_error("Failed to create /proc/ai_scheduler directory");
		return -ENOMEM;
	}
	
	/* Create status file */
	entry = proc_create("status", 0444, ai_proc_dir, &ai_proc_status_ops);
	if (!entry) {
		ai_error("Failed to create /proc/ai_scheduler/status");
		goto error;
	}
	
	/* Create tasks file */
	entry = proc_create("tasks", 0444, ai_proc_dir, &ai_proc_tasks_ops);
	if (!entry) {
		ai_error("Failed to create /proc/ai_scheduler/tasks");
		goto error;
	}
	
	/* Create control file */
	entry = proc_create("control", 0644, ai_proc_dir, &ai_proc_control_ops);
	if (!entry) {
		ai_error("Failed to create /proc/ai_scheduler/control");
		goto error;
	}
	
	/* Create task_detail file */
	entry = proc_create("task_detail", 0200, ai_proc_dir, &ai_proc_task_detail_ops);
	if (!entry) {
		ai_error("Failed to create /proc/ai_scheduler/task_detail");
		goto error;
	}
	
	ai_info("/proc/ai_scheduler interface created");
	return 0;

error:
	proc_remove(ai_proc_dir);
	ai_proc_dir = NULL;
	return -ENOMEM;
}

/**
 * ai_proc_cleanup - Cleanup /proc interface
 */
void ai_proc_cleanup(void)
{
	if (ai_proc_dir) {
		proc_remove(ai_proc_dir);
		ai_proc_dir = NULL;
		ai_info("/proc/ai_scheduler interface removed");
	}
}

/* Stub implementation for ai_dump_task_data */
void ai_dump_task_data(struct ai_task_data *data)
{
	if (!data)
		return;
	
	ai_info("Task Details for %s[%d]:", data->comm, data->pid);
	ai_info("  Type: %d, Required Perf: %d", data->current_type, data->required_perf);
	ai_info("  CPU Usage: %u%%, Memory: %u KB", 
		data->current_features.cpu_usage_avg,
		data->current_features.memory_usage);
	ai_info("  Interaction Score: %u, Priority: %u",
		data->current_features.user_interaction_score,
		data->current_features.priority_score);
	ai_info("  Performance Score: %u, Energy Score: %u",
		data->performance_score, data->energy_score);
	ai_info("  Learning Samples: %u, Accuracy: %u%%",
		data->learning_samples, data->prediction_accuracy);
	ai_info("  History Count: %u", data->history_count);
}

/* Export symbols */
EXPORT_SYMBOL(ai_dump_task_data);