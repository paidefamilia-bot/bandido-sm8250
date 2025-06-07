/*
 * AI Scheduler SysFS Interface
 * System configuration and performance testing interface
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/workqueue.h>

#include "ai_scheduler.h"

/* SysFS objects */
static struct kobject *ai_sysfs_kobj;
static struct kobject *ai_perf_kobj;
static struct kobject *ai_config_kobj;

/* Performance test context */
struct ai_perf_test {
	bool running;
	u32 test_duration_ms;
	u32 test_iterations;
	u32 current_iteration;
	
	/* Results */
	u64 total_prediction_time;
	u64 total_migration_time;
	u64 total_learning_time;
	u32 predictions_made;
	u32 migrations_made;
	u32 learning_updates;
	
	/* Averages */
	u32 avg_prediction_time_ns;
	u32 avg_migration_time_ns;
	u32 avg_learning_time_ns;
	
	struct delayed_work test_work;
	struct workqueue_struct *test_wq;
};

static struct ai_perf_test ai_perf_test_ctx;

/* Forward declarations */
extern void ai_debug_log_prediction(struct task_struct *task, enum ai_task_type predicted_type,
				    u32 confidence, u64 time_taken);
extern void ai_debug_log_migration(struct task_struct *task, int from_cpu, int to_cpu,
				   u32 efficiency, u64 time_taken);
extern void ai_debug_log_learning(u32 samples, u32 accuracy, u64 time_taken);

/**
 * ai_sysfs_enabled_show - Show AI scheduler enabled status
 */
static ssize_t ai_sysfs_enabled_show(struct kobject *kobj,
				     struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ai_scheduler_enabled() ? 1 : 0);
}

/**
 * ai_sysfs_enabled_store - Enable/disable AI scheduler
 */
static ssize_t ai_sysfs_enabled_store(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int enabled;
	
	if (kstrtoint(buf, 10, &enabled) != 0)
		return -EINVAL;
	
	if (enabled) {
		if (ai_scheduler_enable() != 0)
			return -EIO;
	} else {
		ai_scheduler_disable();
	}
	
	return count;
}

/**
 * ai_sysfs_learning_show - Show learning enabled status
 */
static ssize_t ai_sysfs_learning_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", ai_learning_enabled() ? 1 : 0);
}

/**
 * ai_sysfs_learning_store - Enable/disable learning
 */
static ssize_t ai_sysfs_learning_store(struct kobject *kobj,
				       struct kobj_attribute *attr,
				       const char *buf, size_t count)
{
	int enabled;
	
	if (kstrtoint(buf, 10, &enabled) != 0)
		return -EINVAL;
	
	if (enabled) {
		if (ai_start_learning() != 0)
			return -EIO;
	} else {
		ai_stop_learning();
	}
	
	return count;
}

/**
 * ai_sysfs_accuracy_show - Show prediction accuracy
 */
static ssize_t ai_sysfs_accuracy_show(struct kobject *kobj,
				      struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%llu\n", ai_get_accuracy_percentage());
}

/**
 * ai_sysfs_stats_show - Show comprehensive statistics
 */
static ssize_t ai_sysfs_stats_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	u64 decisions_made, decisions_accepted;
	u32 success_rate;
	ssize_t len = 0;
	
	ai_get_integration_stats(&decisions_made, &decisions_accepted, &success_rate);
	
	len += sprintf(buf + len, "enabled=%d\n", ai_scheduler_enabled() ? 1 : 0);
	len += sprintf(buf + len, "learning=%d\n", ai_learning_enabled() ? 1 : 0);
	len += sprintf(buf + len, "predictions_made=%llu\n", 
		       atomic64_read(&ai_sched_ctx.predictions_made));
	len += sprintf(buf + len, "predictions_correct=%llu\n", 
		       atomic64_read(&ai_sched_ctx.predictions_correct));
	len += sprintf(buf + len, "accuracy=%llu\n", ai_get_accuracy_percentage());
	len += sprintf(buf + len, "decisions_made=%llu\n", decisions_made);
	len += sprintf(buf + len, "decisions_accepted=%llu\n", decisions_accepted);
	len += sprintf(buf + len, "decision_success_rate=%u\n", success_rate);
	len += sprintf(buf + len, "learning_samples=%llu\n", 
		       atomic64_read(&ai_sched_ctx.learning_samples));
	len += sprintf(buf + len, "model_updates=%llu\n", 
		       atomic64_read(&ai_sched_ctx.model_updates));
	len += sprintf(buf + len, "num_clusters=%u\n", ai_sched_ctx.num_clusters);
	
	return len;
}

