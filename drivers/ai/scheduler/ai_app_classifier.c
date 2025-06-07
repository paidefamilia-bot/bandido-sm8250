/*
 * AI Scheduler App Classification System
 * Advanced application classification and behavior prediction
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/hash.h>
#include <linux/rculist.h>
#include <linux/sort.h>
#include <linux/math64.h>

#include "ai_scheduler.h"

/* App classification configuration */
#define AI_APP_HASH_BITS		8
#define AI_APP_HASH_SIZE		(1 << AI_APP_HASH_BITS)
#define AI_APP_CATEGORIES		20
#define AI_APP_FEATURES			32
#define AI_APP_LEARNING_SAMPLES		50
#define AI_APP_CONFIDENCE_THRESHOLD	70

/* Application categories */
enum ai_app_category {
	AI_APP_GAMING = 0,
	AI_APP_SOCIAL_MEDIA,
	AI_APP_COMMUNICATION,
	AI_APP_PRODUCTIVITY,
	AI_APP_ENTERTAINMENT,
	AI_APP_NAVIGATION,
	AI_APP_PHOTOGRAPHY,
	AI_APP_SHOPPING,
	AI_APP_NEWS,
	AI_APP_EDUCATION,
	AI_APP_HEALTH_FITNESS,
	AI_APP_FINANCE,
	AI_APP_TRAVEL,
	AI_APP_FOOD_DRINK,
	AI_APP_MUSIC_AUDIO,
	AI_APP_VIDEO_STREAMING,
	AI_APP_BROWSER,
	AI_APP_SYSTEM,
	AI_APP_UTILITY,
	AI_APP_UNKNOWN
};

/* App behavior characteristics */
enum ai_app_behavior {
	AI_BEHAVIOR_CPU_INTENSIVE = 0,
	AI_BEHAVIOR_MEMORY_INTENSIVE,
	AI_BEHAVIOR_IO_INTENSIVE,
	AI_BEHAVIOR_NETWORK_INTENSIVE,
	AI_BEHAVIOR_INTERACTIVE,
	AI_BEHAVIOR_BACKGROUND,
	AI_BEHAVIOR_REAL_TIME,
	AI_BEHAVIOR_BATCH_PROCESSING,
	AI_BEHAVIOR_MAX
};

/* App performance requirements */
enum ai_app_performance_req {
	AI_PERF_REQ_LOW = 0,
	AI_PERF_REQ_MEDIUM,
	AI_PERF_REQ_HIGH,
	AI_PERF_REQ_CRITICAL
};

/* App feature vector */
struct ai_app_features {
	/* Resource usage patterns */
	u32 avg_cpu_usage;		/* Average CPU usage (%) */
	u32 peak_cpu_usage;		/* Peak CPU usage (%) */
	u32 avg_memory_usage;		/* Average memory usage (KB) */
	u32 peak_memory_usage;		/* Peak memory usage (KB) */
	u32 avg_io_rate;		/* Average I/O rate (KB/s) */
	u32 network_usage;		/* Network usage level */
	
	/* Interaction patterns */
	u32 user_interaction_freq;	/* User interaction frequency */
	u32 session_length_avg;		/* Average session length (ms) */
	u32 launch_frequency;		/* How often app is launched */
	u32 background_time_ratio;	/* Time spent in background (%) */
	
	/* Performance characteristics */
	u32 startup_time;		/* App startup time (ms) */
	u32 response_time;		/* UI response time (ms) */
	u32 frame_rate;			/* UI frame rate (fps) */
	u32 power_consumption;		/* Power consumption level */
	
	/* System impact */
	u32 context_switches;		/* Context switches per second */
	u32 page_faults;		/* Page faults per second */
	u32 syscall_rate;		/* System calls per second */
	u32 thermal_impact;		/* Thermal impact level */
	
	/* Temporal patterns */
	u32 time_of_day_usage[24];	/* Usage by hour of day */
	u32 day_of_week_usage[7];	/* Usage by day of week */
	u32 usage_duration_variance;	/* Variance in usage duration */
	u32 usage_frequency_trend;	/* Trend in usage frequency */
	
