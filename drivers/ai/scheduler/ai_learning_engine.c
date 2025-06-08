/*
 * AI Scheduler Online Learning Algorithms
 * Advanced online learning and adaptive optimization
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
#include <linux/random.h>
#include <linux/sort.h>

#include "ai_scheduler.h"

/* Online learning configuration */
#define AI_OL_MAX_SAMPLES		1000	/* Maximum online samples */
#define AI_OL_WINDOW_SIZE		100	/* Learning window size */
#define AI_OL_ADAPTATION_RATE		50	/* Adaptation rate (scaled by 1000) */
#define AI_OL_FORGETTING_FACTOR		950	/* Forgetting factor (scaled by 1000) */
#define AI_OL_EXPLORATION_RATE		100	/* Exploration rate (scaled by 1000) */
#define AI_OL_MIN_CONFIDENCE		600	/* Minimum confidence threshold */
#define AI_OL_UPDATE_INTERVAL_MS	2000	/* Update interval */

/* Learning algorithm types */
enum ai_learning_algorithm {
	AI_LEARNING_SGD = 0,		/* Stochastic Gradient Descent */
	AI_LEARNING_ADAM,		/* Adaptive Moment Estimation */
	AI_LEARNING_RMSPROP,		/* Root Mean Square Propagation */
	AI_LEARNING_ADAGRAD,		/* Adaptive Gradient Algorithm */
	AI_LEARNING_MOMENTUM,		/* Momentum-based SGD */
	AI_LEARNING_NESTEROV,		/* Nesterov Accelerated Gradient */
	AI_LEARNING_ADADELTA,		/* Adaptive Delta */
	AI_LEARNING_NADAM		/* Nesterov-accelerated Adam */
};

/* Optimization state for Adam */
struct ai_adam_state {
	s32 *m_weights;			/* First moment weights */
	s32 *v_weights;			/* Second moment weights */
	s32 *m_biases;			/* First moment biases */
	s32 *v_biases;			/* Second moment biases */
	u32 beta1;			/* First moment decay rate */
	u32 beta2;			/* Second moment decay rate */
	u32 epsilon;			/* Small constant for numerical stability */
	u32 t;				/* Time step */
};

/* Optimization state for RMSprop */
struct ai_rmsprop_state {
	s32 *v_weights;			/* Moving average of squared gradients */
	s32 *v_biases;			/* Moving average of squared gradients */
	u32 decay_rate;			/* Decay rate */
	u32 epsilon;			/* Small constant */
};

/* Online learning sample */
struct ai_online_sample {
	s32 features[AI_NN_INPUT_SIZE];	/* Input features */
	s32 target[AI_NN_OUTPUT_SIZE];	/* Target output */
	s32 prediction[AI_NN_OUTPUT_SIZE]; /* Model prediction */
	u32 confidence;			/* Prediction confidence */
	u32 error;			/* Prediction error */
	u64 timestamp;			/* Sample timestamp */
	u32 weight;			/* Sample importance weight */
	bool is_correct;		/* Prediction correctness */
	struct list_head list;
};

/* Learning performance metrics */
struct ai_learning_metrics {
	u64 total_samples;
	u64 correct_predictions;
	u64 incorrect_predictions;
	u32 accuracy;			/* Current accuracy (%) */
	u32 precision;			/* Precision (%) */
	u32 recall;			/* Recall (%) */
	u32 f1_score;			/* F1 score (%) */
	u32 loss;			/* Current loss */
	u32 learning_rate;		/* Current learning rate */
	u32 convergence_rate;		/* Convergence rate */
	u64 training_time_ms;		/* Total training time */
	u32 adaptation_speed;		/* How fast the model adapts */
};

/* Adaptive learning rate scheduler */
struct ai_lr_scheduler {
	enum {
		AI_LR_CONSTANT,
		AI_LR_EXPONENTIAL_DECAY,
		AI_LR_STEP_DECAY,
		AI_LR_COSINE_ANNEALING,
		AI_LR_ADAPTIVE
	} type;
	