/**
 * ai_sysfs_debug_level_show - Show debug level
 */
static ssize_t ai_sysfs_debug_level_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf, "%u\n", ai_sched_ctx.debug_level);
}

/**
 * ai_sysfs_debug_level_store - Set debug level
 */
static ssize_t ai_sysfs_debug_level_store(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  const char *buf, size_t count)
{
	unsigned int level;
	
	if (kstrtouint(buf, 10, &level) != 0 || level > 4)
		return -EINVAL;
	
	ai_sched_ctx.debug_level = level;
	ai_info("Debug level set to %u", level);
	
	return count;
}

/**
 * ai_perf_test_worker - Performance test worker function
 */
static void ai_perf_test_worker(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct task_struct *current_task = current;
	ktime_t start_time, end_time;
	u64 elapsed_ns;
	enum ai_task_type predicted_type;
	int from_cpu, to_cpu;
	u32 confidence, efficiency;
	
	if (!ai_perf_test_ctx.running)
		return;
	
	ai_perf_test_ctx.current_iteration++;
	
	/* Test 1: Prediction performance */
	start_time = ktime_get();
	predicted_type = ai_predict_task_type(&current_task->ai_features);
	confidence = ai_calculate_confidence(&current_task->ai_features);
	end_time = ktime_get();
	elapsed_ns = ktime_to_ns(ktime_sub(end_time, start_time));
	
	ai_perf_test_ctx.total_prediction_time += elapsed_ns;
	ai_perf_test_ctx.predictions_made++;
	ai_debug_log_prediction(current_task, predicted_type, confidence, elapsed_ns);
	
	/* Test 2: Migration performance */
	from_cpu = smp_processor_id();
	start_time = ktime_get();
	to_cpu = ai_select_cpu(current_task, from_cpu);
	end_time = ktime_get();
	elapsed_ns = ktime_to_ns(ktime_sub(end_time, start_time));
	
	if (to_cpu != from_cpu) {
		efficiency = 75; /* Simulated efficiency */
		ai_perf_test_ctx.total_migration_time += elapsed_ns;
		ai_perf_test_ctx.migrations_made++;
		ai_debug_log_migration(current_task, from_cpu, to_cpu, efficiency, elapsed_ns);
	}
	
	/* Test 3: Learning performance */
	if (ai_learning_enabled() && (ai_perf_test_ctx.current_iteration % 10 == 0)) {
		start_time = ktime_get();
		ai_update_model();
		end_time = ktime_get();
		elapsed_ns = ktime_to_ns(ktime_sub(end_time, start_time));
		
		ai_perf_test_ctx.total_learning_time += elapsed_ns;
		ai_perf_test_ctx.learning_updates++;
		ai_debug_log_learning(10, (u32)ai_get_accuracy_percentage(), elapsed_ns);
	}
	
	/* Calculate averages */
	if (ai_perf_test_ctx.predictions_made > 0) {
		ai_perf_test_ctx.avg_prediction_time_ns = 
			ai_perf_test_ctx.total_prediction_time / ai_perf_test_ctx.predictions_made;
	}
	
	if (ai_perf_test_ctx.migrations_made > 0) {
		ai_perf_test_ctx.avg_migration_time_ns = 
			ai_perf_test_ctx.total_migration_time / ai_perf_test_ctx.migrations_made;
	}
	
	if (ai_perf_test_ctx.learning_updates > 0) {
		ai_perf_test_ctx.avg_learning_time_ns = 
			ai_perf_test_ctx.total_learning_time / ai_perf_test_ctx.learning_updates;
	}
	
	/* Continue test if not finished */
	if (ai_perf_test_ctx.current_iteration < ai_perf_test_ctx.test_iterations) {
		queue_delayed_work(ai_perf_test_ctx.test_wq, 
				   &ai_perf_test_ctx.test_work,
				   msecs_to_jiffies(100));  /* 100ms between tests */
	} else {
		ai_perf_test_ctx.running = false;
		ai_info("Performance test completed: %u iterations", 
			ai_perf_test_ctx.current_iteration);
	}
}

/**
 * ai_sysfs_perf_test_show - Show performance test results
 */