	/* Behavioral indicators */
	bool is_foreground_heavy;	/* Primarily foreground app */
	bool is_notification_heavy;	/* Generates many notifications */
	bool is_location_aware;		/* Uses location services */
	bool is_camera_heavy;		/* Heavy camera usage */
	bool is_audio_heavy;		/* Heavy audio usage */
	bool is_graphics_intensive;	/* Graphics intensive */
	bool is_multithreaded;		/* Uses multiple threads */
	bool is_real_time;		/* Real-time requirements */
};

/* App classification entry */
struct ai_app_classification {
	char package_name[64];		/* App package name */
	char display_name[32];		/* App display name */
	u32 package_hash;		/* Hash of package name */
	
	/* Classification results */
	enum ai_app_category category;
	enum ai_app_behavior primary_behavior;
	enum ai_app_performance_req performance_req;
	u32 classification_confidence;
	
	/* Feature data */
	struct ai_app_features features;
	struct ai_app_features features_avg;	/* Running average */
	
	/* Learning data */
	u32 sample_count;
	u32 learning_iterations;
	u32 classification_accuracy;
	
	/* Usage statistics */
	u64 total_runtime;
	u64 total_launches;
	u64 last_launch_time;
	u32 avg_session_length;
	
	/* Performance metrics */
	u32 performance_score;
	u32 efficiency_score;
	u32 user_satisfaction_score;
	
	/* Prediction state */
	enum ai_app_category predicted_category;
	u32 prediction_confidence;
	u64 prediction_timestamp;
	
	struct hlist_node hlist;
	struct rcu_head rcu;
	atomic_t refcount;
};

/* App pattern database entry */
struct ai_app_pattern {
	char pattern[32];		/* Name/package pattern */
	enum ai_app_category category;
	enum ai_app_behavior behavior;
	enum ai_app_performance_req performance_req;
	u32 confidence;
	struct list_head list;
};

/* App classification context */
struct ai_app_classifier_ctx {
	/* Hash table for app classifications */
	struct hlist_head app_hash_table[AI_APP_HASH_SIZE];
	spinlock_t hash_lock;
	u32 total_apps;
	
	/* Pattern database */
	struct list_head app_patterns;
	spinlock_t patterns_lock;
	u32 pattern_count;
	
	/* Learning state */
	bool learning_enabled;
	bool classification_active;
	u32 learning_threshold;
	
	/* Statistics */
	atomic64_t apps_classified;
	atomic64_t classifications_updated;
	atomic64_t predictions_made;
	atomic64_t predictions_correct;
	
	/* Performance tracking */
	u32 avg_classification_time;
	u32 classification_accuracy;
};

static struct ai_app_classifier_ctx app_classifier_ctx;

/* Category names */
static const char *app_category_names[] = {
	"GAMING",
	"SOCIAL_MEDIA",
	"COMMUNICATION",
	"PRODUCTIVITY",
	"ENTERTAINMENT",
	"NAVIGATION",
	"PHOTOGRAPHY",
	"SHOPPING",
	"NEWS",
	"EDUCATION",
	"HEALTH_FITNESS",
	"FINANCE",
	"TRAVEL",
	"FOOD_DRINK",
	"MUSIC_AUDIO",
	"VIDEO_STREAMING",
	"BROWSER",
	"SYSTEM",
	"UTILITY",
	"UNKNOWN"
};

/* Behavior names */
static const char *app_behavior_names[] = {
	"CPU_INTENSIVE",
	"MEMORY_INTENSIVE",
	"IO_INTENSIVE",
	"NETWORK_INTENSIVE",
	"INTERACTIVE",
	"BACKGROUND",
	"REAL_TIME",
	"BATCH_PROCESSING"
};

/* Built-in app patterns */
static struct ai_app_pattern builtin_patterns[] = {
	/* Gaming */
	{"unity", AI_APP_GAMING, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"unreal", AI_APP_GAMING, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"game", AI_APP_GAMING, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_HIGH, 85},
	{"pubg", AI_APP_GAMING, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_CRITICAL, 98},
	{"fortnite", AI_APP_GAMING, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_CRITICAL, 98},
	{"minecraft", AI_APP_GAMING, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"clash", AI_APP_GAMING, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_MEDIUM, 90},
	