	u32 initial_lr;			/* Initial learning rate */
	u32 current_lr;			/* Current learning rate */
	u32 min_lr;			/* Minimum learning rate */
	u32 max_lr;			/* Maximum learning rate */
	u32 decay_rate;			/* Decay rate */
	u32 step_size;			/* Step size for step decay */
	u32 patience;			/* Patience for adaptive */
	u32 epochs_without_improvement;
	u32 best_loss;			/* Best loss seen so far */
};

/* Online learning engine context */
struct ai_learning_engine_ctx {
	/* Learning algorithm */
	enum ai_learning_algorithm algorithm;
	struct ai_adam_state adam_state;
	struct ai_rmsprop_state rmsprop_state;
	
	/* Online samples */
	struct list_head online_samples;
	spinlock_t samples_lock;
	u32 sample_count;
	u32 window_size;
	
	/* Learning metrics */
	struct ai_learning_metrics metrics;
	spinlock_t metrics_lock;
	
	/* Learning rate scheduler */
	struct ai_lr_scheduler lr_scheduler;
	
	/* Online learning worker */
	struct delayed_work learning_work;
	struct workqueue_struct *learning_wq;
	bool learning_active;
	u32 update_interval_ms;
	
	/* Adaptation parameters */
	u32 adaptation_rate;
	u32 forgetting_factor;
	u32 exploration_rate;
	u32 min_confidence;
	
	/* Experience replay buffer */
	struct ai_online_sample *replay_buffer[AI_OL_MAX_SAMPLES];
	u32 replay_buffer_size;
	u32 replay_buffer_index;
	spinlock_t replay_lock;
	
	/* Statistics */
	atomic64_t learning_updates;
	atomic64_t adaptation_events;
	atomic64_t exploration_events;
	
	/* Configuration */
	bool adaptive_lr_enabled;
	bool experience_replay_enabled;
	bool exploration_enabled;
	bool forgetting_enabled;
};

static struct ai_learning_engine_ctx learning_ctx;

/**
 * ai_init_adam_state - Initialize Adam optimizer state
 * @state: Adam state to initialize
 * @num_weights: Number of weights
 * @num_biases: Number of biases
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_init_adam_state(struct ai_adam_state *state, u32 num_weights, u32 num_biases)
{
	state->m_weights = kzalloc(num_weights * sizeof(s32), GFP_KERNEL);
	state->v_weights = kzalloc(num_weights * sizeof(s32), GFP_KERNEL);
	state->m_biases = kzalloc(num_biases * sizeof(s32), GFP_KERNEL);
	state->v_biases = kzalloc(num_biases * sizeof(s32), GFP_KERNEL);
	
	if (!state->m_weights || !state->v_weights || !state->m_biases || !state->v_biases) {
		kfree(state->m_weights);
		kfree(state->v_weights);
		kfree(state->m_biases);
		kfree(state->v_biases);
		return -ENOMEM;
	}
	
	state->beta1 = 900;		/* 0.9 */
	state->beta2 = 999;		/* 0.999 */
	state->epsilon = 1;		/* 1e-8 scaled */
	state->t = 0;
	
	return 0;
}

/**
 * ai_free_adam_state - Free Adam optimizer state
 * @state: Adam state to free
 */
static void ai_free_adam_state(struct ai_adam_state *state)
{
	kfree(state->m_weights);
	kfree(state->v_weights);
	kfree(state->m_biases);
	kfree(state->v_biases);
	memset(state, 0, sizeof(*state));
}

/**
 * ai_adam_update - Update weights using Adam optimizer
 * @layer: Neural network layer
 * @state: Adam optimizer state
 * @learning_rate: Learning rate
 */
