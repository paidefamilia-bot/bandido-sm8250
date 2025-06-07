/*
 * AI Scheduler Data Persistence Mechanism
 * Advanced data storage and retrieval for AI learning
 * 
 * Copyright (C) 2024 Bandido Kernel Team
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/ktime.h>
#include <linux/jiffies.h>
#include <linux/atomic.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/timer.h>
#include <linux/crc32.h>
#include <linux/compress.h>
#include <linux/zlib.h>

#include "ai_scheduler.h"

/* Data persistence configuration */
#define AI_PERSIST_MAGIC		0x41495343	/* "AISC" */
#define AI_PERSIST_VERSION		1
#define AI_PERSIST_MAX_ENTRIES		10000
#define AI_PERSIST_BUFFER_SIZE		(1024 * 1024)	/* 1MB buffer */
#define AI_PERSIST_SAVE_INTERVAL_MS	30000		/* Save every 30 seconds */
#define AI_PERSIST_COMPRESSION_LEVEL	6		/* Compression level */
#define AI_PERSIST_BASE_PATH		"/data/ai_scheduler"

/* Data types for persistence */
enum ai_persist_data_type {
	AI_PERSIST_TASK_FEATURES = 0,
	AI_PERSIST_BEHAVIOR_PATTERNS,
	AI_PERSIST_CPU_PATTERNS,
	AI_PERSIST_MEMORY_PATTERNS,
	AI_PERSIST_IO_PATTERNS,
	AI_PERSIST_USER_PATTERNS,
	AI_PERSIST_APP_CLASSIFICATIONS,
	AI_PERSIST_PERFORMANCE_METRICS,
	AI_PERSIST_NEURAL_WEIGHTS,
	AI_PERSIST_LEARNING_STATE,
	AI_PERSIST_STATISTICS,
	AI_PERSIST_TYPE_MAX
};

/* Persistence entry header */
struct ai_persist_entry_header {
	u32 magic;			/* Magic number */
	u32 version;			/* Format version */
	enum ai_persist_data_type type;	/* Data type */
	u32 size;			/* Data size */
	u32 checksum;			/* CRC32 checksum */
	u64 timestamp;			/* Creation timestamp */
	u32 flags;			/* Entry flags */
	u32 reserved;			/* Reserved for future use */
} __packed;

/* Persistence file header */
struct ai_persist_file_header {
	u32 magic;			/* Magic number */
	u32 version;			/* Format version */
	u32 entry_count;		/* Number of entries */
	u32 total_size;			/* Total file size */
	u64 creation_time;		/* File creation time */
	u64 last_update;		/* Last update time */
	u32 flags;			/* File flags */
	u32 checksum;			/* Header checksum */
} __packed;

/* Persistence entry */
struct ai_persist_entry {
	struct ai_persist_entry_header header;
	void *data;			/* Entry data */
	struct list_head list;
};

/* Compression context */
struct ai_compress_ctx {
	struct z_stream_s stream;
	void *workspace;
	size_t workspace_size;
	bool initialized;
};

/* Persistence context */
struct ai_data_persistence_ctx {
	/* Entry management */
	struct list_head entries[AI_PERSIST_TYPE_MAX];
	spinlock_t entries_lock[AI_PERSIST_TYPE_MAX];
	u32 entry_counts[AI_PERSIST_TYPE_MAX];
	
	/* Buffer management */
	void *write_buffer;
	void *read_buffer;
	void *compress_buffer;
	size_t buffer_size;
	mutex_t buffer_lock;
	
	/* Compression */
	struct ai_compress_ctx compress_ctx;
	bool compression_enabled;
	
	/* File operations */
	char base_path[256];
	struct file *current_file;
	loff_t file_offset;
	mutex_t file_lock;
	
	/* Persistence worker */
	struct delayed_work save_work;
	struct workqueue_struct *save_wq;
	bool auto_save_enabled;
	u32 save_interval_ms;
	
	/* Statistics */
	atomic64_t entries_saved;
	atomic64_t entries_loaded;
	atomic64_t bytes_written;
	atomic64_t bytes_read;
	atomic64_t save_operations;
	atomic64_t load_operations;
	u32 compression_ratio;
	u32 save_success_rate;
	
	/* State */
	bool persistence_enabled;
	bool dirty;			/* Data needs saving */
	u64 last_save_time;
	u64 last_load_time;
};

static struct ai_data_persistence_ctx persist_ctx;