	/* Social Media */
	{"facebook", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 95},
	{"instagram", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 95},
	{"twitter", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 90},
	{"tiktok", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_CPU_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"snapchat", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_MEDIUM, 90},
	{"linkedin", AI_APP_SOCIAL_MEDIA, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_LOW, 85},
	
	/* Communication */
	{"whatsapp", AI_APP_COMMUNICATION, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 95},
	{"telegram", AI_APP_COMMUNICATION, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 95},
	{"messenger", AI_APP_COMMUNICATION, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 90},
	{"discord", AI_APP_COMMUNICATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_HIGH, 90},
	{"skype", AI_APP_COMMUNICATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_HIGH, 85},
	{"zoom", AI_APP_COMMUNICATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_CRITICAL, 95},
	
	/* Entertainment */
	{"youtube", AI_APP_VIDEO_STREAMING, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"netflix", AI_APP_VIDEO_STREAMING, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_HIGH, 95},
	{"spotify", AI_APP_MUSIC_AUDIO, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_MEDIUM, 95},
	{"music", AI_APP_MUSIC_AUDIO, AI_BEHAVIOR_IO_INTENSIVE, AI_PERF_REQ_LOW, 80},
	
	/* Productivity */
	{"office", AI_APP_PRODUCTIVITY, AI_BEHAVIOR_MEMORY_INTENSIVE, AI_PERF_REQ_MEDIUM, 90},
	{"docs", AI_APP_PRODUCTIVITY, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_MEDIUM, 85},
	{"sheets", AI_APP_PRODUCTIVITY, AI_BEHAVIOR_MEMORY_INTENSIVE, AI_PERF_REQ_MEDIUM, 85},
	{"email", AI_APP_PRODUCTIVITY, AI_BEHAVIOR_NETWORK_INTENSIVE, AI_PERF_REQ_LOW, 80},
	
	/* Navigation */
	{"maps", AI_APP_NAVIGATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_HIGH, 95},
	{"navigation", AI_APP_NAVIGATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_HIGH, 90},
	{"waze", AI_APP_NAVIGATION, AI_BEHAVIOR_REAL_TIME, AI_PERF_REQ_HIGH, 95},
	
	/* Browser */
	{"browser", AI_APP_BROWSER, AI_BEHAVIOR_MEMORY_INTENSIVE, AI_PERF_REQ_MEDIUM, 85},
	{"chrome", AI_APP_BROWSER, AI_BEHAVIOR_MEMORY_INTENSIVE, AI_PERF_REQ_MEDIUM, 90},
	{"firefox", AI_APP_BROWSER, AI_BEHAVIOR_MEMORY_INTENSIVE, AI_PERF_REQ_MEDIUM, 85},
	
	/* System */
	{"system", AI_APP_SYSTEM, AI_BEHAVIOR_BACKGROUND, AI_PERF_REQ_LOW, 90},
	{"launcher", AI_APP_SYSTEM, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_MEDIUM, 85},
	{"settings", AI_APP_SYSTEM, AI_BEHAVIOR_INTERACTIVE, AI_PERF_REQ_LOW, 80},
};

/**
 * ai_hash_package_name - Generate hash for package name
 * @package_name: Package name to hash
 * 
 * Returns: Hash value
 */
static u32 ai_hash_package_name(const char *package_name)
{
	return hash_str(package_name, AI_APP_HASH_BITS);
}

/**
 * ai_match_app_pattern - Match app against known patterns
 * @package_name: Package name to match
 * @category: Output for matched category
 * @behavior: Output for matched behavior
 * @performance_req: Output for performance requirement
 * 
 * Returns: Confidence level (0-100)
 */
static u32 ai_match_app_pattern(const char *package_name, enum ai_app_category *category,
				enum ai_app_behavior *behavior, 
				enum ai_app_performance_req *performance_req)
{
	struct ai_app_pattern *pattern;
	u32 best_confidence = 0;
	int i;
	
