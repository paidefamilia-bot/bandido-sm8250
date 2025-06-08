/*
 * AI Scheduler Pattern Recognition Engine
 * Advanced pattern recognition and classification system
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/math64.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/sort.h>
#include <linux/string.h>
#include <linux/hash.h>

#include "ai_scheduler.h"

/* Pattern recognition configuration */
#define AI_PR_MAX_PATTERNS		256	/* Maximum number of patterns */
#define AI_PR_PATTERN_SIZE		64	/* Pattern feature size */
#define AI_PR_SIMILARITY_THRESHOLD	800	/* Similarity threshold (scaled by 1000) */
#define AI_PR_MIN_SUPPORT		5	/* Minimum pattern support */
#define AI_PR_MAX_CLUSTERS		32	/* Maximum number of clusters */
#define AI_PR_CONVERGENCE_THRESHOLD	10	/* Convergence threshold */
#define AI_PR_UPDATE_INTERVAL_MS	5000	/* Update interval */

/* Pattern types */
enum ai_pattern_type {
	AI_PATTERN_SEQUENTIAL = 0,	/* Sequential patterns */
	AI_PATTERN_FREQUENT,		/* Frequent itemsets */
	AI_PATTERN_TEMPORAL,		/* Temporal patterns */
	AI_PATTERN_SPATIAL,		/* Spatial patterns */
	AI_PATTERN_BEHAVIORAL,		/* Behavioral patterns */
	AI_PATTERN_ANOMALY,		/* Anomaly patterns */
	AI_PATTERN_ASSOCIATION,		/* Association rules */
	AI_PATTERN_CLUSTERING		/* Clustering patterns */
};

/* Pattern recognition algorithm */
enum ai_pr_algorithm {
	AI_PR_KMEANS = 0,		/* K-means clustering */
	AI_PR_DBSCAN,			/* Density-based clustering */
	AI_PR_HIERARCHICAL,		/* Hierarchical clustering */
	AI_PR_SVM,			/* Support Vector Machine */
	AI_PR_DECISION_TREE,		/* Decision tree */
	AI_PR_RANDOM_FOREST,		/* Random forest */
	AI_PR_NEURAL_NETWORK,		/* Neural network */
	AI_PR_ENSEMBLE			/* Ensemble methods */
};

/* Pattern feature vector */
struct ai_pattern_feature {
	s32 values[AI_PR_PATTERN_SIZE];	/* Feature values */
	u32 weight;			/* Feature weight */
	u64 timestamp;			/* Creation timestamp */
	u32 frequency;			/* Usage frequency */
	u32 confidence;			/* Confidence score */
};

/* Recognized pattern */
struct ai_recognized_pattern {
	u32 pattern_id;			/* Unique pattern ID */
	enum ai_pattern_type type;	/* Pattern type */
	struct ai_pattern_feature centroid; /* Pattern centroid */
	u32 support;			/* Pattern support count */
	u32 confidence;			/* Pattern confidence */
	u32 lift;			/* Pattern lift */
	u64 first_seen;			/* First occurrence */
	u64 last_seen;			/* Last occurrence */
	u32 cluster_id;			/* Cluster assignment */
	
	/* Pattern statistics */
	u32 match_count;		/* Number of matches */
	u32 false_positive_count;	/* False positives */
	u32 precision;			/* Precision score */
	u32 recall;			/* Recall score */
	
	struct list_head list;
};

/* Pattern cluster */
struct ai_pattern_cluster {
	u32 cluster_id;			/* Cluster ID */
	struct ai_pattern_feature centroid; /* Cluster centroid */
	u32 size;			/* Number of patterns in cluster */
	u32 inertia;			/* Within-cluster sum of squares */
	u32 silhouette_score;		/* Silhouette coefficient */
	struct list_head patterns;	/* Patterns in this cluster */
	struct list_head list;
};

/* Pattern matching result */
struct ai_pattern_match {
	u32 pattern_id;			/* Matched pattern ID */
	u32 similarity;			/* Similarity score (0-1000) */
	u32 confidence;			/* Match confidence */
	enum ai_pattern_type type;	/* Pattern type */
	u64 match_timestamp;		/* Match timestamp */
	struct list_head list;
};