static void ai_adam_update(struct ai_nn_layer *layer, struct ai_adam_state *state, u32 learning_rate)
{
	u32 i, j, idx;
	s32 m_hat, v_hat, update;
	u32 beta1_t, beta2_t;
	
	state->t++;
	
	/* Compute bias correction terms */
	beta1_t = 1000 - int_pow(state->beta1, state->t) / int_pow(1000, state->t - 1);
	beta2_t = 1000 - int_pow(state->beta2, state->t) / int_pow(1000, state->t - 1);
	
	/* Update weights */
	for (i = 0; i < layer->input_size; i++) {
		for (j = 0; j < layer->output_size; j++) {
			idx = i * layer->output_size + j;
			
			/* Update biased first moment estimate */
			state->m_weights[idx] = (state->beta1 * state->m_weights[idx] + 
						(1000 - state->beta1) * layer->weight_gradients[idx]) / 1000;
			
			/* Update biased second moment estimate */
			s32 grad_squared = ai_fixed_multiply(layer->weight_gradients[idx], 
							     layer->weight_gradients[idx]);
			state->v_weights[idx] = (state->beta2 * state->v_weights[idx] + 
						(1000 - state->beta2) * grad_squared) / 1000;
			
			/* Compute bias-corrected first moment estimate */
			m_hat = (state->m_weights[idx] * 1000) / beta1_t;
			
			/* Compute bias-corrected second moment estimate */
			v_hat = (state->v_weights[idx] * 1000) / beta2_t;
			
			/* Compute update */
			update = ai_fixed_divide(learning_rate * m_hat, int_sqrt(v_hat) + state->epsilon);
			
			/* Apply update */
			layer->weights[idx] -= update;
		}
	}
	
	/* Update biases */
	for (j = 0; j < layer->output_size; j++) {
		/* Update biased first moment estimate */
		state->m_biases[j] = (state->beta1 * state->m_biases[j] + 
				     (1000 - state->beta1) * layer->bias_gradients[j]) / 1000;
		
		/* Update biased second moment estimate */
		s32 grad_squared = ai_fixed_multiply(layer->bias_gradients[j], 
						     layer->bias_gradients[j]);
		state->v_biases[j] = (state->beta2 * state->v_biases[j] + 
				     (1000 - state->beta2) * grad_squared) / 1000;
		
		/* Compute bias-corrected estimates */
		m_hat = (state->m_biases[j] * 1000) / beta1_t;
		v_hat = (state->v_biases[j] * 1000) / beta2_t;
		
		/* Compute and apply update */
		update = ai_fixed_divide(learning_rate * m_hat, int_sqrt(v_hat) + state->epsilon);
		layer->biases[j] -= update;
	}
}

/**
 * ai_calculate_prediction_error - Calculate prediction error
 * @prediction: Model prediction
 * @target: Target output
 * @size: Output size
 * 
 * Returns: Mean squared error (scaled)
 */
static u32 ai_calculate_prediction_error(const s32 *prediction, const s32 *target, u32 size)
{
	u64 total_error = 0;
	u32 i;
	
	for (i = 0; i < size; i++) {
		s32 diff = prediction[i] - target[i];
		total_error += diff * diff;
	}
	
	return (u32)(total_error / size);
}

/**
 * ai_calculate_prediction_confidence - Calculate prediction confidence
 * @prediction: Model prediction
 * @size: Output size
 * 
 * Returns: Confidence score (0-1000)
 */
static u32 ai_calculate_prediction_confidence(const s32 *prediction, u32 size)
{
	s32 max_val = prediction[0];
	s32 second_max = prediction[0];
	u32 i;
	
	/* Find max and second max values */
	for (i = 1; i < size; i++) {
		if (prediction[i] > max_val) {
			second_max = max_val;
			max_val = prediction[i];
		} else if (prediction[i] > second_max) {
			second_max = prediction[i];
		}
	}
	
	/* Confidence is the difference between max and second max */
	if (max_val > second_max) {
		return min((u32)((max_val - second_max) * 1000 / AI_FIXED_POINT_SCALE), 1000U);
	}
	
	return 0;
}

/**
 * ai_update_learning_rate - Update learning rate based on performance
 * @scheduler: Learning rate scheduler
 * @current_loss: Current loss value
 */