	/* Check built-in patterns first */
	for (i = 0; i < ARRAY_SIZE(builtin_patterns); i++) {
		if (strstr(package_name, builtin_patterns[i].pattern)) {
			if (builtin_patterns[i].confidence > best_confidence) {
				best_confidence = builtin_patterns[i].confidence;
				*category = builtin_patterns[i].category;
				*behavior = builtin_patterns[i].behavior;
				*performance_req = builtin_patterns[i].performance_req;
			}
		}
	}
	
	/* Check learned patterns */
	spin_lock(&app_classifier_ctx.patterns_lock);
	list_for_each_entry(pattern, &app_classifier_ctx.app_patterns, list) {
		if (strstr(package_name, pattern->pattern)) {
			if (pattern->confidence > best_confidence) {
				best_confidence = pattern->confidence;
				*category = pattern->category;
				*behavior = pattern->behavior;
				*performance_req = pattern->performance_req;
			}
		}
	}
	spin_unlock(&app_classifier_ctx.patterns_lock);
	
	return best_confidence;
}

/**
 * ai_extract_app_features - Extract features from app behavior
 * @task: Task representing the app
 * @features: Output features structure
 */
static void ai_extract_app_features(struct task_struct *task, struct ai_app_features *features)
{
	struct ai_task_data *task_data;
	
	memset(features, 0, sizeof(*features));
	
	task_data = ai_get_task_data(task);
	if (!task_data)
		return;
	
	/* Resource usage patterns */
	features->avg_cpu_usage = task_data->current_features.cpu_usage_avg;
	features->peak_cpu_usage = task_data->current_features.cpu_usage_peak;
	features->avg_memory_usage = task_data->current_features.memory_usage;
	features->peak_memory_usage = task_data->current_features.memory_peak;
	features->avg_io_rate = task_data->current_features.io_read_rate + 
				task_data->current_features.io_write_rate;
	features->network_usage = task_data->current_features.network_activity;
	
	/* Interaction patterns */
	features->user_interaction_freq = task_data->current_features.user_interaction_score;
	features->background_time_ratio = (task_data->current_features.background_time * 100) /
					  (task_data->current_features.foreground_time + 
					   task_data->current_features.background_time + 1);
	
	/* Performance characteristics */
	features->response_time = task_data->current_features.response_time_avg / 1000; /* Convert to ms */
	features->power_consumption = task_data->current_features.power_consumption;
	
	/* System impact */
	features->context_switches = task_data->current_features.context_switches;
	features->page_faults = task_data->current_features.page_fault_rate;
	features->thermal_impact = task_data->current_features.thermal_impact;
	
	/* Behavioral indicators */
	features->is_foreground_heavy = (features->background_time_ratio < 30);
	features->is_graphics_intensive = (features->avg_cpu_usage > 60 && 
					   features->user_interaction_freq > 70);
	features->is_real_time = (features->response_time < 50); /* <50ms response time */
	features->is_multithreaded = (features->context_switches > 100);
	
	ai_put_task_data(task_data);
}

/**
 * ai_classify_app_by_features - Classify app based on extracted features
 * @features: App features
 * @category: Output category
 * @behavior: Output behavior
 * @performance_req: Output performance requirement
 * 
 * Returns: Classification confidence (0-100)
 */
static u32 ai_classify_app_by_features(const struct ai_app_features *features,
					enum ai_app_category *category,
					enum ai_app_behavior *behavior,
					enum ai_app_performance_req *performance_req)
{
	u32 confidence = 50; /* Base confidence */
	
	/* Gaming classification */
	if (features->avg_cpu_usage > 60 && features->user_interaction_freq > 80 &&
	    features->is_graphics_intensive && features->is_real_time) {
		*category = AI_APP_GAMING;
		*behavior = AI_BEHAVIOR_CPU_INTENSIVE;
		*performance_req = AI_PERF_REQ_HIGH;
		confidence = 90;
		return confidence;
	}
	
	/* Video streaming classification */
	if (features->network_usage > 50 && features->avg_cpu_usage > 40 &&
	    !features->is_foreground_heavy) {
		*category = AI_APP_VIDEO_STREAMING;
		*behavior = AI_BEHAVIOR_NETWORK_INTENSIVE;
		*performance_req = AI_PERF_REQ_HIGH;
		confidence = 85;
		return confidence;
	}
	
