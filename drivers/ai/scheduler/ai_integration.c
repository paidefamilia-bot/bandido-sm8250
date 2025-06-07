/*
 * AI Scheduler Integration with WALT
 * Integration layer between AI scheduler and existing WALT scheduler
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/signal.h>
#include <linux/cpumask.h>
#include <linux/cpufreq.h>
#include <linux/topology.h>
#include <linux/energy_model.h>
#include <linux/pm_qos.h>
#include <linux/kthread.h>
#include <linux/atomic.h>
#include <linux/jiffies.h>

#include "ai_scheduler.h"

/* Integration state */
static atomic_t ai_integration_active = ATOMIC_INIT(0);
static atomic_t ai_decisions_made = ATOMIC_INIT(0);
static atomic_t ai_decisions_accepted = ATOMIC_INIT(0);

/* Performance tracking */
struct ai_performance_tracker {
	u64 total_migrations;
	u64 successful_migrations;
	u64 performance_improvements;
	u64 energy_savings;
	u32 average_response_time;
	u32 system_responsiveness_score;
	spinlock_t lock;
};

static struct ai_performance_tracker ai_perf_tracker;

/* CPU frequency boost requests */
struct ai_freq_request {
	struct freq_qos_request qos_req;
	int cpu;
	u32 target_freq;
	bool active;
};

static struct ai_freq_request ai_freq_requests[NR_CPUS];

/**
 * ai_get_task_load_avg - Get task load average
 * @task: Target task
 * 
 * Returns: Task load average
 */
static unsigned long ai_get_task_load_avg(struct task_struct *task)
{
	if (!task || !task->se.avg.load_avg)
		return 0;
	
	return task->se.avg.load_avg;
}

/**
 * ai_get_cpu_capacity - Get CPU capacity
 * @cpu: Target CPU
 * 
 * Returns: CPU capacity
 */
static unsigned long ai_get_cpu_capacity(int cpu)
{
	return capacity_of(cpu);
}

/**
 * ai_get_cpu_utilization - Get current CPU utilization
 * @cpu: Target CPU
 * 
 * Returns: CPU utilization (0-1024)
 */
static unsigned long ai_get_cpu_utilization(int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	
	if (!rq)
		return 1024;  /* Assume full utilization if unknown */
	
	return rq->avg_rt.util_avg + rq->avg_dl.util_avg + rq->cfs.avg.util_avg;
}

/**
 * ai_calculate_cpu_efficiency - Calculate CPU efficiency for a task
 * @cpu: Target CPU
 * @task: Task to place
 * 
 * Returns: Efficiency score (0-1000)
 */