static void ai_update_learning_rate(struct ai_lr_scheduler *scheduler, u32 current_loss)
{
	switch (scheduler->type) {
	case AI_LR_EXPONENTIAL_DECAY:
		scheduler->current_lr = (scheduler->current_lr * scheduler->decay_rate) / 1000;
		scheduler->current_lr = max(scheduler->current_lr, scheduler->min_lr);
		break;
		
	case AI_LR_STEP_DECAY:
		if (learning_ctx.metrics.total_samples % scheduler->step_size == 0) {
			scheduler->current_lr = (scheduler->current_lr * scheduler->decay_rate) / 1000;
			scheduler->current_lr = max(scheduler->current_lr, scheduler->min_lr);
		}
		break;
		
	case AI_LR_ADAPTIVE:
		if (current_loss < scheduler->best_loss) {
			scheduler->best_loss = current_loss;
			scheduler->epochs_without_improvement = 0;
		} else {
			scheduler->epochs_without_improvement++;
			
			if (scheduler->epochs_without_improvement >= scheduler->patience) {
				scheduler->current_lr = (scheduler->current_lr * scheduler->decay_rate) / 1000;
				scheduler->current_lr = max(scheduler->current_lr, scheduler->min_lr);
				scheduler->epochs_without_improvement = 0;
			}
		}
		break;
		
	case AI_LR_COSINE_ANNEALING:
		/* Simplified cosine annealing */
		scheduler->current_lr = scheduler->min_lr + 
			(scheduler->max_lr - scheduler->min_lr) / 2 * 
			(1 + int_sqrt(learning_ctx.metrics.total_samples % 1000) / 32);
		break;
		
	default:
		/* Constant learning rate */
		break;
	}
}

/**
 * ai_add_online_sample - Add a new online learning sample
 * @features: Input features
 * @target: Target output
 * @prediction: Model prediction
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_add_online_sample(const s32 *features, const s32 *target, const s32 *prediction)
{
	struct ai_online_sample *sample;
	u32 error, confidence;
	bool is_correct;
	
	if (!features || !target || !prediction)
		return -EINVAL;
	
	sample = kzalloc(sizeof(*sample), GFP_ATOMIC);
	if (!sample)
		return -ENOMEM;
	
	/* Copy data */
	memcpy(sample->features, features, AI_NN_INPUT_SIZE * sizeof(s32));
	memcpy(sample->target, target, AI_NN_OUTPUT_SIZE * sizeof(s32));
	memcpy(sample->prediction, prediction, AI_NN_OUTPUT_SIZE * sizeof(s32));
	
	/* Calculate metrics */
	error = ai_calculate_prediction_error(prediction, target, AI_NN_OUTPUT_SIZE);
	confidence = ai_calculate_prediction_confidence(prediction, AI_NN_OUTPUT_SIZE);
	is_correct = (error < (AI_FIXED_POINT_SCALE / 10)); /* 10% error threshold */
	
	sample->error = error;
	sample->confidence = confidence;
	sample->is_correct = is_correct;
	sample->timestamp = ktime_get_ns();
	sample->weight = 1000; /* Default weight */
	
	/* Add to online samples list */
	spin_lock(&learning_ctx.samples_lock);
	
	/* Remove oldest sample if window is full */
	if (learning_ctx.sample_count >= learning_ctx.window_size) {
		struct ai_online_sample *oldest = list_first_entry(&learning_ctx.online_samples,
								   struct ai_online_sample, list);
		list_del(&oldest->list);
		kfree(oldest);
		learning_ctx.sample_count--;
	}
	
	list_add_tail(&sample->list, &learning_ctx.online_samples);
	learning_ctx.sample_count++;
	
	spin_unlock(&learning_ctx.samples_lock);
	
	/* Update metrics */
	spin_lock(&learning_ctx.metrics_lock);
	learning_ctx.metrics.total_samples++;
	if (is_correct) {
		learning_ctx.metrics.correct_predictions++;
	} else {
		learning_ctx.metrics.incorrect_predictions++;
	}
	
	/* Update accuracy */
	if (learning_ctx.metrics.total_samples > 0) {
		learning_ctx.metrics.accuracy = 
			(learning_ctx.metrics.correct_predictions * 100) / 
			learning_ctx.metrics.total_samples;
	}
	
	spin_unlock(&learning_ctx.metrics_lock);
	
	ai_verbose("Added online sample: error=%u, confidence=%u, correct=%d",
		   error, confidence, is_correct);
	
	return 0;
}

