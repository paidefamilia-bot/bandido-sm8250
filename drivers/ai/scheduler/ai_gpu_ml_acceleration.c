/*
 * AI Scheduler GPU ML Acceleration
 * Hardware-accelerated machine learning operations
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
#include <linux/math64.h>

#include "ai_scheduler.h"

/* GPU ML acceleration types */
enum ai_gpu_ml_operation {
	AI_GPU_ML_MATRIX_MULTIPLY = 0,	/* Matrix multiplication */
	AI_GPU_ML_CONVOLUTION_2D,	/* 2D convolution */
	AI_GPU_ML_POOLING,		/* Pooling operations */
	AI_GPU_ML_ACTIVATION,		/* Activation functions */
	AI_GPU_ML_BATCH_NORM,		/* Batch normalization */
	AI_GPU_ML_SOFTMAX,		/* Softmax operation */
	AI_GPU_ML_REDUCTION,		/* Reduction operations */
	AI_GPU_ML_ELEMENTWISE,		/* Element-wise operations */
	AI_GPU_ML_LSTM,			/* LSTM operations */
	AI_GPU_ML_ATTENTION,		/* Attention mechanism */
	AI_GPU_ML_EMBEDDING,		/* Embedding lookup */
	AI_GPU_ML_LOSS_FUNCTION		/* Loss function computation */
};

/* GPU ML tensor descriptor */
struct ai_gpu_tensor {
	u32 tensor_id;			/* Tensor identifier */
	u32 dimensions[4];		/* Tensor dimensions [N,C,H,W] */
	u32 num_dimensions;		/* Number of dimensions */
	u32 data_type;			/* Data type (float32, int8, etc.) */
	size_t element_size;		/* Size of each element */
	size_t total_size;		/* Total tensor size in bytes */
	struct ai_gpu_buffer *buffer;	/* GPU buffer */
	bool is_quantized;		/* Whether tensor is quantized */
	s32 scale;			/* Quantization scale */
	s32 zero_point;			/* Quantization zero point */
	struct list_head list;
};

/* GPU ML operation descriptor */
struct ai_gpu_ml_op {
	u32 operation_id;		/* Operation identifier */
	enum ai_gpu_ml_operation type;	/* Operation type */
	struct ai_gpu_tensor *input_tensors[8];  /* Input tensors */
	struct ai_gpu_tensor *output_tensors[8]; /* Output tensors */
	u32 num_inputs;			/* Number of input tensors */
	u32 num_outputs;		/* Number of output tensors */
	
	/* Operation-specific parameters */
	union {
		struct {
			bool transpose_a;
			bool transpose_b;
			s32 alpha;
			s32 beta;
		} matmul;
		
		struct {
			u32 kernel_size[2];
			u32 stride[2];
			u32 padding[2];
			u32 dilation[2];
			u32 groups;
		} conv2d;
		
		struct {
			u32 kernel_size[2];
			u32 stride[2];
			u32 padding[2];
			enum {
				AI_POOL_MAX,
				AI_POOL_AVG,
				AI_POOL_GLOBAL_AVG
			} pool_type;
		} pooling;
		
		struct {
			enum {
				AI_ACTIVATION_RELU,
				AI_ACTIVATION_SIGMOID,
				AI_ACTIVATION_TANH,
				AI_ACTIVATION_GELU,
				AI_ACTIVATION_SWISH
			} activation_type;
			s32 alpha;	/* For leaky ReLU, etc. */
		} activation;
		
		struct {
			s32 epsilon;
			bool affine;
		} batch_norm;
		
		struct {
			u32 axis;
		} softmax;
		
		struct {
			enum {
				AI_REDUCE_SUM,
				AI_REDUCE_MEAN,
				AI_REDUCE_MAX,
				AI_REDUCE_MIN
			} reduce_type;
			u32 axes[4];
			u32 num_axes;
			bool keep_dims;
		} reduction;
	} params;
	
	/* Execution state */
	u64 submit_time;
	u64 start_time;
	u64 complete_time;
	bool is_complete;
	u32 gpu_kernel_id;		/* Associated GPU kernel */
	
	struct list_head list;
};

/* GPU ML acceleration context */
struct ai_gpu_ml_ctx {
	/* Tensor management */
	struct list_head tensors;
	spinlock_t tensors_lock;
	u32 next_tensor_id;
	u32 tensor_count;
	
	/* Operation queue */
	struct list_head pending_ops;
	struct list_head active_ops;
	struct list_head completed_ops;
	spinlock_t ops_lock;
	u32 next_operation_id;
	
