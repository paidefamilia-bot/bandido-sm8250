/*
 * AI Scheduler Data Collection System
 * Advanced task behavior monitoring and data collection
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task.h>
#include <linux/mm.h>
#include <linux/vmstat.h>
#include <linux/cpufreq.h>
#include <linux/thermal.h>
#include <linux/power_supply.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/rculist.h>

#include "ai_scheduler.h"

/* Hash table for task data */
#define AI_TASK_HASH_BITS	10
#define AI_TASK_HASH_SIZE	(1 << AI_TASK_HASH_BITS)

static struct hlist_head ai_task_hash_table[AI_TASK_HASH_SIZE];
static DEFINE_SPINLOCK(ai_task_hash_lock);

/* Statistics */
static atomic_t ai_total_tasks = ATOMIC_INIT(0);
static atomic_t ai_active_tasks = ATOMIC_INIT(0);
static atomic64_t ai_data_points_collected = ATOMIC64_INIT(0);

/* Memory pool for task data */
static struct kmem_cache *ai_task_data_cache;
static struct kmem_cache *ai_history_entry_cache;

/**
 * ai_hash_task - Generate hash for task identification
 * @task: Target task
 * 
 * Returns: Hash value for the task
 */
u32 ai_hash_task(struct task_struct *task)
{
	u32 hash = 0;
	
	if (!task)
		return 0;
	
	/* Combine PID, TGID, and comm for unique hash */
	hash = hash_32(task->pid, 16);
	hash ^= hash_32(task->tgid, 16) << 8;
	hash ^= hash_str(task->comm, 8) << 16;
	
	return hash_32(hash, AI_TASK_HASH_BITS);
}

/**
 * ai_alloc_task_data - Allocate and initialize task data structure
 * @task: Target task
 * 
 * Returns: Allocated task data or NULL on failure
 */
static struct ai_task_data *ai_alloc_task_data(struct task_struct *task)
{
	struct ai_task_data *data;
	
	data = kmem_cache_zalloc(ai_task_data_cache, GFP_KERNEL);
	if (!data)
		return NULL;
	
	/* Initialize basic fields */
	data->pid = task->pid;
	data->tgid = task->tgid;
	strncpy(data->comm, task->comm, TASK_COMM_LEN - 1);
	data->comm[TASK_COMM_LEN - 1] = '\0';
	data->task_hash = ai_hash_task(task);
	
	/* Initialize lists and locks */
	INIT_LIST_HEAD(&data->history);
	spin_lock_init(&data->history_lock);
	
	/* Initialize reference counting */
	atomic_set(&data->refcount, 1);
	
	/* Initialize performance tracking */
	data->performance_score = 50;  /* Start with neutral score */
	data->energy_score = 50;
	data->user_satisfaction_score = 50;
	
	/* Initialize CPU preferences */
	cpumask_setall(&data->preferred_cpus);
	data->preferred_frequency = 0;  /* Auto-select */
	
	/* Set initial classification */
	data->current_type = AI_TASK_UNKNOWN;
	data->required_perf = AI_PERF_BALANCED;
	
	data->last_update_time = ktime_get_ns();
	
	ai_verbose("Allocated task data for %s[%d]", task->comm, task->pid);
	
	return data;
}

/**
 * ai_free_task_data - Free task data structure
 * @data: Task data to free
 */
static void ai_free_task_data(struct ai_task_data *data)
{
	struct ai_task_history_entry *entry, *tmp;
	
	if (!data)
		return;
	
	ai_verbose("Freeing task data for %s[%d]", data->comm, data->pid);
	
	/* Free history entries */
	spin_lock(&data->history_lock);
	list_for_each_entry_safe(entry, tmp, &data->history, list) {
		list_del(&entry->list);
		kmem_cache_free(ai_history_entry_cache, entry);
	}
	spin_unlock(&data->history_lock);
	
	/* Free the main structure */
	kmem_cache_free(ai_task_data_cache, data);
}

/**
 * ai_get_task_data - Get task data with reference counting
 * @task: Target task
 * 
 * Returns: Task data with incremented reference count
 */
struct ai_task_data *ai_get_task_data(struct task_struct *task)
{
	struct ai_task_data *data;
	u32 hash;
	
	if (!task || !ai_scheduler_enabled())
		return NULL;
	
	hash = ai_hash_task(task);
	
	rcu_read_lock();
	hlist_for_each_entry_rcu(data, &ai_task_hash_table[hash], hlist) {
		if (data->pid == task->pid && data->tgid == task->tgid &&
		    !strcmp(data->comm, task->comm)) {
			if (atomic_inc_not_zero(&data->refcount)) {
				rcu_read_unlock();
				return data;
			}
		}
	}
	rcu_read_unlock();
	
	return NULL;
}

/**
 * ai_put_task_data - Release task data reference
 * @data: Task data to release
 */