static u32 ai_calculate_cpu_efficiency(int cpu, struct task_struct *task)
{
	struct ai_task_data *data;
	unsigned long cpu_cap, cpu_util, task_load;
	u32 efficiency = 500;  /* Base efficiency */
	
	if (!cpu_online(cpu))
		return 0;
	
	data = ai_get_task_data(task);
	if (!data) {
		/* Fallback to basic calculation */
		cpu_cap = ai_get_cpu_capacity(cpu);
		cpu_util = ai_get_cpu_utilization(cpu);
		task_load = ai_get_task_load_avg(task);
		
		if (cpu_cap > 0) {
			/* Prefer less utilized CPUs */
			efficiency = 1000 - (cpu_util * 1000 / cpu_cap);
			
			/* Adjust for task load */
			if (task_load > 0 && cpu_util + task_load <= cpu_cap) {
				efficiency += 100;  /* Bonus for good fit */
			}
		}
		
		return min(efficiency, 1000U);
	}
	
	/* AI-enhanced calculation */
	cpu_cap = ai_get_cpu_capacity(cpu);
	cpu_util = ai_get_cpu_utilization(cpu);
	
	/* Check if CPU matches task requirements */
	enum ai_performance_level required_perf = data->required_perf;
	
	/* Find the cluster this CPU belongs to */
	int cluster_idx = -1;
	for (int i = 0; i < ai_sched_ctx.num_clusters; i++) {
		if (cpumask_test_cpu(cpu, &ai_sched_ctx.clusters[i].cpus)) {
			cluster_idx = i;
			break;
		}
	}
	
	if (cluster_idx >= 0) {
		struct ai_cpu_cluster *cluster = &ai_sched_ctx.clusters[cluster_idx];
		
		/* Match performance requirements */
		switch (required_perf) {
		case AI_PERF_GAMING:
			if (cluster->performance_capability >= 80)
				efficiency += 200;
			else
				efficiency -= 200;
			break;
			
		case AI_PERF_PERFORMANCE:
			if (cluster->performance_capability >= 60)
				efficiency += 150;
			else
				efficiency -= 100;
			break;
			
		case AI_PERF_BALANCED:
			if (cluster->performance_capability >= 40 && 
			    cluster->performance_capability <= 80)
				efficiency += 100;
			break;
			
		case AI_PERF_POWERSAVE:
			if (cluster->power_efficiency >= 70)
				efficiency += 150;
			else
				efficiency -= 100;
			break;
			
		default:
			break;
		}
		
		/* Utilization penalty */
		if (cpu_cap > 0) {
			u32 util_percent = (cpu_util * 100) / cpu_cap;
			if (util_percent > 80)
				efficiency -= 200;
			else if (util_percent > 60)
				efficiency -= 100;
			else if (util_percent < 20)
				efficiency += 50;
		}
		
		/* Gaming tasks prefer big cores */
		if (data->current_type == AI_TASK_GAMING) {
			if (cluster->performance_capability >= 80)
				efficiency += 300;
			else
				efficiency -= 300;
		}
		
		/* Background tasks prefer little cores */
		if (data->current_type == AI_TASK_BACKGROUND) {
			if (cluster->power_efficiency >= 70)
				efficiency += 200;
			else
				efficiency -= 100;
		}
		
		/* Interactive tasks prefer medium cores */
		if (data->current_type == AI_TASK_INTERACTIVE) {
			if (cluster->performance_capability >= 50 && 
			    cluster->performance_capability <= 80)
				efficiency += 150;
		}
	}
	
	ai_put_task_data(data);
	
	return clamp(efficiency, 0U, 1000U);
}

/**
 * ai_select_cpu - AI-powered CPU selection
 * @task: Task to place
 * @prev_cpu: Previous CPU
 * 
 * Returns: Selected CPU or prev_cpu if no better option
 */
int ai_select_cpu(struct task_struct *task, int prev_cpu)
{
	int best_cpu = prev_cpu;
	u32 best_efficiency = 0;
	u32 efficiency;
	int cpu;
	
	if (!ai_scheduler_enabled() || !atomic_read(&ai_integration_active))
		return prev_cpu;
	
	/* Quick check if current CPU is still good */
	if (cpu_online(prev_cpu)) {
		best_efficiency = ai_calculate_cpu_efficiency(prev_cpu, task);
		
		/* If current CPU is very good, stick with it */
		if (best_efficiency >= 800) {
			atomic_inc(&ai_decisions_made);
			atomic_inc(&ai_decisions_accepted);
			return prev_cpu;
		}
	}
	
	/* Search for better CPU */
	for_each_online_cpu(cpu) {
		/* Skip if not allowed */
		if (!cpumask_test_cpu(cpu, &task->cpus_allowed))
			continue;
		
		efficiency = ai_calculate_cpu_efficiency(cpu, task);
		
		if (efficiency > best_efficiency) {
			best_efficiency = efficiency;
			best_cpu = cpu;
		}
	}
	
	atomic_inc(&ai_decisions_made);
	
	/* Only migrate if significantly better */
	if (best_cpu != prev_cpu && best_efficiency > 600) {
		atomic_inc(&ai_decisions_accepted);
		
		ai_verbose("AI CPU selection: %s[%d] %d->%d (efficiency: %u)",
			   task->comm, task->pid, prev_cpu, best_cpu, best_efficiency);
		
		/* Track migration */
		spin_lock(&ai_perf_tracker.lock);
		ai_perf_tracker.total_migrations++;
		if (best_efficiency > 700)
			ai_perf_tracker.successful_migrations++;
		spin_unlock(&ai_perf_tracker.lock);
		
		return best_cpu;
	}
	
	return prev_cpu;
}

/**
 * ai_boost_task_performance - Boost performance for important tasks
 * @task: Task to boost
 */