	/* GPU kernels for ML operations */
	u32 ml_kernels[AI_GPU_ML_LOSS_FUNCTION + 1];
	bool kernels_loaded;
	
	/* ML acceleration worker */
	struct workqueue_struct *ml_wq;
	struct delayed_work ml_work;
	bool ml_active;
	
	/* Performance metrics */
	struct {
		u64 total_operations;
		u64 successful_operations;
		u64 failed_operations;
		u64 total_execution_time_ns;
		u64 total_flops;		/* Floating point operations */
		u32 average_throughput_gflops;	/* GFLOPS throughput */
		u32 peak_throughput_gflops;
		u64 memory_bandwidth_gbps;	/* Memory bandwidth utilization */
	} metrics;
	spinlock_t metrics_lock;
	
	/* Statistics per operation type */
	struct {
		u64 count;
		u64 total_time_ns;
		u64 total_flops;
		u32 average_time_us;
		u32 throughput_gflops;
	} op_stats[AI_GPU_ML_LOSS_FUNCTION + 1];
	
	/* Configuration */
	bool quantization_enabled;
	bool mixed_precision_enabled;
	bool tensor_fusion_enabled;
	bool memory_optimization_enabled;
	u32 max_concurrent_ops;
	
	/* Statistics */
	atomic64_t tensors_created;
	atomic64_t operations_submitted;
	atomic64_t operations_completed;
	atomic64_t gpu_memory_used;
};

static struct ai_gpu_ml_ctx ml_ctx;

/* Operation type names */
static const char *ml_operation_names[] = {
	"MATRIX_MULTIPLY",
	"CONVOLUTION_2D",
	"POOLING",
	"ACTIVATION",
	"BATCH_NORM",
	"SOFTMAX",
	"REDUCTION",
	"ELEMENTWISE",
	"LSTM",
	"ATTENTION",
	"EMBEDDING",
	"LOSS_FUNCTION"
};

/**
 * ai_gpu_create_tensor - Create a GPU tensor
 * @dimensions: Tensor dimensions
 * @num_dims: Number of dimensions
 * @data_type: Data type
 * @element_size: Size of each element
 * 
 * Returns: Tensor pointer or NULL on failure
 */
struct ai_gpu_tensor *ai_gpu_create_tensor(const u32 *dimensions, u32 num_dims,
					   u32 data_type, size_t element_size)
{
	struct ai_gpu_tensor *tensor;
	size_t total_size = element_size;
	u32 i;
	
	if (!dimensions || num_dims == 0 || num_dims > 4 || element_size == 0)
		return NULL;
	
	tensor = kzalloc(sizeof(*tensor), GFP_KERNEL);
	if (!tensor)
		return NULL;
	
	tensor->tensor_id = ml_ctx.next_tensor_id++;
	tensor->num_dimensions = num_dims;
	tensor->data_type = data_type;
	tensor->element_size = element_size;
	
	/* Copy dimensions and calculate total size */
	for (i = 0; i < num_dims; i++) {
		tensor->dimensions[i] = dimensions[i];
		total_size *= dimensions[i];
	}
	tensor->total_size = total_size;
	
	/* Allocate GPU buffer */
	tensor->buffer = ai_gpu_allocate_buffer(total_size, 0);
	if (!tensor->buffer) {
		kfree(tensor);
		return NULL;
	}
	
	/* Add to tensors list */
	spin_lock(&ml_ctx.tensors_lock);
	list_add(&tensor->list, &ml_ctx.tensors);
	ml_ctx.tensor_count++;
	spin_unlock(&ml_ctx.tensors_lock);
	
	atomic64_inc(&ml_ctx.tensors_created);
	atomic64_add(total_size, &ml_ctx.gpu_memory_used);
	
	ai_verbose("GPU tensor created: ID=%u, dims=[%u,%u,%u,%u], size=%zu bytes",
		   tensor->tensor_id, 
		   num_dims > 0 ? dimensions[0] : 0,
		   num_dims > 1 ? dimensions[1] : 0,
		   num_dims > 2 ? dimensions[2] : 0,
		   num_dims > 3 ? dimensions[3] : 0,
		   total_size);
	
	return tensor;
}

/**
 * ai_gpu_destroy_tensor - Destroy a GPU tensor
 * @tensor: Tensor to destroy
 */