void ai_put_task_data(struct ai_task_data *data)
{
	if (!data)
		return;
	
	if (atomic_dec_and_test(&data->refcount)) {
		call_rcu(&data->rcu, (rcu_callback_t)ai_free_task_data);
	}
}

/**
 * ai_collect_cpu_stats - Collect CPU-related statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_cpu_stats(struct task_struct *task, 
				 struct ai_task_features *features)
{
	u64 runtime, total_time;
	u32 cpu_usage;
	
	/* Get task runtime information */
	runtime = task->se.sum_exec_runtime;
	total_time = ktime_get_ns() - task->start_time;
	
	if (total_time > 0) {
		cpu_usage = div64_u64(runtime * 100, total_time);
		features->cpu_usage_avg = min(cpu_usage, 100U);
	} else {
		features->cpu_usage_avg = 0;
	}
	
	/* Calculate CPU burst patterns */
	features->cpu_burst_frequency = task->se.nr_wakeups;
	features->cpu_idle_time = total_time - runtime;
	
	/* Context switch information */
	features->context_switches = task->nvcsw + task->nivcsw;
	features->voluntary_switches = task->nvcsw;
	features->involuntary_switches = task->nivcsw;
	
	/* Wakeup frequency (approximate) */
	features->wakeup_frequency = task->se.nr_wakeups;
	
	ai_verbose("CPU stats for %s[%d]: usage=%u%%, bursts=%u, ctx_sw=%u",
		   task->comm, task->pid, features->cpu_usage_avg,
		   features->cpu_burst_frequency, features->context_switches);
}

/**
 * ai_collect_memory_stats - Collect memory-related statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_memory_stats(struct task_struct *task,
				    struct ai_task_features *features)
{
	struct mm_struct *mm;
	unsigned long rss, vsize;
	
	mm = get_task_mm(task);
	if (!mm) {
		features->memory_usage = 0;
		features->memory_peak = 0;
		features->page_fault_rate = 0;
		return;
	}
	
	/* Get RSS and virtual memory size */
	rss = get_mm_rss(mm) << (PAGE_SHIFT - 10);  /* Convert to KB */
	vsize = mm->total_vm << (PAGE_SHIFT - 10);   /* Convert to KB */
	
	features->memory_usage = rss;
	features->memory_peak = max(features->memory_peak, rss);
	
	/* Page fault information */
	features->page_fault_rate = mm->pgfault;
	
	mmput(mm);
	
	ai_verbose("Memory stats for %s[%d]: rss=%uKB, vsize=%luKB, faults=%u",
		   task->comm, task->pid, features->memory_usage, vsize,
		   features->page_fault_rate);
}

/**
 * ai_collect_io_stats - Collect I/O-related statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_io_stats(struct task_struct *task,
				struct ai_task_features *features)
{
	struct task_io_accounting *ioac;
	
	ioac = &task->ioac;
	
	/* Read/write operations */
	features->io_read_rate = ioac->read_bytes >> 10;   /* Convert to KB */
	features->io_write_rate = ioac->write_bytes >> 10; /* Convert to KB */
	
	/* I/O wait time (approximate from delay accounting) */
	features->io_wait_time = task->delays ? 
		task->delays->blkio_delay : 0;
	
	ai_verbose("I/O stats for %s[%d]: read=%uKB, write=%uKB, wait=%u",
		   task->comm, task->pid, features->io_read_rate,
		   features->io_write_rate, features->io_wait_time);
}

/**
 * ai_collect_timing_stats - Collect timing-related statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_timing_stats(struct task_struct *task,
				    struct ai_task_features *features)
{
	u64 now = ktime_get_ns();
	
	features->runtime_total = task->se.sum_exec_runtime;
	features->runtime_recent = task->se.sum_exec_runtime; /* TODO: Calculate recent window */
	
	/* Sleep time calculation */
	if (task->last_wakee) {
		features->sleep_time_avg = now - task->last_wakee->wake_entry_time;
	} else {
		features->sleep_time_avg = 0;
	}
	
	/* Response time (simplified) */
	features->response_time_avg = task->se.avg.load_avg;
	
	ai_verbose("Timing stats for %s[%d]: runtime=%llu, sleep_avg=%llu",
		   task->comm, task->pid, features->runtime_total,
		   features->sleep_time_avg);
}