/* Pattern recognition context */
struct ai_pattern_recognition_ctx {
	/* Recognized patterns */
	struct list_head patterns;
	spinlock_t patterns_lock;
	u32 pattern_count;
	u32 next_pattern_id;
	
	/* Pattern clusters */
	struct list_head clusters;
	spinlock_t clusters_lock;
	u32 cluster_count;
	u32 optimal_k;			/* Optimal number of clusters */
	
	/* Recent matches */
	struct list_head recent_matches;
	spinlock_t matches_lock;
	u32 match_count;
	
	/* Recognition algorithm */
	enum ai_pr_algorithm algorithm;
	u32 similarity_threshold;
	u32 min_support;
	u32 convergence_threshold;
	
	/* Pattern recognition worker */
	struct delayed_work recognition_work;
	struct workqueue_struct *recognition_wq;
	bool recognition_active;
	u32 update_interval_ms;
	
	/* Statistics */
	atomic64_t patterns_recognized;
	atomic64_t patterns_matched;
	atomic64_t clustering_operations;
	atomic64_t recognition_time_ms;
	
	/* Performance metrics */
	u32 overall_precision;
	u32 overall_recall;
	u32 overall_f1_score;
	u32 clustering_quality;
	
	/* Configuration */
	bool clustering_enabled;
	bool anomaly_detection_enabled;
	bool temporal_analysis_enabled;
	bool adaptive_threshold_enabled;
};

static struct ai_pattern_recognition_ctx pr_ctx;

/**
 * ai_calculate_feature_distance - Calculate distance between two feature vectors
 * @f1: First feature vector
 * @f2: Second feature vector
 * 
 * Returns: Euclidean distance (scaled)
 */
static u32 ai_calculate_feature_distance(const struct ai_pattern_feature *f1,
					 const struct ai_pattern_feature *f2)
{
	u64 sum_squared_diff = 0;
	u32 i;
	
	for (i = 0; i < AI_PR_PATTERN_SIZE; i++) {
		s32 diff = f1->values[i] - f2->values[i];
		sum_squared_diff += diff * diff;
	}
	
	return int_sqrt(sum_squared_diff);
}

/**
 * ai_calculate_feature_similarity - Calculate similarity between two feature vectors
 * @f1: First feature vector
 * @f2: Second feature vector
 * 
 * Returns: Similarity score (0-1000)
 */
static u32 ai_calculate_feature_similarity(const struct ai_pattern_feature *f1,
					   const struct ai_pattern_feature *f2)
{
	u32 distance = ai_calculate_feature_distance(f1, f2);
	u32 max_distance = AI_PR_PATTERN_SIZE * AI_FIXED_POINT_SCALE;
	
	if (distance >= max_distance)
		return 0;
	
	return 1000 - (distance * 1000) / max_distance;
}

/**
 * ai_update_pattern_centroid - Update pattern centroid
 * @pattern: Pattern to update
 * @new_feature: New feature to incorporate
 */
static void ai_update_pattern_centroid(struct ai_recognized_pattern *pattern,
				       const struct ai_pattern_feature *new_feature)
{
	u32 i;
	u32 weight = pattern->support + 1;
	
	/* Update centroid using weighted average */
	for (i = 0; i < AI_PR_PATTERN_SIZE; i++) {
		s64 weighted_sum = (s64)pattern->centroid.values[i] * pattern->support +
				   (s64)new_feature->values[i];
		pattern->centroid.values[i] = (s32)(weighted_sum / weight);
	}
	
	pattern->centroid.timestamp = ktime_get_ns();
	pattern->support++;
	pattern->last_seen = pattern->centroid.timestamp;
}

/**
 * ai_create_new_pattern - Create a new recognized pattern
 * @feature: Feature vector for the new pattern
 * @type: Pattern type
 * 
 * Returns: New pattern or NULL on failure
 */
static struct ai_recognized_pattern *ai_create_new_pattern(const struct ai_pattern_feature *feature,
							   enum ai_pattern_type type)
{
	struct ai_recognized_pattern *pattern;
	