	/* Communication app classification */
	if (features->network_usage > 30 && features->user_interaction_freq > 60 &&
	    features->is_real_time) {
		*category = AI_APP_COMMUNICATION;
		*behavior = AI_BEHAVIOR_REAL_TIME;
		*performance_req = AI_PERF_REQ_MEDIUM;
		confidence = 80;
		return confidence;
	}
	
	/* Social media classification */
	if (features->network_usage > 40 && features->user_interaction_freq > 50) {
		*category = AI_APP_SOCIAL_MEDIA;
		*behavior = AI_BEHAVIOR_NETWORK_INTENSIVE;
		*performance_req = AI_PERF_REQ_MEDIUM;
		confidence = 75;
		return confidence;
	}
	
	/* Productivity app classification */
	if (features->avg_memory_usage > 100000 && features->user_interaction_freq > 40 &&
	    features->avg_cpu_usage < 50) {
		*category = AI_APP_PRODUCTIVITY;
		*behavior = AI_BEHAVIOR_MEMORY_INTENSIVE;
		*performance_req = AI_PERF_REQ_MEDIUM;
		confidence = 70;
		return confidence;
	}
	
	/* Background/system app classification */
	if (features->background_time_ratio > 80 && features->user_interaction_freq < 20) {
		*category = AI_APP_SYSTEM;
		*behavior = AI_BEHAVIOR_BACKGROUND;
		*performance_req = AI_PERF_REQ_LOW;
		confidence = 75;
		return confidence;
	}
	
	/* Default classification */
	*category = AI_APP_UNKNOWN;
	*behavior = AI_BEHAVIOR_INTERACTIVE;
	*performance_req = AI_PERF_REQ_MEDIUM;
	
	return confidence;
}

/**
 * ai_get_app_classification - Get or create app classification
 * @package_name: App package name
 * 
 * Returns: App classification entry
 */
static struct ai_app_classification *ai_get_app_classification(const char *package_name)
{
	struct ai_app_classification *app;
	u32 hash = ai_hash_package_name(package_name);
	
	/* Search existing classifications */
	rcu_read_lock();
	hlist_for_each_entry_rcu(app, &app_classifier_ctx.app_hash_table[hash], hlist) {
		if (!strcmp(app->package_name, package_name)) {
			if (atomic_inc_not_zero(&app->refcount)) {
				rcu_read_unlock();
				return app;
			}
		}
	}
	rcu_read_unlock();
	
	/* Create new classification */
	app = kzalloc(sizeof(*app), GFP_KERNEL);
	if (!app)
		return NULL;
	
	strncpy(app->package_name, package_name, 63);
	app->package_name[63] = '\0';
	app->package_hash = hash;
	app->category = AI_APP_UNKNOWN;
	app->primary_behavior = AI_BEHAVIOR_INTERACTIVE;
	app->performance_req = AI_PERF_REQ_MEDIUM;
	atomic_set(&app->refcount, 1);
	
	/* Add to hash table */
	spin_lock(&app_classifier_ctx.hash_lock);
	hlist_add_head_rcu(&app->hlist, &app_classifier_ctx.app_hash_table[hash]);
	app_classifier_ctx.total_apps++;
	spin_unlock(&app_classifier_ctx.hash_lock);
	
	ai_verbose("Created new app classification for %s", package_name);
	
	return app;
}

/**
 * ai_put_app_classification - Release app classification reference
 * @app: App classification to release
 */
static void ai_put_app_classification(struct ai_app_classification *app)
{
	if (!app)
		return;
	
	if (atomic_dec_and_test(&app->refcount)) {
		call_rcu(&app->rcu, (rcu_callback_t)kfree);
	}
}