/**
 * ai_online_learning_worker - Background online learning worker
 * @work: Work structure
 */
static void ai_online_learning_worker(struct work_struct *work)
{
	struct ai_online_sample *sample;
	u32 samples_processed = 0;
	ktime_t start_time, end_time;
	
	if (!learning_ctx.learning_active)
		return;
	
	start_time = ktime_get();
	
	/* Process online samples */
	spin_lock(&learning_ctx.samples_lock);
	list_for_each_entry(sample, &learning_ctx.online_samples, list) {
		if (samples_processed >= 10) /* Limit processing per cycle */
			break;
		
		/* Skip samples with low confidence if exploration is disabled */
		if (!learning_ctx.exploration_enabled && 
		    sample->confidence < learning_ctx.min_confidence)
			continue;
		
		/* Perform online learning update */
		/* This would integrate with the neural network training */
		
		samples_processed++;
	}
	spin_unlock(&learning_ctx.samples_lock);
	
	/* Update learning rate */
	if (learning_ctx.adaptive_lr_enabled) {
		ai_update_learning_rate(&learning_ctx.lr_scheduler, learning_ctx.metrics.loss);
	}
	
	end_time = ktime_get();
	u64 processing_time = ktime_to_ms(ktime_sub(end_time, start_time));
	
	atomic64_add(processing_time, &learning_ctx.metrics.training_time_ms);
	atomic64_inc(&learning_ctx.learning_updates);
	
	ai_verbose("Online learning update: processed %u samples in %llu ms",
		   samples_processed, processing_time);
	
	/* Schedule next update */
	if (learning_ctx.learning_active) {
		queue_delayed_work(learning_ctx.learning_wq, &learning_ctx.learning_work,
				   msecs_to_jiffies(learning_ctx.update_interval_ms));
	}
}

/**
 * ai_get_learning_metrics - Get current learning metrics
 * @metrics: Output metrics structure
 */
void ai_get_learning_metrics(struct ai_learning_metrics *metrics)
{
	if (!metrics)
		return;
	
	spin_lock(&learning_ctx.metrics_lock);
	*metrics = learning_ctx.metrics;
	spin_unlock(&learning_ctx.metrics_lock);
}