/* Data type names */
static const char *persist_type_names[] = {
	"TASK_FEATURES",
	"BEHAVIOR_PATTERNS",
	"CPU_PATTERNS",
	"MEMORY_PATTERNS",
	"IO_PATTERNS",
	"USER_PATTERNS",
	"APP_CLASSIFICATIONS",
	"PERFORMANCE_METRICS",
	"NEURAL_WEIGHTS",
	"LEARNING_STATE",
	"STATISTICS"
};

/**
 * ai_init_compression - Initialize compression context
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_init_compression(void)
{
	struct ai_compress_ctx *ctx = &persist_ctx.compress_ctx;
	int ret;
	
	if (ctx->initialized)
		return 0;
	
	/* Allocate workspace */
	ctx->workspace_size = zlib_deflate_workspacesize(MAX_WBITS, MAX_MEM_LEVEL);
	ctx->workspace = vmalloc(ctx->workspace_size);
	if (!ctx->workspace)
		return -ENOMEM;
	
	/* Initialize deflate stream */
	memset(&ctx->stream, 0, sizeof(ctx->stream));
	ctx->stream.workspace = ctx->workspace;
	
	ret = zlib_deflateInit(&ctx->stream, AI_PERSIST_COMPRESSION_LEVEL);
	if (ret != Z_OK) {
		vfree(ctx->workspace);
		ctx->workspace = NULL;
		return -EINVAL;
	}
	
	ctx->initialized = true;
	ai_verbose("Compression initialized with level %d", AI_PERSIST_COMPRESSION_LEVEL);
	
	return 0;
}

/**
 * ai_cleanup_compression - Cleanup compression context
 */
static void ai_cleanup_compression(void)
{
	struct ai_compress_ctx *ctx = &persist_ctx.compress_ctx;
	
	if (!ctx->initialized)
		return;
	
	zlib_deflateEnd(&ctx->stream);
	
	if (ctx->workspace) {
		vfree(ctx->workspace);
		ctx->workspace = NULL;
	}
	
	ctx->initialized = false;
}

/**
 * ai_compress_data - Compress data using zlib
 * @input: Input data
 * @input_size: Input data size
 * @output: Output buffer
 * @output_size: Output buffer size
 * @compressed_size: Output compressed size
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_compress_data(const void *input, size_t input_size,
			    void *output, size_t output_size,
			    size_t *compressed_size)
{
	struct ai_compress_ctx *ctx = &persist_ctx.compress_ctx;
	int ret;
	
	if (!ctx->initialized) {
		ret = ai_init_compression();
		if (ret)
			return ret;
	}
	
	/* Reset stream */
	ret = zlib_deflateReset(&ctx->stream);
	if (ret != Z_OK)
		return -EINVAL;
	
	/* Set input and output */
	ctx->stream.next_in = (u8 *)input;
	ctx->stream.avail_in = input_size;
	ctx->stream.next_out = output;
	ctx->stream.avail_out = output_size;
	
	/* Compress */
	ret = zlib_deflate(&ctx->stream, Z_FINISH);
	if (ret != Z_STREAM_END)
		return -EINVAL;
	
	*compressed_size = output_size - ctx->stream.avail_out;
	
	/* Update compression ratio */
	if (input_size > 0) {
		persist_ctx.compression_ratio = (*compressed_size * 100) / input_size;
	}
	
	return 0;
}

/**
 * ai_decompress_data - Decompress data using zlib
 * @input: Compressed input data
 * @input_size: Compressed input size
 * @output: Output buffer
 * @output_size: Output buffer size
 * @decompressed_size: Output decompressed size
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_decompress_data(const void *input, size_t input_size,
			      void *output, size_t output_size,
			      size_t *decompressed_size)
{
	struct z_stream_s stream;
	void *workspace;
	size_t workspace_size;
	int ret;
	
	/* Allocate workspace */
	workspace_size = zlib_inflate_workspacesize();
	workspace = vmalloc(workspace_size);
	if (!workspace)
		return -ENOMEM;
	
	/* Initialize inflate stream */
	memset(&stream, 0, sizeof(stream));
	stream.workspace = workspace;
	stream.next_in = (u8 *)input;
	stream.avail_in = input_size;
	stream.next_out = output;
	stream.avail_out = output_size;
	
	ret = zlib_inflateInit(&stream);
	if (ret != Z_OK) {
		vfree(workspace);
		return -EINVAL;
	}
	
	/* Decompress */
	ret = zlib_inflate(&stream, Z_FINISH);
	if (ret != Z_STREAM_END) {
		zlib_inflateEnd(&stream);
		vfree(workspace);
		return -EINVAL;
	}
	
	*decompressed_size = output_size - stream.avail_out;
	
	zlib_inflateEnd(&stream);
	vfree(workspace);
	
	return 0;
}