void ai_gpu_destroy_tensor(struct ai_gpu_tensor *tensor)
{
	if (!tensor)
		return;
	
	/* Remove from tensors list */
	spin_lock(&ml_ctx.tensors_lock);
	list_del(&tensor->list);
	ml_ctx.tensor_count--;
	spin_unlock(&ml_ctx.tensors_lock);
	
	/* Free GPU buffer */
	if (tensor->buffer) {
		atomic64_sub(tensor->total_size, &ml_ctx.gpu_memory_used);
		ai_gpu_free_buffer(tensor->buffer);
	}
	
	kfree(tensor);
	
	ai_verbose("GPU tensor destroyed");
}

/**
 * ai_gpu_calculate_flops - Calculate FLOPs for an operation
 * @op: ML operation
 * 
 * Returns: Number of floating point operations
 */
static u64 ai_gpu_calculate_flops(const struct ai_gpu_ml_op *op)
{
	u64 flops = 0;
	
	switch (op->type) {
	case AI_GPU_ML_MATRIX_MULTIPLY:
		if (op->num_inputs >= 2 && op->input_tensors[0] && op->input_tensors[1]) {
			/* For matrix multiply A[M,K] * B[K,N] = C[M,N]: 2*M*K*N FLOPs */
			u32 M = op->input_tensors[0]->dimensions[0];
			u32 K = op->input_tensors[0]->dimensions[1];
			u32 N = op->input_tensors[1]->dimensions[1];
			flops = 2ULL * M * K * N;
		}
		break;
		
	case AI_GPU_ML_CONVOLUTION_2D:
		if (op->num_inputs >= 2 && op->input_tensors[0] && op->input_tensors[1]) {
			/* For conv2d: 2 * output_elements * kernel_elements */
			u32 batch = op->input_tensors[0]->dimensions[0];
			u32 out_channels = op->input_tensors[1]->dimensions[0];
			u32 out_h = op->output_tensors[0]->dimensions[2];
			u32 out_w = op->output_tensors[0]->dimensions[3];
			u32 kernel_h = op->params.conv2d.kernel_size[0];
			u32 kernel_w = op->params.conv2d.kernel_size[1];
			u32 in_channels = op->input_tensors[0]->dimensions[1];
			
			flops = 2ULL * batch * out_channels * out_h * out_w * 
				kernel_h * kernel_w * in_channels;
		}
		break;
		
	case AI_GPU_ML_ACTIVATION:
		if (op->num_inputs >= 1 && op->input_tensors[0]) {
			/* Activation functions: 1 FLOP per element */
			u32 elements = 1;
			for (u32 i = 0; i < op->input_tensors[0]->num_dimensions; i++) {
				elements *= op->input_tensors[0]->dimensions[i];
			}
			flops = elements;
		}
		break;
		
	case AI_GPU_ML_SOFTMAX:
		if (op->num_inputs >= 1 && op->input_tensors[0]) {
			/* Softmax: ~3 FLOPs per element (exp + sum + div) */
			u32 elements = 1;
			for (u32 i = 0; i < op->input_tensors[0]->num_dimensions; i++) {
				elements *= op->input_tensors[0]->dimensions[i];
			}
			flops = 3ULL * elements;
		}
		break;
		
	default:
		/* Default: 1 FLOP per output element */
		if (op->num_outputs >= 1 && op->output_tensors[0]) {
			u32 elements = 1;
			for (u32 i = 0; i < op->output_tensors[0]->num_dimensions; i++) {
				elements *= op->output_tensors[0]->dimensions[i];
			}
			flops = elements;
		}
		break;
	}
	
	return flops;
}

/**
 * ai_gpu_submit_ml_operation - Submit ML operation to GPU
 * @type: Operation type
 * @input_tensors: Input tensors
 * @output_tensors: Output tensors
 * @num_inputs: Number of input tensors
 * @num_outputs: Number of output tensors
 * @params: Operation parameters (optional)
 * 
 * Returns: Operation ID on success, negative error code on failure
 */