/**
 * ai_collect_interaction_stats - Collect user interaction statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_interaction_stats(struct task_struct *task,
					 struct ai_task_features *features)
{
	/* Determine if task is in foreground */
	bool is_foreground = (task->signal && 
			      task->signal->oom_score_adj <= 0);
	
	if (is_foreground) {
		features->foreground_time++;
		features->user_interaction_score = 80;
	} else {
		features->background_time++;
		features->user_interaction_score = 20;
	}
	
	/* Detect interactive tasks by name patterns */
	if (strstr(task->comm, "android.") || 
	    strstr(task->comm, "com.") ||
	    strstr(task->comm, "surfaceflinger") ||
	    strstr(task->comm, "system_server")) {
		features->user_interaction_score += 20;
	}
	
	/* Gaming detection */
	if (ai_is_gaming_task(task)) {
		features->user_interaction_score = 100;
		features->touch_events = 50; /* Estimate for games */
	}
	
	features->user_interaction_score = min(features->user_interaction_score, 100U);
	
	ai_verbose("Interaction stats for %s[%d]: fg=%s, score=%u",
		   task->comm, task->pid, is_foreground ? "yes" : "no",
		   features->user_interaction_score);
}

/**
 * ai_collect_system_impact - Collect system impact statistics
 * @task: Target task
 * @features: Features structure to fill
 */
static void ai_collect_system_impact(struct task_struct *task,
				     struct ai_task_features *features)
{
	/* System load impact (simplified) */
	features->system_load_impact = task->se.avg.load_avg >> 10;
	
	/* Thermal impact estimation */
	features->thermal_impact = features->cpu_usage_avg / 4;
	
	/* Power consumption estimation */
	features->power_consumption = (features->cpu_usage_avg * 60 +
				       features->memory_usage / 1024 * 10 +
				       features->io_read_rate / 1024 * 5) / 100;
	
	/* Battery drain rate */
	features->battery_drain_rate = features->power_consumption / 10;
	
	ai_verbose("System impact for %s[%d]: load=%u, thermal=%u, power=%u",
		   task->comm, task->pid, features->system_load_impact,
		   features->thermal_impact, features->power_consumption);
}

/**
 * ai_extract_task_features - Extract all features from a task
 * @task: Target task
 * @features: Features structure to fill
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_extract_task_features(struct task_struct *task,
			     struct ai_task_features *features)
{
	if (!task || !features)
		return -EINVAL;
	
	if (!ai_scheduler_enabled())
		return -ENODEV;
	
	/* Clear the features structure */
	memset(features, 0, sizeof(*features));
	
	/* Collect different types of statistics */
	ai_collect_cpu_stats(task, features);
	ai_collect_memory_stats(task, features);
	ai_collect_io_stats(task, features);
	ai_collect_timing_stats(task, features);
	ai_collect_interaction_stats(task, features);
	ai_collect_system_impact(task, features);
	
	/* Set initial classification */
	features->predicted_type = AI_TASK_UNKNOWN;
	features->confidence_score = 0;
	features->priority_score = 50;  /* Neutral priority */
	features->performance_requirement = AI_PERF_BALANCED;
	
	atomic64_inc(&ai_data_points_collected);
	
	ai_info("Extracted features for %s[%d]: cpu=%u%%, mem=%uKB, interaction=%u",
		task->comm, task->pid, features->cpu_usage_avg,
		features->memory_usage, features->user_interaction_score);
	
	return 0;
}

/**
 * ai_add_history_entry - Add a history entry for a task
 * @data: Task data
 * @features: Current features
 */
static int ai_add_history_entry(struct ai_task_data *data,
				const struct ai_task_features *features)
{
	struct ai_task_history_entry *entry;
	
	entry = kmem_cache_zalloc(ai_history_entry_cache, GFP_ATOMIC);
	if (!entry)
		return -ENOMEM;
	
	entry->timestamp = ktime_get();
	entry->features = *features;
	entry->actual_performance = data->performance_score;
	entry->user_satisfaction = data->user_satisfaction_score;
	entry->energy_efficiency = data->energy_score;
	
	spin_lock(&data->history_lock);
	
	/* Add to history list */
	list_add(&entry->list, &data->history);
	data->history_count++;
	
	/* Limit history size */
	if (data->history_count > ai_sched_ctx.max_history_entries) {
		struct ai_task_history_entry *old_entry;
		
		old_entry = list_last_entry(&data->history,
					    struct ai_task_history_entry, list);
		list_del(&old_entry->list);
		data->history_count--;
		
		spin_unlock(&data->history_lock);
		kmem_cache_free(ai_history_entry_cache, old_entry);
	} else {
		spin_unlock(&data->history_lock);
	}
	
	return 0;
}

/**
 * ai_update_task_features - Update features for a task
 * @data: Task data to update
 */