/**
 * ai_create_persist_entry - Create a persistence entry
 * @type: Data type
 * @data: Data to persist
 * @size: Data size
 * 
 * Returns: Persistence entry or NULL on failure
 */
static struct ai_persist_entry *ai_create_persist_entry(enum ai_persist_data_type type,
							const void *data, size_t size)
{
	struct ai_persist_entry *entry;
	void *entry_data;
	
	if (type >= AI_PERSIST_TYPE_MAX || !data || size == 0)
		return NULL;
	
	entry = kzalloc(sizeof(*entry), GFP_KERNEL);
	if (!entry)
		return NULL;
	
	entry_data = kmalloc(size, GFP_KERNEL);
	if (!entry_data) {
		kfree(entry);
		return NULL;
	}
	
	/* Copy data */
	memcpy(entry_data, data, size);
	
	/* Fill header */
	entry->header.magic = AI_PERSIST_MAGIC;
	entry->header.version = AI_PERSIST_VERSION;
	entry->header.type = type;
	entry->header.size = size;
	entry->header.timestamp = ktime_get_ns();
	entry->header.checksum = crc32(0, entry_data, size);
	entry->header.flags = 0;
	
	entry->data = entry_data;
	
	return entry;
}

/**
 * ai_free_persist_entry - Free a persistence entry
 * @entry: Entry to free
 */
static void ai_free_persist_entry(struct ai_persist_entry *entry)
{
	if (!entry)
		return;
	
	if (entry->data)
		kfree(entry->data);
	
	kfree(entry);
}

/**
 * ai_add_persist_entry - Add entry to persistence list
 * @type: Data type
 * @data: Data to persist
 * @size: Data size
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_add_persist_entry(enum ai_persist_data_type type, const void *data, size_t size)
{
	struct ai_persist_entry *entry;
	
	if (!persist_ctx.persistence_enabled || type >= AI_PERSIST_TYPE_MAX)
		return -EINVAL;
	
	/* Check entry limit */
	if (persist_ctx.entry_counts[type] >= AI_PERSIST_MAX_ENTRIES) {
		ai_warn("Maximum entries reached for type %s", persist_type_names[type]);
		return -ENOSPC;
	}
	
	entry = ai_create_persist_entry(type, data, size);
	if (!entry)
		return -ENOMEM;
	
	/* Add to list */
	spin_lock(&persist_ctx.entries_lock[type]);
	list_add_tail(&entry->list, &persist_ctx.entries[type]);
	persist_ctx.entry_counts[type]++;
	spin_unlock(&persist_ctx.entries_lock[type]);
	
	persist_ctx.dirty = true;
	
	ai_verbose("Added persistence entry: type=%s, size=%zu", 
		   persist_type_names[type], size);
	
	return 0;
}

/**
 * ai_save_entries_to_buffer - Save entries to buffer
 * @type: Data type to save
 * @buffer: Output buffer
 * @buffer_size: Buffer size
 * @written: Output bytes written
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_save_entries_to_buffer(enum ai_persist_data_type type, void *buffer,
				     size_t buffer_size, size_t *written)
{
	struct ai_persist_entry *entry;
	u8 *buf_ptr = buffer;
	size_t remaining = buffer_size;
	size_t total_written = 0;
	
	spin_lock(&persist_ctx.entries_lock[type]);
	
	list_for_each_entry(entry, &persist_ctx.entries[type], list) {
		size_t entry_size = sizeof(entry->header) + entry->header.size;
		
		if (entry_size > remaining) {
			spin_unlock(&persist_ctx.entries_lock[type]);
			ai_warn("Buffer too small for persistence entries");
			return -ENOSPC;
		}
		
		/* Copy header */
		memcpy(buf_ptr, &entry->header, sizeof(entry->header));
		buf_ptr += sizeof(entry->header);
		
		/* Copy data */
		memcpy(buf_ptr, entry->data, entry->header.size);
		buf_ptr += entry->header.size;
		
		remaining -= entry_size;
		total_written += entry_size;
	}
	
	spin_unlock(&persist_ctx.entries_lock[type]);
	
	*written = total_written;
	return 0;
}