int ai_gpu_submit_ml_operation(enum ai_gpu_ml_operation type,
			       struct ai_gpu_tensor **input_tensors,
			       struct ai_gpu_tensor **output_tensors,
			       u32 num_inputs, u32 num_outputs,
			       const void *params)
{
	struct ai_gpu_ml_op *op;
	u32 i;
	
	if (type > AI_GPU_ML_LOSS_FUNCTION || num_inputs > 8 || num_outputs > 8)
		return -EINVAL;
	
	op = kzalloc(sizeof(*op), GFP_KERNEL);
	if (!op)
		return -ENOMEM;
	
	op->operation_id = ml_ctx.next_operation_id++;
	op->type = type;
	op->num_inputs = num_inputs;
	op->num_outputs = num_outputs;
	op->submit_time = ktime_get_ns();
	
	/* Copy tensor references */
	for (i = 0; i < num_inputs; i++) {
		op->input_tensors[i] = input_tensors[i];
	}
	
	for (i = 0; i < num_outputs; i++) {
		op->output_tensors[i] = output_tensors[i];
	}
	
	/* Copy operation parameters */
	if (params) {
		switch (type) {
		case AI_GPU_ML_MATRIX_MULTIPLY:
			op->params.matmul = *(const typeof(op->params.matmul) *)params;
			break;
		case AI_GPU_ML_CONVOLUTION_2D:
			op->params.conv2d = *(const typeof(op->params.conv2d) *)params;
			break;
		case AI_GPU_ML_POOLING:
			op->params.pooling = *(const typeof(op->params.pooling) *)params;
			break;
		case AI_GPU_ML_ACTIVATION:
			op->params.activation = *(const typeof(op->params.activation) *)params;
			break;
		default:
			/* No parameters needed */
			break;
		}
	}
	
	/* Set GPU kernel ID */
	if (ml_ctx.kernels_loaded && type <= AI_GPU_ML_LOSS_FUNCTION) {
		op->gpu_kernel_id = ml_ctx.ml_kernels[type];
	}
	
	/* Add to pending operations */
	spin_lock(&ml_ctx.ops_lock);
	list_add_tail(&op->list, &ml_ctx.pending_ops);
	spin_unlock(&ml_ctx.ops_lock);
	
	atomic64_inc(&ml_ctx.operations_submitted);
	
	/* Wake up ML worker */
	if (ml_ctx.ml_active) {
		queue_delayed_work(ml_ctx.ml_wq, &ml_ctx.ml_work, 0);
	}
	
	ai_verbose("GPU ML operation submitted: ID=%u, type=%s, inputs=%u, outputs=%u",
		   op->operation_id, ml_operation_names[type], num_inputs, num_outputs);
	
	return op->operation_id;
}

/**
 * ai_gpu_ml_worker - GPU ML acceleration worker
 * @work: Work structure
 */