/**
 * ai_set_learning_algorithm - Set the learning algorithm
 * @algorithm: Learning algorithm to use
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_set_learning_algorithm(enum ai_learning_algorithm algorithm)
{
	if (algorithm >= AI_LEARNING_NADAM + 1)
		return -EINVAL;
	
	learning_ctx.algorithm = algorithm;
	
	ai_info("Learning algorithm set to: %d", algorithm);
	
	return 0;
}

/**
 * ai_learning_engine_init - Initialize online learning engine
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_learning_engine_init(void)
{
	int ret;
	
	/* Initialize context */
	memset(&learning_ctx, 0, sizeof(learning_ctx));
	
	INIT_LIST_HEAD(&learning_ctx.online_samples);
	spin_lock_init(&learning_ctx.samples_lock);
	spin_lock_init(&learning_ctx.metrics_lock);
	spin_lock_init(&learning_ctx.replay_lock);
	
	learning_ctx.algorithm = AI_LEARNING_ADAM;
	learning_ctx.window_size = AI_OL_WINDOW_SIZE;
	learning_ctx.adaptation_rate = AI_OL_ADAPTATION_RATE;
	learning_ctx.forgetting_factor = AI_OL_FORGETTING_FACTOR;
	learning_ctx.exploration_rate = AI_OL_EXPLORATION_RATE;
	learning_ctx.min_confidence = AI_OL_MIN_CONFIDENCE;
	learning_ctx.update_interval_ms = AI_OL_UPDATE_INTERVAL_MS;
	
	/* Initialize learning rate scheduler */
	learning_ctx.lr_scheduler.type = AI_LR_ADAPTIVE;
	learning_ctx.lr_scheduler.initial_lr = AI_NN_LEARNING_RATE;
	learning_ctx.lr_scheduler.current_lr = AI_NN_LEARNING_RATE;
	learning_ctx.lr_scheduler.min_lr = AI_NN_LEARNING_RATE / 10;
	learning_ctx.lr_scheduler.max_lr = AI_NN_LEARNING_RATE * 2;
	learning_ctx.lr_scheduler.decay_rate = 950; /* 0.95 */
	learning_ctx.lr_scheduler.patience = 10;
	learning_ctx.lr_scheduler.best_loss = UINT_MAX;
	
	/* Initialize Adam optimizer state */
	ret = ai_init_adam_state(&learning_ctx.adam_state, 
				 AI_NN_INPUT_SIZE * 32 + 32 * 16 + 16 * AI_NN_OUTPUT_SIZE,
				 32 + 16 + AI_NN_OUTPUT_SIZE);
	if (ret) {
		ai_error("Failed to initialize Adam state: %d", ret);
		return ret;
	}
	
	/* Create learning work queue */
	learning_ctx.learning_wq = create_singlethread_workqueue("ai_online_learning");
	if (!learning_ctx.learning_wq) {
		ai_error("Failed to create online learning work queue");
		ai_free_adam_state(&learning_ctx.adam_state);
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&learning_ctx.learning_work, ai_online_learning_worker);
	
	/* Configuration */
	learning_ctx.learning_active = true;
	learning_ctx.adaptive_lr_enabled = true;
	learning_ctx.experience_replay_enabled = true;
	learning_ctx.exploration_enabled = true;
	learning_ctx.forgetting_enabled = true;
	
	/* Initialize statistics */
	atomic64_set(&learning_ctx.learning_updates, 0);
	atomic64_set(&learning_ctx.adaptation_events, 0);
	atomic64_set(&learning_ctx.exploration_events, 0);
	
	/* Start online learning */
	queue_delayed_work(learning_ctx.learning_wq, &learning_ctx.learning_work,
			   msecs_to_jiffies(learning_ctx.update_interval_ms));
	
	ai_info("Online learning engine initialized with algorithm: %d", learning_ctx.algorithm);
	
	return 0;
}

/**
 * ai_learning_engine_exit - Cleanup online learning engine
 */
void ai_learning_engine_exit(void)
{
	struct ai_online_sample *sample, *tmp;
	
	learning_ctx.learning_active = false;
	
	/* Stop work queue */
	if (learning_ctx.learning_wq) {
		cancel_delayed_work_sync(&learning_ctx.learning_work);
		destroy_workqueue(learning_ctx.learning_wq);
		learning_ctx.learning_wq = NULL;
	}
	
	/* Free optimizer states */
	ai_free_adam_state(&learning_ctx.adam_state);
	
	/* Free online samples */
	spin_lock(&learning_ctx.samples_lock);
	list_for_each_entry_safe(sample, tmp, &learning_ctx.online_samples, list) {
		list_del(&sample->list);
		kfree(sample);
	}
	spin_unlock(&learning_ctx.samples_lock);
	
	ai_info("Online learning engine cleaned up");
}

/**
 * ai_learning_engine_get_statistics - Get learning engine statistics
 */
void ai_learning_engine_get_statistics(u64 *updates, u64 *adaptations, u64 *explorations,
				       u32 *accuracy, u32 *learning_rate)
{
	if (updates)
		*updates = atomic64_read(&learning_ctx.learning_updates);
	
	if (adaptations)
		*adaptations = atomic64_read(&learning_ctx.adaptation_events);
	
	if (explorations)
		*explorations = atomic64_read(&learning_ctx.exploration_events);
	
	if (accuracy)
		*accuracy = learning_ctx.metrics.accuracy;
	
	if (learning_rate)
		*learning_rate = learning_ctx.lr_scheduler.current_lr;
}

/* Export symbols */
EXPORT_SYMBOL(ai_add_online_sample);
EXPORT_SYMBOL(ai_get_learning_metrics);
EXPORT_SYMBOL(ai_set_learning_algorithm);
EXPORT_SYMBOL(ai_learning_engine_get_statistics);