static ssize_t ai_sysfs_perf_test_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	
	len += sprintf(buf + len, "running=%d\n", ai_perf_test_ctx.running ? 1 : 0);
	len += sprintf(buf + len, "iterations=%u/%u\n", 
		       ai_perf_test_ctx.current_iteration,
		       ai_perf_test_ctx.test_iterations);
	len += sprintf(buf + len, "predictions_made=%u\n", ai_perf_test_ctx.predictions_made);
	len += sprintf(buf + len, "migrations_made=%u\n", ai_perf_test_ctx.migrations_made);
	len += sprintf(buf + len, "learning_updates=%u\n", ai_perf_test_ctx.learning_updates);
	len += sprintf(buf + len, "avg_prediction_time_ns=%u\n", 
		       ai_perf_test_ctx.avg_prediction_time_ns);
	len += sprintf(buf + len, "avg_migration_time_ns=%u\n", 
		       ai_perf_test_ctx.avg_migration_time_ns);
	len += sprintf(buf + len, "avg_learning_time_ns=%u\n", 
		       ai_perf_test_ctx.avg_learning_time_ns);
	len += sprintf(buf + len, "total_prediction_time_ns=%llu\n", 
		       ai_perf_test_ctx.total_prediction_time);
	len += sprintf(buf + len, "total_migration_time_ns=%llu\n", 
		       ai_perf_test_ctx.total_migration_time);
	len += sprintf(buf + len, "total_learning_time_ns=%llu\n", 
		       ai_perf_test_ctx.total_learning_time);
	
	return len;
}

/**
 * ai_sysfs_perf_test_store - Start/stop performance test
 */
static ssize_t ai_sysfs_perf_test_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	unsigned int iterations;
	
	if (strncmp(buf, "stop", 4) == 0) {
		ai_perf_test_ctx.running = false;
		cancel_delayed_work_sync(&ai_perf_test_ctx.test_work);
		ai_info("Performance test stopped");
		return count;
	}
	
	if (kstrtouint(buf, 10, &iterations) != 0 || iterations == 0 || iterations > 10000)
		return -EINVAL;
	
	if (ai_perf_test_ctx.running) {
		ai_warn("Performance test already running");
		return -EBUSY;
	}
	
	/* Reset test context */
	memset(&ai_perf_test_ctx, 0, sizeof(ai_perf_test_ctx));
	ai_perf_test_ctx.test_iterations = iterations;
	ai_perf_test_ctx.running = true;
	
	/* Start test */
	queue_delayed_work(ai_perf_test_ctx.test_wq, 
			   &ai_perf_test_ctx.test_work, 0);
	
	ai_info("Started performance test with %u iterations", iterations);
	
	return count;
}

/**
 * ai_sysfs_benchmark_show - Show benchmark results
 */
static ssize_t ai_sysfs_benchmark_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	ssize_t len = 0;
	u64 accuracy = ai_get_accuracy_percentage();
	u64 decisions_made, decisions_accepted;
	u32 success_rate;
	
	ai_get_integration_stats(&decisions_made, &decisions_accepted, &success_rate);
	
	len += sprintf(buf + len, "=== AI Scheduler Benchmark ===\n");
	len += sprintf(buf + len, "Prediction Accuracy: %llu%%\n", accuracy);
	len += sprintf(buf + len, "Decision Success Rate: %u%%\n", success_rate);
	len += sprintf(buf + len, "Performance Score: ");
	
	/* Calculate overall performance score */
	u32 perf_score = (accuracy + success_rate) / 2;
	if (perf_score >= 90)
		len += sprintf(buf + len, "%u%% (Excellent)\n", perf_score);
	else if (perf_score >= 75)
		len += sprintf(buf + len, "%u%% (Good)\n", perf_score);
	else if (perf_score >= 60)
		len += sprintf(buf + len, "%u%% (Fair)\n", perf_score);
	else
		len += sprintf(buf + len, "%u%% (Poor)\n", perf_score);
	
	len += sprintf(buf + len, "Total Predictions: %llu\n", 
		       atomic64_read(&ai_sched_ctx.predictions_made));
	len += sprintf(buf + len, "Total Decisions: %llu\n", decisions_made);
	len += sprintf(buf + len, "Learning Samples: %llu\n", 
		       atomic64_read(&ai_sched_ctx.learning_samples));
	
	/* Performance recommendations */
	len += sprintf(buf + len, "\nRecommendations:\n");
	if (accuracy < 70)
		len += sprintf(buf + len, "- Increase learning samples for better accuracy\n");
	if (success_rate < 80)
		len += sprintf(buf + len, "- Tune CPU selection algorithm\n");
	if (atomic64_read(&ai_sched_ctx.learning_samples) < 1000)
		len += sprintf(buf + len, "- Allow more time for learning\n");
	
	return len;
}

/* SysFS attributes */
static struct kobj_attribute ai_enabled_attr = 
	__ATTR(enabled, 0644, ai_sysfs_enabled_show, ai_sysfs_enabled_store);

static struct kobj_attribute ai_learning_attr = 
	__ATTR(learning, 0644, ai_sysfs_learning_show, ai_sysfs_learning_store);