	pattern = kzalloc(sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		return NULL;
	
	pattern->pattern_id = pr_ctx.next_pattern_id++;
	pattern->type = type;
	pattern->centroid = *feature;
	pattern->support = 1;
	pattern->confidence = 500; /* Initial confidence */
	pattern->first_seen = ktime_get_ns();
	pattern->last_seen = pattern->first_seen;
	pattern->cluster_id = UINT_MAX; /* Unassigned */
	
	INIT_LIST_HEAD(&pattern->list);
	
	return pattern;
}

/**
 * ai_find_matching_pattern - Find the best matching pattern
 * @feature: Feature vector to match
 * @type: Pattern type to match
 * @similarity: Output similarity score
 * 
 * Returns: Matching pattern or NULL if no match
 */
static struct ai_recognized_pattern *ai_find_matching_pattern(const struct ai_pattern_feature *feature,
							      enum ai_pattern_type type,
							      u32 *similarity)
{
	struct ai_recognized_pattern *pattern, *best_match = NULL;
	u32 best_similarity = 0;
	
	list_for_each_entry(pattern, &pr_ctx.patterns, list) {
		if (pattern->type != type)
			continue;
		
		u32 sim = ai_calculate_feature_similarity(&pattern->centroid, feature);
		if (sim > best_similarity && sim >= pr_ctx.similarity_threshold) {
			best_similarity = sim;
			best_match = pattern;
		}
	}
	
	if (similarity)
		*similarity = best_similarity;
	
	return best_match;
}

/**
 * ai_kmeans_clustering - Perform K-means clustering on patterns
 * @k: Number of clusters
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_kmeans_clustering(u32 k)
{
	struct ai_pattern_cluster *clusters[AI_PR_MAX_CLUSTERS];
	struct ai_recognized_pattern *pattern;
	bool converged = false;
	u32 iteration = 0;
	u32 i, j;
	int ret = 0;
	
	if (k > AI_PR_MAX_CLUSTERS || k == 0)
		return -EINVAL;
	
	/* Initialize clusters */
	for (i = 0; i < k; i++) {
		clusters[i] = kzalloc(sizeof(*clusters[i]), GFP_KERNEL);
		if (!clusters[i]) {
			ret = -ENOMEM;
			goto cleanup;
		}
		
		clusters[i]->cluster_id = i;
		INIT_LIST_HEAD(&clusters[i]->patterns);
		INIT_LIST_HEAD(&clusters[i]->list);
	}
	
	/* Initialize centroids randomly */
	i = 0;
	list_for_each_entry(pattern, &pr_ctx.patterns, list) {
		if (i < k) {
			clusters[i]->centroid = pattern->centroid;
			i++;
		}
	}
	
	/* K-means iterations */
	while (!converged && iteration < 100) {
		bool any_change = false;
		
		/* Clear cluster assignments */
		for (i = 0; i < k; i++) {
			INIT_LIST_HEAD(&clusters[i]->patterns);
			clusters[i]->size = 0;
		}
		
		/* Assign patterns to nearest clusters */
		list_for_each_entry(pattern, &pr_ctx.patterns, list) {
			u32 min_distance = UINT_MAX;
			u32 best_cluster = 0;
			
			for (i = 0; i < k; i++) {
				u32 distance = ai_calculate_feature_distance(&pattern->centroid,
									     &clusters[i]->centroid);
				if (distance < min_distance) {
					min_distance = distance;
					best_cluster = i;
				}
			}
			
			if (pattern->cluster_id != best_cluster) {
				pattern->cluster_id = best_cluster;
				any_change = true;
			}
			
			clusters[best_cluster]->size++;
		}
		
		/* Update centroids */
		for (i = 0; i < k; i++) {
			if (clusters[i]->size == 0)
				continue;
			
			/* Reset centroid */
			memset(&clusters[i]->centroid, 0, sizeof(clusters[i]->centroid));
			
			/* Sum all patterns in cluster */
			list_for_each_entry(pattern, &pr_ctx.patterns, list) {
				if (pattern->cluster_id == i) {
					for (j = 0; j < AI_PR_PATTERN_SIZE; j++) {
						clusters[i]->centroid.values[j] += pattern->centroid.values[j];
					}
				}
			}
			
			/* Average to get centroid */
			for (j = 0; j < AI_PR_PATTERN_SIZE; j++) {
				clusters[i]->centroid.values[j] /= clusters[i]->size;
			}
		}
		
		converged = !any_change;
		iteration++;
	}
	