/**
 * ai_classify_app - Classify an application
 * @task: Task representing the app
 * @package_name: App package name
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_classify_app(struct task_struct *task, const char *package_name)
{
	struct ai_app_classification *app;
	struct ai_app_features features;
	enum ai_app_category category;
	enum ai_app_behavior behavior;
	enum ai_app_performance_req performance_req;
	u32 pattern_confidence, feature_confidence, final_confidence;
	ktime_t start_time, end_time;
	
	if (!task || !package_name || !app_classifier_ctx.classification_active)
		return -EINVAL;
	
	start_time = ktime_get();
	
	/* Get or create app classification */
	app = ai_get_app_classification(package_name);
	if (!app)
		return -ENOMEM;
	
	/* Try pattern matching first */
	pattern_confidence = ai_match_app_pattern(package_name, &category, &behavior, &performance_req);
	
	/* Extract features for machine learning classification */
	ai_extract_app_features(task, &features);
	feature_confidence = ai_classify_app_by_features(&features, &category, &behavior, &performance_req);
	
	/* Use the classification with higher confidence */
	if (pattern_confidence > feature_confidence) {
		final_confidence = pattern_confidence;
	} else {
		final_confidence = feature_confidence;
	}
	
	/* Update classification if confidence is sufficient */
	if (final_confidence > AI_APP_CONFIDENCE_THRESHOLD || app->sample_count == 0) {
		app->category = category;
		app->primary_behavior = behavior;
		app->performance_req = performance_req;
		app->classification_confidence = final_confidence;
		
		/* Update features */
		if (app->sample_count == 0) {
			app->features = features;
			app->features_avg = features;
		} else {
			/* Running average */
			app->features_avg.avg_cpu_usage = (app->features_avg.avg_cpu_usage + features.avg_cpu_usage) / 2;
			app->features_avg.avg_memory_usage = (app->features_avg.avg_memory_usage + features.avg_memory_usage) / 2;
			app->features_avg.user_interaction_freq = (app->features_avg.user_interaction_freq + features.user_interaction_freq) / 2;
			/* ... update other features similarly */
		}
		
		app->sample_count++;
		app->learning_iterations++;
		
		atomic64_inc(&app_classifier_ctx.classifications_updated);
		
		ai_verbose("Classified app %s as %s (confidence: %u%%)",
			   package_name, app_category_names[category], final_confidence);
	}
	
	/* Update usage statistics */
	app->total_launches++;
	app->last_launch_time = ktime_get_ns();
	
	end_time = ktime_get();
	u32 classification_time = ktime_to_us(ktime_sub(end_time, start_time));
	app_classifier_ctx.avg_classification_time = 
		(app_classifier_ctx.avg_classification_time + classification_time) / 2;
	
	ai_put_app_classification(app);
	atomic64_inc(&app_classifier_ctx.apps_classified);
	
	return 0;
}

/**
 * ai_get_app_category - Get app category
 * @package_name: App package name
 * @confidence: Output for classification confidence
 * 
 * Returns: App category
 */
enum ai_app_category ai_get_app_category(const char *package_name, u32 *confidence)
{
	struct ai_app_classification *app;
	enum ai_app_category category = AI_APP_UNKNOWN;
	
	if (!package_name)
		return AI_APP_UNKNOWN;
	
	app = ai_get_app_classification(package_name);
	if (app) {
		category = app->category;
		if (confidence)
			*confidence = app->classification_confidence;
		ai_put_app_classification(app);
	}
	
	return category;
}

/**
 * ai_get_app_performance_req - Get app performance requirement
 * @package_name: App package name
 * 
 * Returns: Performance requirement level
 */
enum ai_app_performance_req ai_get_app_performance_req(const char *package_name)
{
	struct ai_app_classification *app;
	enum ai_app_performance_req req = AI_PERF_REQ_MEDIUM;
	
	if (!package_name)
		return AI_PERF_REQ_MEDIUM;
	
	app = ai_get_app_classification(package_name);
	if (app) {
		req = app->performance_req;
		ai_put_app_classification(app);
	}
	
	return req;
}

/**
 * ai_predict_app_behavior - Predict app behavior
 * @package_name: App package name
 * 
 * Returns: Predicted behavior type
 */
enum ai_app_behavior ai_predict_app_behavior(const char *package_name)
{
	struct ai_app_classification *app;
	enum ai_app_behavior behavior = AI_BEHAVIOR_INTERACTIVE;
	
	if (!package_name)
		return AI_BEHAVIOR_INTERACTIVE;
	