static struct kobj_attribute ai_accuracy_attr = 
	__ATTR(accuracy, 0444, ai_sysfs_accuracy_show, NULL);

static struct kobj_attribute ai_stats_attr = 
	__ATTR(stats, 0444, ai_sysfs_stats_show, NULL);

static struct kobj_attribute ai_debug_level_attr = 
	__ATTR(debug_level, 0644, ai_sysfs_debug_level_show, ai_sysfs_debug_level_store);

static struct kobj_attribute ai_perf_test_attr = 
	__ATTR(test, 0644, ai_sysfs_perf_test_show, ai_sysfs_perf_test_store);

static struct kobj_attribute ai_benchmark_attr = 
	__ATTR(benchmark, 0444, ai_sysfs_benchmark_show, NULL);

/* Attribute groups */
static struct attribute *ai_attrs[] = {
	&ai_enabled_attr.attr,
	&ai_learning_attr.attr,
	&ai_accuracy_attr.attr,
	&ai_stats_attr.attr,
	&ai_debug_level_attr.attr,
	NULL,
};

static struct attribute *ai_perf_attrs[] = {
	&ai_perf_test_attr.attr,
	&ai_benchmark_attr.attr,
	NULL,
};

static struct attribute_group ai_attr_group = {
	.attrs = ai_attrs,
};

static struct attribute_group ai_perf_attr_group = {
	.attrs = ai_perf_attrs,
};

/**
 * ai_sysfs_init - Initialize sysfs interface
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_sysfs_init(void)
{
	int ret;
	
	/* Create main kobject */
	ai_sysfs_kobj = kobject_create_and_add("ai_scheduler", kernel_kobj);
	if (!ai_sysfs_kobj) {
		ai_error("Failed to create ai_scheduler sysfs directory");
		return -ENOMEM;
	}
	
	/* Create main attribute group */
	ret = sysfs_create_group(ai_sysfs_kobj, &ai_attr_group);
	if (ret) {
		ai_error("Failed to create ai_scheduler sysfs attributes");
		goto error_main;
	}
	
	/* Create performance subdirectory */
	ai_perf_kobj = kobject_create_and_add("performance", ai_sysfs_kobj);
	if (!ai_perf_kobj) {
		ai_error("Failed to create performance sysfs directory");
		ret = -ENOMEM;
		goto error_perf_dir;
	}
	
	/* Create performance attributes */
	ret = sysfs_create_group(ai_perf_kobj, &ai_perf_attr_group);
	if (ret) {
		ai_error("Failed to create performance sysfs attributes");
		goto error_perf_attrs;
	}
	
	/* Initialize performance test context */
	ai_perf_test_ctx.test_wq = create_singlethread_workqueue("ai_perf_test");
	if (!ai_perf_test_ctx.test_wq) {
		ai_error("Failed to create performance test work queue");
		ret = -ENOMEM;
		goto error_wq;
	}
	
	INIT_DELAYED_WORK(&ai_perf_test_ctx.test_work, ai_perf_test_worker);
	
	ai_info("SysFS interface created at /sys/kernel/ai_scheduler/");
	return 0;

error_wq:
	sysfs_remove_group(ai_perf_kobj, &ai_perf_attr_group);
error_perf_attrs:
	kobject_put(ai_perf_kobj);
error_perf_dir:
	sysfs_remove_group(ai_sysfs_kobj, &ai_attr_group);
error_main:
	kobject_put(ai_sysfs_kobj);
	return ret;
}

/**
 * ai_sysfs_cleanup - Cleanup sysfs interface
 */
void ai_sysfs_cleanup(void)
{
	/* Stop any running performance test */
	if (ai_perf_test_ctx.running) {
		ai_perf_test_ctx.running = false;
		cancel_delayed_work_sync(&ai_perf_test_ctx.test_work);
	}
	
	/* Destroy work queue */
	if (ai_perf_test_ctx.test_wq) {
		destroy_workqueue(ai_perf_test_ctx.test_wq);
		ai_perf_test_ctx.test_wq = NULL;
	}
	
	/* Remove sysfs entries */
	if (ai_perf_kobj) {
		sysfs_remove_group(ai_perf_kobj, &ai_perf_attr_group);
		kobject_put(ai_perf_kobj);
		ai_perf_kobj = NULL;
	}
	
	if (ai_sysfs_kobj) {
		sysfs_remove_group(ai_sysfs_kobj, &ai_attr_group);
		kobject_put(ai_sysfs_kobj);
		ai_sysfs_kobj = NULL;
	}
	
	ai_info("SysFS interface cleaned up");
}