static void ai_gpu_ml_worker(struct work_struct *work)
{
	struct ai_gpu_ml_op *op, *tmp;
	ktime_t start_time, end_time;
	u32 ops_processed = 0;
	
	if (!ml_ctx.ml_active)
		return;
	
	start_time = ktime_get();
	
	/* Process pending operations */
	spin_lock(&ml_ctx.ops_lock);
	list_for_each_entry_safe(op, tmp, &ml_ctx.pending_ops, list) {
		/* Move to active operations */
		list_move(&op->list, &ml_ctx.active_ops);
		
		op->start_time = ktime_get_ns();
		
		/* Submit to GPU (simplified - would use actual GPU commands) */
		if (op->gpu_kernel_id > 0) {
			struct ai_gpu_buffer *input_buffers[8] = {0};
			struct ai_gpu_buffer *output_buffers[8] = {0};
			u32 global_work_size[3] = {1, 1, 1};
			u32 i;
			
			/* Prepare input/output buffers */
			for (i = 0; i < op->num_inputs; i++) {
				if (op->input_tensors[i]) {
					input_buffers[i] = op->input_tensors[i]->buffer;
				}
			}
			
			for (i = 0; i < op->num_outputs; i++) {
				if (op->output_tensors[i]) {
					output_buffers[i] = op->output_tensors[i]->buffer;
				}
			}
			
			/* Calculate work size based on output tensor */
			if (op->num_outputs > 0 && op->output_tensors[0]) {
				global_work_size[0] = op->output_tensors[0]->dimensions[0];
				if (op->output_tensors[0]->num_dimensions > 1) {
					global_work_size[1] = op->output_tensors[0]->dimensions[1];
				}
				if (op->output_tensors[0]->num_dimensions > 2) {
					global_work_size[2] = op->output_tensors[0]->dimensions[2];
				}
			}
			
			/* Submit GPU command */
			ai_gpu_submit_command(op->gpu_kernel_id, input_buffers, output_buffers,
					      op->num_inputs, op->num_outputs, global_work_size);
		}
		
		/* Simulate completion (in real implementation, this would be async) */
		op->complete_time = ktime_get_ns();
		op->is_complete = true;
		
		/* Move to completed operations */
		list_move(&op->list, &ml_ctx.completed_ops);
		
		/* Update statistics */
		u64 execution_time = op->complete_time - op->start_time;
		u64 flops = ai_gpu_calculate_flops(op);
		
		spin_lock(&ml_ctx.metrics_lock);
		ml_ctx.metrics.total_operations++;
		ml_ctx.metrics.successful_operations++;
		ml_ctx.metrics.total_execution_time_ns += execution_time;
		ml_ctx.metrics.total_flops += flops;
		
		/* Update per-operation statistics */
		ml_ctx.op_stats[op->type].count++;
		ml_ctx.op_stats[op->type].total_time_ns += execution_time;
		ml_ctx.op_stats[op->type].total_flops += flops;
		ml_ctx.op_stats[op->type].average_time_us = 
			ml_ctx.op_stats[op->type].total_time_ns / 
			(ml_ctx.op_stats[op->type].count * 1000);
		
		if (execution_time > 0) {
			u32 gflops = (flops / 1000000) / (execution_time / 1000000000);
			ml_ctx.op_stats[op->type].throughput_gflops = gflops;
			
			if (gflops > ml_ctx.metrics.peak_throughput_gflops) {
				ml_ctx.metrics.peak_throughput_gflops = gflops;
			}
		}
		
		spin_unlock(&ml_ctx.metrics_lock);
		
		atomic64_inc(&ml_ctx.operations_completed);
		
		ai_verbose("GPU ML operation completed: ID=%u, type=%s, time=%llu ns, flops=%llu",
			   op->operation_id, ml_operation_names[op->type], execution_time, flops);
		
		ops_processed++;
		
		/* Limit processing per cycle */
		if (ops_processed >= 5)
			break;
	}
	spin_unlock(&ml_ctx.ops_lock);
	
	end_time = ktime_get();
	u64 worker_time = ktime_to_ns(ktime_sub(end_time, start_time));
	
	ai_verbose("GPU ML worker processed %u operations in %llu ns", ops_processed, worker_time);
	
	/* Schedule next work if there are pending operations */
	spin_lock(&ml_ctx.ops_lock);
	if (!list_empty(&ml_ctx.pending_ops) && ml_ctx.ml_active) {
		queue_delayed_work(ml_ctx.ml_wq, &ml_ctx.ml_work, msecs_to_jiffies(5));
	}
	spin_unlock(&ml_ctx.ops_lock);
}

/**
 * ai_gpu_ml_get_metrics - Get ML acceleration metrics
 * @total_ops: Total operations executed
 * @avg_throughput: Average throughput in GFLOPS
 * @peak_throughput: Peak throughput in GFLOPS
 * @memory_usage: GPU memory usage in bytes
 */
void ai_gpu_ml_get_metrics(u64 *total_ops, u32 *avg_throughput, u32 *peak_throughput,
			   u64 *memory_usage)
{
	spin_lock(&ml_ctx.metrics_lock);
	
	if (total_ops)
		*total_ops = ml_ctx.metrics.total_operations;
	
	if (avg_throughput) {
		if (ml_ctx.metrics.total_execution_time_ns > 0) {
			*avg_throughput = (ml_ctx.metrics.total_flops / 1000000) / 
					  (ml_ctx.metrics.total_execution_time_ns / 1000000000);
		} else {
			*avg_throughput = 0;
		}
	}
	
	if (peak_throughput)
		*peak_throughput = ml_ctx.metrics.peak_throughput_gflops;
	
	spin_unlock(&ml_ctx.metrics_lock);
	
	if (memory_usage)
		*memory_usage = atomic64_read(&ml_ctx.gpu_memory_used);
}