	app = ai_get_app_classification(package_name);
	if (app) {
		behavior = app->primary_behavior;
		
		/* Update prediction statistics */
		app->predicted_category = app->category;
		app->prediction_confidence = app->classification_confidence;
		app->prediction_timestamp = ktime_get_ns();
		
		ai_put_app_classification(app);
		atomic64_inc(&app_classifier_ctx.predictions_made);
	}
	
	return behavior;
}

/**
 * ai_app_classifier_init - Initialize app classification system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_app_classifier_init(void)
{
	int i;
	
	/* Initialize hash table */
	for (i = 0; i < AI_APP_HASH_SIZE; i++)
		INIT_HLIST_HEAD(&app_classifier_ctx.app_hash_table[i]);
	
	spin_lock_init(&app_classifier_ctx.hash_lock);
	spin_lock_init(&app_classifier_ctx.patterns_lock);
	INIT_LIST_HEAD(&app_classifier_ctx.app_patterns);
	
	app_classifier_ctx.classification_active = true;
	app_classifier_ctx.learning_enabled = true;
	app_classifier_ctx.learning_threshold = AI_APP_LEARNING_SAMPLES;
	
	atomic64_set(&app_classifier_ctx.apps_classified, 0);
	atomic64_set(&app_classifier_ctx.classifications_updated, 0);
	atomic64_set(&app_classifier_ctx.predictions_made, 0);
	atomic64_set(&app_classifier_ctx.predictions_correct, 0);
	
	ai_info("App classification system initialized with %zu built-in patterns",
		ARRAY_SIZE(builtin_patterns));
	
	return 0;
}

/**
 * ai_app_classifier_exit - Cleanup app classification system
 */
void ai_app_classifier_exit(void)
{
	struct ai_app_classification *app;
	struct hlist_node *tmp;
	struct ai_app_pattern *pattern, *tmp_pattern;
	int i;
	
	app_classifier_ctx.classification_active = false;
	app_classifier_ctx.learning_enabled = false;
	
	/* Clean up app classifications */
	spin_lock(&app_classifier_ctx.hash_lock);
	for (i = 0; i < AI_APP_HASH_SIZE; i++) {
		hlist_for_each_entry_safe(app, tmp, &app_classifier_ctx.app_hash_table[i], hlist) {
			hlist_del_rcu(&app->hlist);
			ai_put_app_classification(app);
		}
	}
	app_classifier_ctx.total_apps = 0;
	spin_unlock(&app_classifier_ctx.hash_lock);
	
	/* Clean up patterns */
	spin_lock(&app_classifier_ctx.patterns_lock);
	list_for_each_entry_safe(pattern, tmp_pattern, &app_classifier_ctx.app_patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	app_classifier_ctx.pattern_count = 0;
	spin_unlock(&app_classifier_ctx.patterns_lock);
	
	/* Wait for RCU grace period */
	synchronize_rcu();
	
	ai_info("App classification system cleaned up");
}

/**
 * ai_app_classifier_get_statistics - Get classification statistics
 */
void ai_app_classifier_get_statistics(u64 *classified, u64 *updated, u64 *predictions,
				      u32 *total_apps, u32 *avg_time, u32 *accuracy)
{
	if (classified)
		*classified = atomic64_read(&app_classifier_ctx.apps_classified);
	
	if (updated)
		*updated = atomic64_read(&app_classifier_ctx.classifications_updated);
	
	if (predictions)
		*predictions = atomic64_read(&app_classifier_ctx.predictions_made);
	
	if (total_apps)
		*total_apps = app_classifier_ctx.total_apps;
	
	if (avg_time)
		*avg_time = app_classifier_ctx.avg_classification_time;
	
	if (accuracy)
		*accuracy = app_classifier_ctx.classification_accuracy;
}

/* Export symbols */
EXPORT_SYMBOL(ai_classify_app);
EXPORT_SYMBOL(ai_get_app_category);
EXPORT_SYMBOL(ai_get_app_performance_req);
EXPORT_SYMBOL(ai_predict_app_behavior);
EXPORT_SYMBOL(ai_app_classifier_get_statistics);