	/* Add clusters to context */
	spin_lock(&pr_ctx.clusters_lock);
	for (i = 0; i < k; i++) {
		list_add(&clusters[i]->list, &pr_ctx.clusters);
		pr_ctx.cluster_count++;
	}
	spin_unlock(&pr_ctx.clusters_lock);
	
	atomic64_inc(&pr_ctx.clustering_operations);
	
	ai_info("K-means clustering completed: %u clusters, %u iterations", k, iteration);
	
	return 0;

cleanup:
	for (i = 0; i < k; i++) {
		kfree(clusters[i]);
	}
	return ret;
}

/**
 * ai_recognize_pattern - Recognize a pattern from feature vector
 * @feature: Feature vector to recognize
 * @type: Pattern type
 * 
 * Returns: Pattern ID if recognized, 0 if new pattern created, negative on error
 */
int ai_recognize_pattern(const struct ai_pattern_feature *feature, enum ai_pattern_type type)
{
	struct ai_recognized_pattern *pattern;
	struct ai_pattern_match *match;
	u32 similarity;
	
	if (!feature || type >= AI_PATTERN_CLUSTERING + 1)
		return -EINVAL;
	
	spin_lock(&pr_ctx.patterns_lock);
	
	/* Try to find matching pattern */
	pattern = ai_find_matching_pattern(feature, type, &similarity);
	
	if (pattern) {
		/* Update existing pattern */
		ai_update_pattern_centroid(pattern, feature);
		pattern->match_count++;
		
		/* Update confidence based on match frequency */
		pattern->confidence = min(pattern->confidence + 10, 1000U);
		
		spin_unlock(&pr_ctx.patterns_lock);
		
		/* Record the match */
		match = kzalloc(sizeof(*match), GFP_ATOMIC);
		if (match) {
			match->pattern_id = pattern->pattern_id;
			match->similarity = similarity;
			match->confidence = pattern->confidence;
			match->type = type;
			match->match_timestamp = ktime_get_ns();
			
			spin_lock(&pr_ctx.matches_lock);
			list_add(&match->list, &pr_ctx.recent_matches);
			pr_ctx.match_count++;
			
			/* Keep only recent matches */
			if (pr_ctx.match_count > 100) {
				struct ai_pattern_match *old_match = 
					list_last_entry(&pr_ctx.recent_matches,
							struct ai_pattern_match, list);
				list_del(&old_match->list);
				kfree(old_match);
				pr_ctx.match_count--;
			}
			spin_unlock(&pr_ctx.matches_lock);
		}
		
		atomic64_inc(&pr_ctx.patterns_matched);
		
		ai_verbose("Pattern recognized: ID=%u, similarity=%u, confidence=%u",
			   pattern->pattern_id, similarity, pattern->confidence);
		
		return pattern->pattern_id;
	} else {
		/* Create new pattern */
		pattern = ai_create_new_pattern(feature, type);
		if (!pattern) {
			spin_unlock(&pr_ctx.patterns_lock);
			return -ENOMEM;
		}
		
		list_add(&pattern->list, &pr_ctx.patterns);
		pr_ctx.pattern_count++;
		
		spin_unlock(&pr_ctx.patterns_lock);
		
		atomic64_inc(&pr_ctx.patterns_recognized);
		
		ai_info("New pattern created: ID=%u, type=%d", pattern->pattern_id, type);
		
		return 0; /* New pattern */
	}
}

/**
 * ai_pattern_recognition_worker - Background pattern recognition worker
 * @work: Work structure
 */