void ai_boost_task_performance(struct task_struct *task)
{
	struct ai_task_data *data;
	int cpu;
	
	if (!ai_scheduler_enabled() || !task)
		return;
	
	data = ai_get_task_data(task);
	if (!data)
		return;
	
	/* Only boost gaming and interactive tasks */
	if (data->current_type != AI_TASK_GAMING && 
	    data->current_type != AI_TASK_INTERACTIVE) {
		ai_put_task_data(data);
		return;
	}
	
	cpu = task_cpu(task);
	if (!cpu_online(cpu)) {
		ai_put_task_data(data);
		return;
	}
	
	/* Request frequency boost */
	ai_optimize_cpu_frequency(cpu);
	
	ai_verbose("Boosted performance for %s[%d] on CPU %d",
		   task->comm, task->pid, cpu);
	
	ai_put_task_data(data);
}

/**
 * ai_optimize_cpu_frequency - Optimize CPU frequency for current workload
 * @cpu: Target CPU
 */
void ai_optimize_cpu_frequency(int cpu)
{
	struct cpufreq_policy *policy;
	struct ai_freq_request *req;
	u32 target_freq;
	unsigned long cpu_util;
	
	if (!cpu_online(cpu) || cpu >= NR_CPUS)
		return;
	
	policy = cpufreq_cpu_get(cpu);
	if (!policy)
		return;
	
	req = &ai_freq_requests[cpu];
	cpu_util = ai_get_cpu_utilization(cpu);
	
	/* Calculate target frequency based on utilization */
	if (cpu_util > 800) {
		/* High utilization - boost to max */
		target_freq = policy->cpuinfo.max_freq;
	} else if (cpu_util > 500) {
		/* Medium utilization - scale proportionally */
		target_freq = policy->cpuinfo.min_freq + 
			      ((policy->cpuinfo.max_freq - policy->cpuinfo.min_freq) * 
			       cpu_util) / 1024;
	} else {
		/* Low utilization - use minimum */
		target_freq = policy->cpuinfo.min_freq;
	}
	
	/* Apply frequency request if changed significantly */
	if (!req->active || abs((int)target_freq - (int)req->target_freq) > 100000) {
		if (req->active) {
			freq_qos_remove_request(&req->qos_req);
			req->active = false;
		}
		
		if (freq_qos_add_request(&policy->constraints, &req->qos_req,
					 FREQ_QOS_MIN, target_freq) >= 0) {
			req->cpu = cpu;
			req->target_freq = target_freq;
			req->active = true;
			
			ai_verbose("Set CPU %d frequency target: %u kHz", cpu, target_freq);
		}
	}
	
	cpufreq_cpu_put(policy);
}

/**
 * ai_task_tick - Called on each scheduler tick for AI tasks
 * @task: Current task
 */
void ai_task_tick(struct task_struct *task)
{
	struct ai_task_data *data;
	static unsigned long last_update = 0;
	
	if (!ai_scheduler_enabled())
		return;
	
	/* Rate limit updates */
	if (time_before(jiffies, last_update + HZ))
		return;
	
	last_update = jiffies;
	
	data = ai_get_task_data(task);
	if (!data)
		return;
	
	/* Update task features */
	ai_update_task_features(data);
	
	/* Add training sample if learning is enabled */
	if (ai_learning_enabled()) {
		ai_add_training_sample(data);
	}
	
	ai_put_task_data(data);
}

/**
 * ai_enqueue_task - Called when task is enqueued
 * @task: Task being enqueued
 */
void ai_enqueue_task(struct task_struct *task)
{
	if (!ai_scheduler_enabled())
		return;
	
	/* Register task for AI monitoring */
	ai_register_task(task);
	
	/* Boost performance if needed */
	ai_boost_task_performance(task);
	
	ai_verbose("Enqueued task %s[%d] for AI monitoring", task->comm, task->pid);
}

/**
 * ai_dequeue_task - Called when task is dequeued
 * @task: Task being dequeued
 */
void ai_dequeue_task(struct task_struct *task)
{
	if (!ai_scheduler_enabled())
		return;
	
	/* Task is going to sleep, good time to update features */
	struct ai_task_data *data = ai_get_task_data(task);
	if (data) {
		ai_update_task_features(data);
		ai_put_task_data(data);
	}
	
	ai_verbose("Dequeued task %s[%d]", task->comm, task->pid);
}