/**
 * ai_write_file_data - Write data to file
 * @filename: File name
 * @data: Data to write
 * @size: Data size
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_write_file_data(const char *filename, const void *data, size_t size)
{
	struct file *file;
	loff_t pos = 0;
	ssize_t written;
	int ret = 0;
	
	file = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(file)) {
		ai_error("Failed to open file %s for writing: %ld", filename, PTR_ERR(file));
		return PTR_ERR(file);
	}
	
	written = kernel_write(file, data, size, &pos);
	if (written != size) {
		ai_error("Failed to write complete data to %s: %zd/%zu", 
			 filename, written, size);
		ret = -EIO;
	}
	
	filp_close(file, NULL);
	
	if (ret == 0) {
		atomic64_add(written, &persist_ctx.bytes_written);
		ai_verbose("Wrote %zu bytes to %s", size, filename);
	}
	
	return ret;
}

/**
 * ai_read_file_data - Read data from file
 * @filename: File name
 * @buffer: Buffer to read into
 * @buffer_size: Buffer size
 * @read_size: Output bytes read
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_read_file_data(const char *filename, void *buffer, 
			     size_t buffer_size, size_t *read_size)
{
	struct file *file;
	loff_t pos = 0;
	ssize_t read;
	int ret = 0;
	
	file = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(file)) {
		ai_error("Failed to open file %s for reading: %ld", filename, PTR_ERR(file));
		return PTR_ERR(file);
	}
	
	read = kernel_read(file, buffer, buffer_size, &pos);
	if (read < 0) {
		ai_error("Failed to read data from %s: %zd", filename, read);
		ret = read;
	} else {
		*read_size = read;
		atomic64_add(read, &persist_ctx.bytes_read);
		ai_verbose("Read %zd bytes from %s", read, filename);
	}
	
	filp_close(file, NULL);
	
	return ret;
}

/**
 * ai_save_data_type - Save all entries of a specific type
 * @type: Data type to save
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_save_data_type(enum ai_persist_data_type type)
{
	char filename[512];
	struct ai_persist_file_header file_header;
	void *save_buffer = NULL;
	void *final_buffer;
	size_t data_size, final_size;
	int ret;
	
	if (persist_ctx.entry_counts[type] == 0)
		return 0; /* Nothing to save */
	
	/* Allocate save buffer */
	save_buffer = vmalloc(persist_ctx.buffer_size);
	if (!save_buffer)
		return -ENOMEM;
	
	/* Save entries to buffer */
	ret = ai_save_entries_to_buffer(type, save_buffer, persist_ctx.buffer_size, &data_size);
	if (ret) {
		vfree(save_buffer);
		return ret;
	}
	
	/* Prepare file header */
	memset(&file_header, 0, sizeof(file_header));
	file_header.magic = AI_PERSIST_MAGIC;
	file_header.version = AI_PERSIST_VERSION;
	file_header.entry_count = persist_ctx.entry_counts[type];
	file_header.creation_time = ktime_get_ns();
	file_header.last_update = file_header.creation_time;
	file_header.total_size = sizeof(file_header) + data_size;
	file_header.checksum = crc32(0, &file_header, sizeof(file_header) - sizeof(u32));
	
	/* Compress if enabled */
	if (persist_ctx.compression_enabled) {
		size_t compressed_size;
		
		ret = ai_compress_data(save_buffer, data_size, persist_ctx.compress_buffer,
				       persist_ctx.buffer_size, &compressed_size);
		if (ret == 0) {
			final_buffer = persist_ctx.compress_buffer;
			final_size = compressed_size;
			file_header.flags |= 0x1; /* Compression flag */
			file_header.total_size = sizeof(file_header) + compressed_size;
		} else {
			ai_warn("Compression failed, saving uncompressed");
			final_buffer = save_buffer;
			final_size = data_size;
		}
	} else {
		final_buffer = save_buffer;
		final_size = data_size;
	}
	
	/* Create filename */
	snprintf(filename, sizeof(filename), "%s/%s.dat", 
		 persist_ctx.base_path, persist_type_names[type]);
	
	/* Write header + data */
	mutex_lock(&persist_ctx.file_lock);
	
	/* Write header first */
	ret = ai_write_file_data(filename, &file_header, sizeof(file_header));
	if (ret == 0) {
		/* Append data */
		struct file *file = filp_open(filename, O_WRONLY | O_APPEND, 0644);
		if (!IS_ERR(file)) {
			loff_t pos = sizeof(file_header);
			ssize_t written = kernel_write(file, final_buffer, final_size, &pos);
			if (written != final_size) {
				ret = -EIO;
			}
			filp_close(file, NULL);
		} else {
			ret = PTR_ERR(file);
		}
	}
	
	mutex_unlock(&persist_ctx.file_lock);
	
	vfree(save_buffer);
	
	if (ret == 0) {
		atomic64_inc(&persist_ctx.entries_saved);
		ai_info("Saved %u entries of type %s (%zu bytes)", 
			persist_ctx.entry_counts[type], persist_type_names[type], final_size);
	}
	
	return ret;
}