static void ai_pattern_recognition_worker(struct work_struct *work)
{
	ktime_t start_time, end_time;
	u32 patterns_processed = 0;
	
	if (!pr_ctx.recognition_active)
		return;
	
	start_time = ktime_get();
	
	/* Perform clustering if enabled and enough patterns */
	if (pr_ctx.clustering_enabled && pr_ctx.pattern_count >= 10) {
		/* Determine optimal number of clusters using elbow method (simplified) */
		u32 optimal_k = min(pr_ctx.pattern_count / 5, 8U);
		if (optimal_k != pr_ctx.optimal_k) {
			/* Clear existing clusters */
			struct ai_pattern_cluster *cluster, *tmp;
			spin_lock(&pr_ctx.clusters_lock);
			list_for_each_entry_safe(cluster, tmp, &pr_ctx.clusters, list) {
				list_del(&cluster->list);
				kfree(cluster);
			}
			pr_ctx.cluster_count = 0;
			spin_unlock(&pr_ctx.clusters_lock);
			
			/* Perform new clustering */
			if (ai_kmeans_clustering(optimal_k) == 0) {
				pr_ctx.optimal_k = optimal_k;
			}
		}
	}
	
	/* Update pattern statistics */
	spin_lock(&pr_ctx.patterns_lock);
	struct ai_recognized_pattern *pattern;
	list_for_each_entry(pattern, &pr_ctx.patterns, list) {
		/* Update precision and recall (simplified) */
		if (pattern->match_count > 0) {
			pattern->precision = (pattern->match_count * 1000) / 
					     (pattern->match_count + pattern->false_positive_count);
			pattern->recall = min(pattern->match_count * 10, 1000U); /* Simplified */
		}
		patterns_processed++;
	}
	spin_unlock(&pr_ctx.patterns_lock);
	
	/* Calculate overall performance metrics */
	if (pr_ctx.pattern_count > 0) {
		u64 total_precision = 0, total_recall = 0;
		
		spin_lock(&pr_ctx.patterns_lock);
		list_for_each_entry(pattern, &pr_ctx.patterns, list) {
			total_precision += pattern->precision;
			total_recall += pattern->recall;
		}
		spin_unlock(&pr_ctx.patterns_lock);
		
		pr_ctx.overall_precision = total_precision / pr_ctx.pattern_count;
		pr_ctx.overall_recall = total_recall / pr_ctx.pattern_count;
		pr_ctx.overall_f1_score = (2 * pr_ctx.overall_precision * pr_ctx.overall_recall) /
					  (pr_ctx.overall_precision + pr_ctx.overall_recall + 1);
	}
	
	end_time = ktime_get();
	u64 processing_time = ktime_to_ms(ktime_sub(end_time, start_time));
	atomic64_add(processing_time, &pr_ctx.recognition_time_ms);
	
	ai_verbose("Pattern recognition update: processed %u patterns in %llu ms",
		   patterns_processed, processing_time);
	
	/* Schedule next update */
	if (pr_ctx.recognition_active) {
		queue_delayed_work(pr_ctx.recognition_wq, &pr_ctx.recognition_work,
				   msecs_to_jiffies(pr_ctx.update_interval_ms));
	}
}