/**
 * ai_balance_system_load - Balance system load across CPUs
 */
void ai_balance_system_load(void)
{
	int cpu;
	unsigned long total_util = 0, avg_util;
	int num_cpus = 0;
	
	if (!ai_scheduler_enabled())
		return;
	
	/* Calculate average utilization */
	for_each_online_cpu(cpu) {
		total_util += ai_get_cpu_utilization(cpu);
		num_cpus++;
	}
	
	if (num_cpus == 0)
		return;
	
	avg_util = total_util / num_cpus;
	
	/* Optimize frequency for each CPU */
	for_each_online_cpu(cpu) {
		unsigned long cpu_util = ai_get_cpu_utilization(cpu);
		
		/* If CPU is significantly above average, optimize it */
		if (cpu_util > avg_util + 200) {
			ai_optimize_cpu_frequency(cpu);
		}
	}
	
	ai_verbose("Balanced system load, avg_util=%lu", avg_util);
}

/**
 * ai_integration_enable - Enable AI scheduler integration
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_integration_enable(void)
{
	int cpu;
	
	if (atomic_read(&ai_integration_active))
		return 0;
	
	/* Initialize performance tracker */
	spin_lock_init(&ai_perf_tracker.lock);
	memset(&ai_perf_tracker, 0, sizeof(ai_perf_tracker));
	
	/* Initialize frequency requests */
	for_each_possible_cpu(cpu) {
		struct ai_freq_request *req = &ai_freq_requests[cpu];
		memset(req, 0, sizeof(*req));
		req->cpu = cpu;
	}
	
	atomic_set(&ai_integration_active, 1);
	
	ai_info("AI scheduler integration enabled");
	return 0;
}

/**
 * ai_integration_disable - Disable AI scheduler integration
 */
static void ai_integration_disable(void)
{
	int cpu;
	
	if (!atomic_read(&ai_integration_active))
		return;
	
	atomic_set(&ai_integration_active, 0);
	
	/* Remove frequency requests */
	for_each_possible_cpu(cpu) {
		struct ai_freq_request *req = &ai_freq_requests[cpu];
		if (req->active) {
			freq_qos_remove_request(&req->qos_req);
			req->active = false;
		}
	}
	
	ai_info("AI scheduler integration disabled");
}

/**
 * ai_get_integration_stats - Get integration statistics
 * @decisions_made: Output for total decisions made
 * @decisions_accepted: Output for decisions accepted
 * @success_rate: Output for success rate percentage
 */
void ai_get_integration_stats(u64 *decisions_made, u64 *decisions_accepted, u32 *success_rate)
{
	u64 made = atomic_read(&ai_decisions_made);
	u64 accepted = atomic_read(&ai_decisions_accepted);
	
	if (decisions_made)
		*decisions_made = made;
	
	if (decisions_accepted)
		*decisions_accepted = accepted;
	
	if (success_rate) {
		if (made > 0)
			*success_rate = (accepted * 100) / made;
		else
			*success_rate = 0;
	}
}

/**
 * ai_integration_init - Initialize AI scheduler integration
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_integration_init(void)
{
	int ret;
	
	ret = ai_integration_enable();
	if (ret) {
		ai_error("Failed to enable AI integration: %d", ret);
		return ret;
	}
	
	ai_info("AI scheduler integration initialized");
	return 0;
}

/**
 * ai_integration_exit - Cleanup AI scheduler integration
 */
void ai_integration_exit(void)
{
	ai_integration_disable();
	ai_info("AI scheduler integration cleaned up");
}

/* Export symbols */
EXPORT_SYMBOL(ai_select_cpu);
EXPORT_SYMBOL(ai_task_tick);
EXPORT_SYMBOL(ai_enqueue_task);
EXPORT_SYMBOL(ai_dequeue_task);
EXPORT_SYMBOL(ai_boost_task_performance);
EXPORT_SYMBOL(ai_optimize_cpu_frequency);
EXPORT_SYMBOL(ai_balance_system_load);
EXPORT_SYMBOL(ai_get_integration_stats);