/**
 * ai_save_all_data - Save all persistence data
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_save_all_data(void)
{
	int i, ret, success_count = 0, total_count = 0;
	
	if (!persist_ctx.persistence_enabled || !persist_ctx.dirty)
		return 0;
	
	mutex_lock(&persist_ctx.buffer_lock);
	
	for (i = 0; i < AI_PERSIST_TYPE_MAX; i++) {
		if (persist_ctx.entry_counts[i] > 0) {
			total_count++;
			ret = ai_save_data_type(i);
			if (ret == 0) {
				success_count++;
			} else {
				ai_error("Failed to save data type %s: %d", 
					 persist_type_names[i], ret);
			}
		}
	}
	
	mutex_unlock(&persist_ctx.buffer_lock);
	
	if (total_count > 0) {
		persist_ctx.save_success_rate = (success_count * 100) / total_count;
		persist_ctx.last_save_time = ktime_get_ns();
		persist_ctx.dirty = false;
		atomic64_inc(&persist_ctx.save_operations);
		
		ai_info("Saved %d/%d data types successfully (%u%% success rate)",
			success_count, total_count, persist_ctx.save_success_rate);
	}
	
	return (success_count == total_count) ? 0 : -EIO;
}

/**
 * ai_persistence_save_worker - Background save worker
 * @work: Work structure
 */
static void ai_persistence_save_worker(struct work_struct *work)
{
	if (!persist_ctx.auto_save_enabled || !persist_ctx.dirty)
		return;
	
	ai_save_all_data();
	
	/* Schedule next save */
	if (persist_ctx.auto_save_enabled) {
		queue_delayed_work(persist_ctx.save_wq, &persist_ctx.save_work,
				   msecs_to_jiffies(persist_ctx.save_interval_ms));
	}
}