/**
 * ai_get_pattern_info - Get information about a recognized pattern
 * @pattern_id: Pattern ID
 * @info: Output pattern information
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_get_pattern_info(u32 pattern_id, struct ai_recognized_pattern *info)
{
	struct ai_recognized_pattern *pattern;
	
	if (!info)
		return -EINVAL;
	
	spin_lock(&pr_ctx.patterns_lock);
	list_for_each_entry(pattern, &pr_ctx.patterns, list) {
		if (pattern->pattern_id == pattern_id) {
			*info = *pattern;
			spin_unlock(&pr_ctx.patterns_lock);
			return 0;
		}
	}
	spin_unlock(&pr_ctx.patterns_lock);
	
	return -ENOENT;
}

/**
 * ai_pattern_recognition_init - Initialize pattern recognition engine
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_pattern_recognition_init(void)
{
	/* Initialize context */
	memset(&pr_ctx, 0, sizeof(pr_ctx));
	
	INIT_LIST_HEAD(&pr_ctx.patterns);
	INIT_LIST_HEAD(&pr_ctx.clusters);
	INIT_LIST_HEAD(&pr_ctx.recent_matches);
	
	spin_lock_init(&pr_ctx.patterns_lock);
	spin_lock_init(&pr_ctx.clusters_lock);
	spin_lock_init(&pr_ctx.matches_lock);
	
	pr_ctx.algorithm = AI_PR_KMEANS;
	pr_ctx.similarity_threshold = AI_PR_SIMILARITY_THRESHOLD;
	pr_ctx.min_support = AI_PR_MIN_SUPPORT;
	pr_ctx.convergence_threshold = AI_PR_CONVERGENCE_THRESHOLD;
	pr_ctx.update_interval_ms = AI_PR_UPDATE_INTERVAL_MS;
	pr_ctx.next_pattern_id = 1;
	
	/* Create recognition work queue */
	pr_ctx.recognition_wq = create_singlethread_workqueue("ai_pattern_recognition");
	if (!pr_ctx.recognition_wq) {
		ai_error("Failed to create pattern recognition work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&pr_ctx.recognition_work, ai_pattern_recognition_worker);
	
	/* Configuration */
	pr_ctx.recognition_active = true;
	pr_ctx.clustering_enabled = true;
	pr_ctx.anomaly_detection_enabled = true;
	pr_ctx.temporal_analysis_enabled = true;
	pr_ctx.adaptive_threshold_enabled = true;
	
	/* Initialize statistics */
	atomic64_set(&pr_ctx.patterns_recognized, 0);
	atomic64_set(&pr_ctx.patterns_matched, 0);
	atomic64_set(&pr_ctx.clustering_operations, 0);
	atomic64_set(&pr_ctx.recognition_time_ms, 0);
	
	/* Start pattern recognition */
	queue_delayed_work(pr_ctx.recognition_wq, &pr_ctx.recognition_work,
			   msecs_to_jiffies(pr_ctx.update_interval_ms));
	
	ai_info("Pattern recognition engine initialized with algorithm: %d", pr_ctx.algorithm);
	
	return 0;
}

/**
 * ai_pattern_recognition_exit - Cleanup pattern recognition engine
 */
void ai_pattern_recognition_exit(void)
{
	struct ai_recognized_pattern *pattern, *tmp_pattern;
	struct ai_pattern_cluster *cluster, *tmp_cluster;
	struct ai_pattern_match *match, *tmp_match;
	
	pr_ctx.recognition_active = false;
	
	/* Stop work queue */
	if (pr_ctx.recognition_wq) {
		cancel_delayed_work_sync(&pr_ctx.recognition_work);
		destroy_workqueue(pr_ctx.recognition_wq);
		pr_ctx.recognition_wq = NULL;
	}
	
	/* Free patterns */
	spin_lock(&pr_ctx.patterns_lock);
	list_for_each_entry_safe(pattern, tmp_pattern, &pr_ctx.patterns, list) {
		list_del(&pattern->list);
		kfree(pattern);
	}
	spin_unlock(&pr_ctx.patterns_lock);
	
	/* Free clusters */
	spin_lock(&pr_ctx.clusters_lock);
	list_for_each_entry_safe(cluster, tmp_cluster, &pr_ctx.clusters, list) {
		list_del(&cluster->list);
		kfree(cluster);
	}
	spin_unlock(&pr_ctx.clusters_lock);
	
	/* Free matches */
	spin_lock(&pr_ctx.matches_lock);
	list_for_each_entry_safe(match, tmp_match, &pr_ctx.recent_matches, list) {
		list_del(&match->list);
		kfree(match);
	}
	spin_unlock(&pr_ctx.matches_lock);
	
	ai_info("Pattern recognition engine cleaned up");
}

/**
 * ai_pattern_recognition_get_statistics - Get pattern recognition statistics
 */
void ai_pattern_recognition_get_statistics(u64 *recognized, u64 *matched, u64 *clustering_ops,
					   u32 *pattern_count, u32 *cluster_count,
					   u32 *precision, u32 *recall, u32 *f1_score)
{
	if (recognized)
		*recognized = atomic64_read(&pr_ctx.patterns_recognized);
	
	if (matched)
		*matched = atomic64_read(&pr_ctx.patterns_matched);
	
	if (clustering_ops)
		*clustering_ops = atomic64_read(&pr_ctx.clustering_operations);
	
	if (pattern_count)
		*pattern_count = pr_ctx.pattern_count;
	
	if (cluster_count)
		*cluster_count = pr_ctx.cluster_count;
	
	if (precision)
		*precision = pr_ctx.overall_precision;
	
	if (recall)
		*recall = pr_ctx.overall_recall;
	
	if (f1_score)
		*f1_score = pr_ctx.overall_f1_score;
}

/* Export symbols */
EXPORT_SYMBOL(ai_recognize_pattern);
EXPORT_SYMBOL(ai_get_pattern_info);
EXPORT_SYMBOL(ai_pattern_recognition_get_statistics);