void ai_update_task_features(struct ai_task_data *data)
{
	struct task_struct *task;
	struct ai_task_features features;
	u64 now = ktime_get_ns();
	
	if (!data)
		return;
	
	/* Rate limiting - don't update too frequently */
	if (now - data->last_update_time < AI_LEARNING_WINDOW_MS * 1000000)
		return;
	
	/* Find the task */
	rcu_read_lock();
	task = find_task_by_vpid(data->pid);
	if (!task || task->tgid != data->tgid) {
		rcu_read_unlock();
		return;
	}
	get_task_struct(task);
	rcu_read_unlock();
	
	/* Extract current features */
	if (ai_extract_task_features(task, &features) == 0) {
		/* Update current features */
		data->current_features = features;
		data->last_update_time = now;
		
		/* Add to history if learning is enabled */
		if (ai_learning_enabled()) {
			ai_add_history_entry(data, &features);
			data->learning_samples++;
		}
		
		ai_verbose("Updated features for %s[%d]", data->comm, data->pid);
	}
	
	put_task_struct(task);
}

/**
 * ai_register_task - Register a task for AI monitoring
 * @task: Task to register
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_register_task(struct task_struct *task)
{
	struct ai_task_data *data;
	u32 hash;
	
	if (!task || !ai_scheduler_enabled())
		return -EINVAL;
	
	/* Check if task is already registered */
	data = ai_get_task_data(task);
	if (data) {
		ai_put_task_data(data);
		return 0;  /* Already registered */
	}
	
	/* Allocate new task data */
	data = ai_alloc_task_data(task);
	if (!data)
		return -ENOMEM;
	
	/* Extract initial features */
	if (ai_extract_task_features(task, &data->current_features) != 0) {
		ai_free_task_data(data);
		return -EIO;
	}
	
	/* Add to hash table */
	hash = ai_hash_task(task);
	spin_lock(&ai_task_hash_lock);
	hlist_add_head_rcu(&data->hlist, &ai_task_hash_table[hash]);
	spin_unlock(&ai_task_hash_lock);
	
	atomic_inc(&ai_total_tasks);
	atomic_inc(&ai_active_tasks);
	
	ai_info("Registered task %s[%d] for AI monitoring", task->comm, task->pid);
	
	return 0;
}

/**
 * ai_unregister_task - Unregister a task from AI monitoring
 * @task: Task to unregister
 */
void ai_unregister_task(struct task_struct *task)
{
	struct ai_task_data *data;
	u32 hash;
	
	if (!task)
		return;
	
	hash = ai_hash_task(task);
	
	spin_lock(&ai_task_hash_lock);
	hlist_for_each_entry_rcu(data, &ai_task_hash_table[hash], hlist) {
		if (data->pid == task->pid && data->tgid == task->tgid &&
		    !strcmp(data->comm, task->comm)) {
			hlist_del_rcu(&data->hlist);
			spin_unlock(&ai_task_hash_lock);
			
			atomic_dec(&ai_active_tasks);
			ai_put_task_data(data);
			
			ai_info("Unregistered task %s[%d] from AI monitoring",
				task->comm, task->pid);
			return;
		}
	}
	spin_unlock(&ai_task_hash_lock);
}

/**
 * ai_data_collection_init - Initialize data collection system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_data_collection_init(void)
{
	int i;
	
	/* Initialize hash table */
	for (i = 0; i < AI_TASK_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&ai_task_hash_table[i]);
	
	/* Create memory caches */
	ai_task_data_cache = kmem_cache_create("ai_task_data",
					       sizeof(struct ai_task_data),
					       0, SLAB_HWCACHE_ALIGN, NULL);
	if (!ai_task_data_cache)
		return -ENOMEM;
	
	ai_history_entry_cache = kmem_cache_create("ai_history_entry",
						   sizeof(struct ai_task_history_entry),
						   0, SLAB_HWCACHE_ALIGN, NULL);
	if (!ai_history_entry_cache) {
		kmem_cache_destroy(ai_task_data_cache);
		return -ENOMEM;
	}
	
	ai_info("AI data collection system initialized");
	
	return 0;
}

/**
 * ai_data_collection_exit - Cleanup data collection system
 */
void ai_data_collection_exit(void)
{
	struct ai_task_data *data;
	struct hlist_node *tmp;
	int i;
	
	/* Clean up all task data */
	spin_lock(&ai_task_hash_lock);
	for (i = 0; i < AI_TASK_HASH_SIZE; i++) {
		hlist_for_each_entry_safe(data, tmp, &ai_task_hash_table[i], hlist) {
			hlist_del_rcu(&data->hlist);
			ai_put_task_data(data);
		}
	}
	spin_unlock(&ai_task_hash_lock);
	
	/* Wait for RCU grace period */
	synchronize_rcu();
	
	/* Destroy memory caches */
	kmem_cache_destroy(ai_history_entry_cache);
	kmem_cache_destroy(ai_task_data_cache);
	
	ai_info("AI data collection system cleaned up");
}

/* Export symbols for other AI scheduler modules */
EXPORT_SYMBOL(ai_get_task_data);
EXPORT_SYMBOL(ai_put_task_data);
EXPORT_SYMBOL(ai_extract_task_features);
EXPORT_SYMBOL(ai_register_task);
EXPORT_SYMBOL(ai_unregister_task);