/**
 * ai_persist_task_features - Persist task features
 * @task: Task to persist
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_persist_task_features(struct task_struct *task)
{
	struct ai_task_data *task_data;
	int ret;
	
	if (!task)
		return -EINVAL;
	
	task_data = ai_get_task_data(task);
	if (!task_data)
		return -ENOENT;
	
	ret = ai_add_persist_entry(AI_PERSIST_TASK_FEATURES, 
				   &task_data->current_features,
				   sizeof(task_data->current_features));
	
	ai_put_task_data(task_data);
	
	return ret;
}

/**
 * ai_persist_neural_weights - Persist neural network weights
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_persist_neural_weights(void)
{
	if (!ai_sched_ctx.learning_enabled)
		return -EINVAL;
	
	return ai_add_persist_entry(AI_PERSIST_NEURAL_WEIGHTS,
				    ai_sched_ctx.neural_weights,
				    sizeof(ai_sched_ctx.neural_weights));
}

/**
 * ai_data_persistence_init - Initialize data persistence system
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_data_persistence_init(void)
{
	int i;
	
	/* Initialize lists and locks */
	for (i = 0; i < AI_PERSIST_TYPE_MAX; i++) {
		INIT_LIST_HEAD(&persist_ctx.entries[i]);
		spin_lock_init(&persist_ctx.entries_lock[i]);
	}
	
	mutex_init(&persist_ctx.buffer_lock);
	mutex_init(&persist_ctx.file_lock);
	
	/* Allocate buffers */
	persist_ctx.buffer_size = AI_PERSIST_BUFFER_SIZE;
	persist_ctx.write_buffer = vmalloc(persist_ctx.buffer_size);
	persist_ctx.read_buffer = vmalloc(persist_ctx.buffer_size);
	persist_ctx.compress_buffer = vmalloc(persist_ctx.buffer_size);
	
	if (!persist_ctx.write_buffer || !persist_ctx.read_buffer || 
	    !persist_ctx.compress_buffer) {
		ai_error("Failed to allocate persistence buffers");
		return -ENOMEM;
	}
	
	/* Set base path */
	strncpy(persist_ctx.base_path, AI_PERSIST_BASE_PATH, sizeof(persist_ctx.base_path) - 1);
	persist_ctx.base_path[sizeof(persist_ctx.base_path) - 1] = '\0';
	
	/* Initialize compression */
	if (ai_init_compression() == 0) {
		persist_ctx.compression_enabled = true;
	} else {
		ai_warn("Compression initialization failed, disabling compression");
		persist_ctx.compression_enabled = false;
	}
	
	/* Create work queue */
	persist_ctx.save_wq = create_singlethread_workqueue("ai_persistence");
	if (!persist_ctx.save_wq) {
		ai_error("Failed to create persistence work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&persist_ctx.save_work, ai_persistence_save_worker);
	
	/* Configuration */
	persist_ctx.persistence_enabled = true;
	persist_ctx.auto_save_enabled = true;
	persist_ctx.save_interval_ms = AI_PERSIST_SAVE_INTERVAL_MS;
	persist_ctx.dirty = false;
	
	/* Initialize statistics */
	atomic64_set(&persist_ctx.entries_saved, 0);
	atomic64_set(&persist_ctx.entries_loaded, 0);
	atomic64_set(&persist_ctx.bytes_written, 0);
	atomic64_set(&persist_ctx.bytes_read, 0);
	atomic64_set(&persist_ctx.save_operations, 0);
	atomic64_set(&persist_ctx.load_operations, 0);
	
	/* Start auto-save */
	queue_delayed_work(persist_ctx.save_wq, &persist_ctx.save_work,
			   msecs_to_jiffies(persist_ctx.save_interval_ms));
	
	ai_info("Data persistence system initialized (compression: %s)",
		persist_ctx.compression_enabled ? "enabled" : "disabled");
	
	return 0;
}

/**
 * ai_data_persistence_exit - Cleanup data persistence system
 */
void ai_data_persistence_exit(void)
{
	struct ai_persist_entry *entry, *tmp;
	int i;
	
	persist_ctx.persistence_enabled = false;
	persist_ctx.auto_save_enabled = false;
	
	/* Stop work queue */
	if (persist_ctx.save_wq) {
		cancel_delayed_work_sync(&persist_ctx.save_work);
		destroy_workqueue(persist_ctx.save_wq);
		persist_ctx.save_wq = NULL;
	}
	
	/* Save any remaining data */
	if (persist_ctx.dirty) {
		ai_save_all_data();
	}
	
	/* Free all entries */
	for (i = 0; i < AI_PERSIST_TYPE_MAX; i++) {
		spin_lock(&persist_ctx.entries_lock[i]);
		list_for_each_entry_safe(entry, tmp, &persist_ctx.entries[i], list) {
			list_del(&entry->list);
			ai_free_persist_entry(entry);
		}
		persist_ctx.entry_counts[i] = 0;
		spin_unlock(&persist_ctx.entries_lock[i]);
	}
	
	/* Free buffers */
	if (persist_ctx.write_buffer) {
		vfree(persist_ctx.write_buffer);
		persist_ctx.write_buffer = NULL;
	}
	
	if (persist_ctx.read_buffer) {
		vfree(persist_ctx.read_buffer);
		persist_ctx.read_buffer = NULL;
	}
	
	if (persist_ctx.compress_buffer) {
		vfree(persist_ctx.compress_buffer);
		persist_ctx.compress_buffer = NULL;
	}
	
	/* Cleanup compression */
	ai_cleanup_compression();
	
	ai_info("Data persistence system cleaned up");
}

/**
 * ai_data_persistence_get_statistics - Get persistence statistics
 */
void ai_data_persistence_get_statistics(u64 *saved, u64 *loaded, u64 *written,
					u64 *read, u32 *compression_ratio,
					u32 *success_rate)
{
	if (saved)
		*saved = atomic64_read(&persist_ctx.entries_saved);
	
	if (loaded)
		*loaded = atomic64_read(&persist_ctx.entries_loaded);
	
	if (written)
		*written = atomic64_read(&persist_ctx.bytes_written);
	
	if (read)
		*read = atomic64_read(&persist_ctx.bytes_read);
	
	if (compression_ratio)
		*compression_ratio = persist_ctx.compression_ratio;
	
	if (success_rate)
		*success_rate = persist_ctx.save_success_rate;
}

/* Export symbols */
EXPORT_SYMBOL(ai_add_persist_entry);
EXPORT_SYMBOL(ai_persist_task_features);
EXPORT_SYMBOL(ai_persist_neural_weights);
EXPORT_SYMBOL(ai_data_persistence_get_statistics);