/**
 * ai_gpu_ml_acceleration_init - Initialize GPU ML acceleration
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_gpu_ml_acceleration_init(void)
{
	u32 i;
	
	/* Initialize context */
	memset(&ml_ctx, 0, sizeof(ml_ctx));
	
	INIT_LIST_HEAD(&ml_ctx.tensors);
	INIT_LIST_HEAD(&ml_ctx.pending_ops);
	INIT_LIST_HEAD(&ml_ctx.active_ops);
	INIT_LIST_HEAD(&ml_ctx.completed_ops);
	
	spin_lock_init(&ml_ctx.tensors_lock);
	spin_lock_init(&ml_ctx.ops_lock);
	spin_lock_init(&ml_ctx.metrics_lock);
	
	ml_ctx.next_tensor_id = 1;
	ml_ctx.next_operation_id = 1;
	ml_ctx.max_concurrent_ops = 8;
	
	/* Load ML kernels */
	for (i = 0; i <= AI_GPU_ML_LOSS_FUNCTION; i++) {
		int kernel_id = ai_gpu_load_kernel(AI_GPU_ACCEL_NEURAL_NETWORK, NULL, 0);
		if (kernel_id > 0) {
			ml_ctx.ml_kernels[i] = kernel_id;
		}
	}
	ml_ctx.kernels_loaded = true;
	
	/* Create ML work queue */
	ml_ctx.ml_wq = create_singlethread_workqueue("ai_gpu_ml_acceleration");
	if (!ml_ctx.ml_wq) {
		ai_error("Failed to create GPU ML acceleration work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&ml_ctx.ml_work, ai_gpu_ml_worker);
	
	/* Configuration */
	ml_ctx.ml_active = true;
	ml_ctx.quantization_enabled = true;
	ml_ctx.mixed_precision_enabled = true;
	ml_ctx.tensor_fusion_enabled = true;
	ml_ctx.memory_optimization_enabled = true;
	
	/* Initialize statistics */
	atomic64_set(&ml_ctx.tensors_created, 0);
	atomic64_set(&ml_ctx.operations_submitted, 0);
	atomic64_set(&ml_ctx.operations_completed, 0);
	atomic64_set(&ml_ctx.gpu_memory_used, 0);
	
	ai_info("GPU ML acceleration initialized with %d operation types", 
		AI_GPU_ML_LOSS_FUNCTION + 1);
	
	return 0;
}

/**
 * ai_gpu_ml_acceleration_exit - Cleanup GPU ML acceleration
 */
void ai_gpu_ml_acceleration_exit(void)
{
	struct ai_gpu_tensor *tensor, *tmp_tensor;
	struct ai_gpu_ml_op *op, *tmp_op;
	
	ml_ctx.ml_active = false;
	
	/* Stop ML worker */
	if (ml_ctx.ml_wq) {
		cancel_delayed_work_sync(&ml_ctx.ml_work);
		destroy_workqueue(ml_ctx.ml_wq);
		ml_ctx.ml_wq = NULL;
	}
	
	/* Free operations */
	spin_lock(&ml_ctx.ops_lock);
	list_for_each_entry_safe(op, tmp_op, &ml_ctx.pending_ops, list) {
		list_del(&op->list);
		kfree(op);
	}
	list_for_each_entry_safe(op, tmp_op, &ml_ctx.active_ops, list) {
		list_del(&op->list);
		kfree(op);
	}
	list_for_each_entry_safe(op, tmp_op, &ml_ctx.completed_ops, list) {
		list_del(&op->list);
		kfree(op);
	}
	spin_unlock(&ml_ctx.ops_lock);
	
	/* Free tensors */
	spin_lock(&ml_ctx.tensors_lock);
	list_for_each_entry_safe(tensor, tmp_tensor, &ml_ctx.tensors, list) {
		list_del(&tensor->list);
		if (tensor->buffer) {
			ai_gpu_free_buffer(tensor->buffer);
		}
		kfree(tensor);
	}
	spin_unlock(&ml_ctx.tensors_lock);
	
	ai_info("GPU ML acceleration cleaned up");
}

/**
 * ai_gpu_ml_acceleration_get_statistics - Get ML acceleration statistics
 */
void ai_gpu_ml_acceleration_get_statistics(u64 *tensors_created, u64 *operations_submitted,
					   u64 *operations_completed, u64 *gpu_memory_used)
{
	if (tensors_created)
		*tensors_created = atomic64_read(&ml_ctx.tensors_created);
	
	if (operations_submitted)
		*operations_submitted = atomic64_read(&ml_ctx.operations_submitted);
	
	if (operations_completed)
		*operations_completed = atomic64_read(&ml_ctx.operations_completed);
	
	if (gpu_memory_used)
		*gpu_memory_used = atomic64_read(&ml_ctx.gpu_memory_used);
}

/* Export symbols */
EXPORT_SYMBOL(ai_gpu_create_tensor);
EXPORT_SYMBOL(ai_gpu_destroy_tensor);
EXPORT_SYMBOL(ai_gpu_submit_ml_operation);
EXPORT_SYMBOL(ai_gpu_ml_get_metrics);
EXPORT_SYMBOL(ai_gpu_ml_acceleration_get_statistics);