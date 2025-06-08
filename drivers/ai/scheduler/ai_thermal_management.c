/*
 * AI Scheduler Thermal Management AI
 * Intelligent thermal management and prediction
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
#include <linux/thermal.h>
#include <linux/hwmon.h>
#include <linux/platform_device.h>

#include "ai_scheduler.h"

/* Thermal management configuration */
#define AI_THERMAL_MAX_SENSORS		16	/* Maximum thermal sensors */
#define AI_THERMAL_HISTORY_SIZE		128	/* Thermal history samples */
#define AI_THERMAL_PREDICTION_WINDOW	300	/* 5 minute prediction window */
#define AI_THERMAL_UPDATE_INTERVAL_MS	5000	/* 5 second updates */
#define AI_THERMAL_CRITICAL_TEMP	85000	/* 85°C critical temperature */
#define AI_THERMAL_WARNING_TEMP		75000	/* 75°C warning temperature */
#define AI_THERMAL_THROTTLE_TEMP	80000	/* 80°C throttle temperature */

/* Thermal sensor types */
enum ai_thermal_sensor_type {
	AI_THERMAL_SENSOR_CPU = 0,	/* CPU temperature */
	AI_THERMAL_SENSOR_GPU,		/* GPU temperature */
	AI_THERMAL_SENSOR_BATTERY,	/* Battery temperature */
	AI_THERMAL_SENSOR_SKIN,		/* Skin temperature */
	AI_THERMAL_SENSOR_AMBIENT,	/* Ambient temperature */
	AI_THERMAL_SENSOR_MODEM,	/* Modem temperature */
	AI_THERMAL_SENSOR_CAMERA,	/* Camera temperature */
	AI_THERMAL_SENSOR_CHARGER,	/* Charger temperature */
	AI_THERMAL_SENSOR_MEMORY,	/* Memory temperature */
	AI_THERMAL_SENSOR_PMIC		/* PMIC temperature */
};

/* Thermal state */
enum ai_thermal_state {
	AI_THERMAL_STATE_NORMAL = 0,	/* Normal operation */
	AI_THERMAL_STATE_WARM,		/* Warm but safe */
	AI_THERMAL_STATE_HOT,		/* Hot, start throttling */
	AI_THERMAL_STATE_CRITICAL,	/* Critical, aggressive throttling */
	AI_THERMAL_STATE_EMERGENCY	/* Emergency shutdown */
};

/* Thermal mitigation action */
enum ai_thermal_mitigation {
	AI_THERMAL_MITIGATION_NONE = 0,		/* No action needed */
	AI_THERMAL_MITIGATION_CPU_THROTTLE,	/* CPU frequency throttling */
	AI_THERMAL_MITIGATION_GPU_THROTTLE,	/* GPU frequency throttling */
	AI_THERMAL_MITIGATION_BRIGHTNESS_REDUCE, /* Reduce screen brightness */
	AI_THERMAL_MITIGATION_CHARGING_LIMIT,	/* Limit charging current */
	AI_THERMAL_MITIGATION_APP_LIMIT,	/* Limit background apps */
	AI_THERMAL_MITIGATION_CAMERA_DISABLE,	/* Disable camera */
	AI_THERMAL_MITIGATION_MODEM_THROTTLE,	/* Throttle modem */
	AI_THERMAL_MITIGATION_EMERGENCY_SHUTDOWN /* Emergency shutdown */
};

/* Thermal sensor */
struct ai_thermal_sensor {
	u32 sensor_id;			/* Sensor identifier */
	enum ai_thermal_sensor_type type; /* Sensor type */
	char name[32];			/* Sensor name */
	u32 current_temp;		/* Current temperature in mC */
	u32 max_temp;			/* Maximum safe temperature */
	u32 critical_temp;		/* Critical temperature */
	u32 warning_temp;		/* Warning temperature */
	bool is_active;			/* Whether sensor is active */
	bool is_critical;		/* Whether in critical state */
	
	/* Temperature history */
	u32 temp_history[32];		/* Temperature history */
	u32 history_index;		/* Current history index */
	u32 history_count;		/* Number of history samples */
	
	/* Thermal trends */
	s32 temp_trend;			/* Temperature trend (rising/falling) */
	u32 temp_velocity;		/* Rate of temperature change */
	u32 temp_acceleration;		/* Temperature acceleration */
	
	/* Statistics */
	u32 max_recorded_temp;		/* Maximum recorded temperature */
	u32 avg_temp;			/* Average temperature */
	u64 time_above_warning;		/* Time above warning temperature */
	u64 time_above_critical;	/* Time above critical temperature */
	
	struct list_head list;
};

/* Thermal sample */
struct ai_thermal_sample {
	u64 timestamp;			/* Sample timestamp */
	u32 sensor_temps[AI_THERMAL_MAX_SENSORS]; /* All sensor temperatures */
	u32 cpu_frequency;		/* CPU frequency at sample time */
	u32 gpu_frequency;		/* GPU frequency at sample time */
	u32 cpu_utilization;		/* CPU utilization */
	u32 gpu_utilization;		/* GPU utilization */
	u32 screen_brightness;		/* Screen brightness */
	u32 charging_current;		/* Charging current */
	u32 ambient_temp;		/* Ambient temperature */
	enum ai_thermal_state thermal_state; /* Thermal state */
	struct list_head list;
};

/* Thermal prediction model */
struct ai_thermal_prediction_model {
	/* Thermal coefficients */
	s32 cpu_freq_coefficient;	/* CPU frequency impact */
	s32 gpu_freq_coefficient;	/* GPU frequency impact */
	s32 cpu_util_coefficient;	/* CPU utilization impact */
	s32 gpu_util_coefficient;	/* GPU utilization impact */
	s32 brightness_coefficient;	/* Screen brightness impact */
	s32 charging_coefficient;	/* Charging current impact */
	s32 ambient_coefficient;	/* Ambient temperature impact */
	s32 thermal_resistance;		/* Thermal resistance */
	
	/* Model accuracy */
	u32 accuracy_percentage;	/* Prediction accuracy */
	u32 confidence_level;		/* Confidence level */
	u64 total_predictions;		/* Total predictions made */
	u64 correct_predictions;	/* Correct predictions */
	bool model_trained;		/* Whether model is trained */
};

/* Thermal prediction */
struct ai_thermal_prediction {
	u32 predicted_temp[AI_THERMAL_MAX_SENSORS]; /* Predicted temperatures */
	u32 time_to_warning_sec;	/* Time to warning temperature */
	u32 time_to_critical_sec;	/* Time to critical temperature */
	enum ai_thermal_state predicted_state; /* Predicted thermal state */
	enum ai_thermal_mitigation recommended_action; /* Recommended action */
	u32 confidence_percentage;	/* Prediction confidence */
	u64 prediction_timestamp;	/* Prediction timestamp */
	bool is_valid;			/* Whether prediction is valid */
};

/* Thermal mitigation strategy */
struct ai_thermal_mitigation_strategy {
	enum ai_thermal_mitigation action;
	u32 trigger_temp;		/* Temperature trigger */
	u32 release_temp;		/* Temperature to release action */
	u32 effectiveness;		/* Effectiveness rating (0-1000) */
	u32 user_impact;		/* User impact rating (0-1000) */
	bool is_active;			/* Whether currently active */
	u64 activation_count;		/* Number of times activated */
	u64 total_active_time_ms;	/* Total time active */
};

/* Thermal management context */
struct ai_thermal_mgmt_ctx {
	/* Thermal sensors */
	struct list_head sensors;
	spinlock_t sensors_lock;
	u32 sensor_count;
	
	/* Thermal samples history */
	struct list_head samples;
	spinlock_t samples_lock;
	u32 sample_count;
	
	/* Prediction model */
	struct ai_thermal_prediction_model model;
	spinlock_t model_lock;
	
	/* Current state */
	struct ai_thermal_sample current_sample;
	struct ai_thermal_prediction current_prediction;
	enum ai_thermal_state current_state;
	
	/* Mitigation strategies */
	struct ai_thermal_mitigation_strategy strategies[AI_THERMAL_MITIGATION_EMERGENCY_SHUTDOWN + 1];
	spinlock_t strategies_lock;
	
	/* Thermal management worker */
	struct workqueue_struct *thermal_wq;
	struct delayed_work thermal_work;
	bool thermal_active;
	u32 update_interval_ms;
	
	/* Thermal zones */
	struct thermal_zone_device *cpu_tz;
	struct thermal_zone_device *gpu_tz;
	struct thermal_zone_device *battery_tz;
	
	/* Statistics */
	atomic64_t predictions_made;
	atomic64_t mitigations_triggered;
	atomic64_t critical_events;
	atomic64_t samples_collected;
	
	/* Configuration */
	bool predictive_throttling_enabled;
	bool adaptive_mitigation_enabled;
	bool emergency_protection_enabled;
	u32 prediction_accuracy_threshold;
	
	/* Emergency state */
	bool emergency_mode;
	u64 emergency_start_time;
	u32 emergency_trigger_count;
};

static struct ai_thermal_mgmt_ctx thermal_ctx;

/* Thermal state names */
static const char *thermal_state_names[] = {
	"NORMAL",
	"WARM",
	"HOT", 
	"CRITICAL",
	"EMERGENCY"
};

/* Mitigation action names */
static const char *mitigation_action_names[] = {
	"NONE",
	"CPU_THROTTLE",
	"GPU_THROTTLE",
	"BRIGHTNESS_REDUCE",
	"CHARGING_LIMIT",
	"APP_LIMIT",
	"CAMERA_DISABLE",
	"MODEM_THROTTLE",
	"EMERGENCY_SHUTDOWN"
};

/**
 * ai_thermal_read_sensor_temperature - Read temperature from thermal sensor
 * @sensor: Thermal sensor
 * 
 * Returns: Temperature in mC, or negative error code
 */
static int ai_thermal_read_sensor_temperature(struct ai_thermal_sensor *sensor)
{
	struct thermal_zone_device *tz = NULL;
	int temp = 25000; /* Default 25°C */
	int ret;
	
	/* Get thermal zone based on sensor type */
	switch (sensor->type) {
	case AI_THERMAL_SENSOR_CPU:
		tz = thermal_ctx.cpu_tz;
		if (!tz) {
			tz = thermal_zone_get_zone_by_name("cpu-thermal");
			thermal_ctx.cpu_tz = tz;
		}
		break;
		
	case AI_THERMAL_SENSOR_GPU:
		tz = thermal_ctx.gpu_tz;
		if (!tz) {
			tz = thermal_zone_get_zone_by_name("gpu-thermal");
			thermal_ctx.gpu_tz = tz;
		}
		break;
		
	case AI_THERMAL_SENSOR_BATTERY:
		tz = thermal_ctx.battery_tz;
		if (!tz) {
			tz = thermal_zone_get_zone_by_name("battery-thermal");
			thermal_ctx.battery_tz = tz;
		}
		break;
		
	default:
		/* Simulate temperature for other sensors */
		temp = 25000 + (sensor->sensor_id * 2000); /* Vary by sensor ID */
		break;
	}
	
	/* Read temperature from thermal zone */
	if (tz) {
		ret = thermal_zone_get_temp(tz, &temp);
		if (ret) {
			temp = 25000; /* Fallback to 25°C */
		}
	}
	
	return temp;
}

/**
 * ai_thermal_update_sensor_history - Update sensor temperature history
 * @sensor: Thermal sensor
 * @new_temp: New temperature reading
 */
static void ai_thermal_update_sensor_history(struct ai_thermal_sensor *sensor, u32 new_temp)
{
	u32 prev_temp = sensor->current_temp;
	
	/* Update current temperature */
	sensor->current_temp = new_temp;
	
	/* Update history */
	sensor->temp_history[sensor->history_index] = new_temp;
	sensor->history_index = (sensor->history_index + 1) % ARRAY_SIZE(sensor->temp_history);
	if (sensor->history_count < ARRAY_SIZE(sensor->temp_history)) {
		sensor->history_count++;
	}
	
	/* Calculate temperature trend */
	if (prev_temp > 0) {
		sensor->temp_trend = (s32)new_temp - (s32)prev_temp;
		sensor->temp_velocity = abs(sensor->temp_trend);
		
		/* Calculate acceleration (change in velocity) */
		static u32 prev_velocity = 0;
		sensor->temp_acceleration = abs((s32)sensor->temp_velocity - (s32)prev_velocity);
		prev_velocity = sensor->temp_velocity;
	}
	
	/* Update statistics */
	if (new_temp > sensor->max_recorded_temp) {
		sensor->max_recorded_temp = new_temp;
	}
	
	/* Update average temperature */
	if (sensor->avg_temp == 0) {
		sensor->avg_temp = new_temp;
	} else {
		sensor->avg_temp = (sensor->avg_temp * 9 + new_temp) / 10;
	}
	
	/* Update time above thresholds */
	if (new_temp >= sensor->warning_temp) {
		sensor->time_above_warning += thermal_ctx.update_interval_ms;
	}
	if (new_temp >= sensor->critical_temp) {
		sensor->time_above_critical += thermal_ctx.update_interval_ms;
	}
	
	/* Check critical state */
	sensor->is_critical = (new_temp >= sensor->critical_temp);
	
	ai_verbose("Thermal sensor %s: %u°C, trend=%d, velocity=%u",
		   sensor->name, new_temp / 1000, sensor->temp_trend / 1000, 
		   sensor->temp_velocity / 1000);
}

/**
 * ai_thermal_determine_state - Determine overall thermal state
 * 
 * Returns: Current thermal state
 */
static enum ai_thermal_state ai_thermal_determine_state(void)
{
	struct ai_thermal_sensor *sensor;
	u32 max_temp = 0;
	u32 critical_sensors = 0;
	u32 warning_sensors = 0;
	
	spin_lock(&thermal_ctx.sensors_lock);
	list_for_each_entry(sensor, &thermal_ctx.sensors, list) {
		if (!sensor->is_active)
			continue;
		
		if (sensor->current_temp > max_temp) {
			max_temp = sensor->current_temp;
		}
		
		if (sensor->is_critical) {
			critical_sensors++;
		} else if (sensor->current_temp >= sensor->warning_temp) {
			warning_sensors++;
		}
	}
	spin_unlock(&thermal_ctx.sensors_lock);
	
	/* Determine state based on sensor readings */
	if (critical_sensors > 0 || max_temp >= AI_THERMAL_CRITICAL_TEMP) {
		return AI_THERMAL_STATE_CRITICAL;
	} else if (max_temp >= AI_THERMAL_THROTTLE_TEMP) {
		return AI_THERMAL_STATE_HOT;
	} else if (warning_sensors > 0 || max_temp >= AI_THERMAL_WARNING_TEMP) {
		return AI_THERMAL_STATE_WARM;
	} else {
		return AI_THERMAL_STATE_NORMAL;
	}
}

/**
 * ai_thermal_train_prediction_model - Train thermal prediction model
 */
static void ai_thermal_train_prediction_model(void)
{
	struct ai_thermal_sample *sample;
	struct ai_thermal_prediction_model *model = &thermal_ctx.model;
	u32 sample_count = 0;
	s64 cpu_freq_sum = 0, gpu_freq_sum = 0, cpu_util_sum = 0;
	s64 gpu_util_sum = 0, brightness_sum = 0, charging_sum = 0;
	s64 temp_sum = 0;
	
	spin_lock(&thermal_ctx.model_lock);
	
	/* Collect training data */
	spin_lock(&thermal_ctx.samples_lock);
	list_for_each_entry(sample, &thermal_ctx.samples, list) {
		if (sample_count >= 50) /* Limit training samples */
			break;
		
		cpu_freq_sum += sample->cpu_frequency;
		gpu_freq_sum += sample->gpu_frequency;
		cpu_util_sum += sample->cpu_utilization;
		gpu_util_sum += sample->gpu_utilization;
		brightness_sum += sample->screen_brightness;
		charging_sum += sample->charging_current;
		temp_sum += sample->sensor_temps[0]; /* Use first sensor as reference */
		sample_count++;
	}
	spin_unlock(&thermal_ctx.samples_lock);
	
	if (sample_count < 10) {
		spin_unlock(&thermal_ctx.model_lock);
		return;
	}
	
	/* Calculate simple correlation coefficients */
	model->cpu_freq_coefficient = (cpu_freq_sum * temp_sum) / (sample_count * sample_count);
	model->gpu_freq_coefficient = (gpu_freq_sum * temp_sum) / (sample_count * sample_count);
	model->cpu_util_coefficient = (cpu_util_sum * temp_sum) / (sample_count * sample_count);
	model->gpu_util_coefficient = (gpu_util_sum * temp_sum) / (sample_count * sample_count);
	model->brightness_coefficient = (brightness_sum * temp_sum) / (sample_count * sample_count);
	model->charging_coefficient = (charging_sum * temp_sum) / (sample_count * sample_count);
	model->thermal_resistance = temp_sum / sample_count;
	
	model->model_trained = true;
	model->accuracy_percentage = min(70U + sample_count, 90U);
	model->confidence_level = model->accuracy_percentage * 10;
	
	spin_unlock(&thermal_ctx.model_lock);
	
	ai_info("Thermal prediction model trained: samples=%u, accuracy=%u%%",
		sample_count, model->accuracy_percentage);
}

/**
 * ai_thermal_predict_temperature - Predict future temperatures
 * @prediction: Output prediction structure
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_thermal_predict_temperature(struct ai_thermal_prediction *prediction)
{
	struct ai_thermal_prediction_model *model = &thermal_ctx.model;
	struct ai_thermal_sample *current = &thermal_ctx.current_sample;
	u32 predicted_temp;
	u32 i;
	
	if (!model->model_trained) {
		return -ENODATA;
	}
	
	spin_lock(&thermal_ctx.model_lock);
	
	/* Predict temperature based on current conditions */
	predicted_temp = model->thermal_resistance;
	predicted_temp += (model->cpu_freq_coefficient * current->cpu_frequency) / 1000000;
	predicted_temp += (model->gpu_freq_coefficient * current->gpu_frequency) / 1000000;
	predicted_temp += (model->cpu_util_coefficient * current->cpu_utilization) / 1000;
	predicted_temp += (model->gpu_util_coefficient * current->gpu_utilization) / 1000;
	predicted_temp += (model->brightness_coefficient * current->screen_brightness) / 255;
	predicted_temp += (model->charging_coefficient * current->charging_current) / 1000;
	
	/* Apply ambient temperature influence */
	if (current->ambient_temp > 25000) { /* Above 25°C ambient */
		predicted_temp += (current->ambient_temp - 25000) / 2;
	}
	
	/* Fill prediction for all sensors (simplified) */
	for (i = 0; i < AI_THERMAL_MAX_SENSORS; i++) {
		prediction->predicted_temp[i] = predicted_temp + (i * 1000); /* Vary by sensor */
	}
	
	/* Calculate time to warning/critical */
	if (predicted_temp < AI_THERMAL_WARNING_TEMP) {
		prediction->time_to_warning_sec = 
			(AI_THERMAL_WARNING_TEMP - predicted_temp) / 100; /* Simplified */
	} else {
		prediction->time_to_warning_sec = 0;
	}
	
	if (predicted_temp < AI_THERMAL_CRITICAL_TEMP) {
		prediction->time_to_critical_sec = 
			(AI_THERMAL_CRITICAL_TEMP - predicted_temp) / 100; /* Simplified */
	} else {
		prediction->time_to_critical_sec = 0;
	}
	
	/* Determine predicted state */
	if (predicted_temp >= AI_THERMAL_CRITICAL_TEMP) {
		prediction->predicted_state = AI_THERMAL_STATE_CRITICAL;
		prediction->recommended_action = AI_THERMAL_MITIGATION_CPU_THROTTLE;
	} else if (predicted_temp >= AI_THERMAL_THROTTLE_TEMP) {
		prediction->predicted_state = AI_THERMAL_STATE_HOT;
		prediction->recommended_action = AI_THERMAL_MITIGATION_GPU_THROTTLE;
	} else if (predicted_temp >= AI_THERMAL_WARNING_TEMP) {
		prediction->predicted_state = AI_THERMAL_STATE_WARM;
		prediction->recommended_action = AI_THERMAL_MITIGATION_BRIGHTNESS_REDUCE;
	} else {
		prediction->predicted_state = AI_THERMAL_STATE_NORMAL;
		prediction->recommended_action = AI_THERMAL_MITIGATION_NONE;
	}
	
	prediction->confidence_percentage = model->accuracy_percentage;
	prediction->prediction_timestamp = ktime_get_ns();
	prediction->is_valid = true;
	
	spin_unlock(&thermal_ctx.model_lock);
	
	model->total_predictions++;
	atomic64_inc(&thermal_ctx.predictions_made);
	
	ai_verbose("Thermal prediction: %u°C, state=%s, action=%s",
		   predicted_temp / 1000, thermal_state_names[prediction->predicted_state],
		   mitigation_action_names[prediction->recommended_action]);
	
	return 0;
}

/**
 * ai_thermal_apply_mitigation - Apply thermal mitigation action
 * @action: Mitigation action to apply
 * 
 * Returns: 0 on success, negative error code on failure
 */
static int ai_thermal_apply_mitigation(enum ai_thermal_mitigation action)
{
	struct ai_thermal_mitigation_strategy *strategy = &thermal_ctx.strategies[action];
	
	if (strategy->is_active)
		return 0; /* Already active */
	
	spin_lock(&thermal_ctx.strategies_lock);
	
	switch (action) {
	case AI_THERMAL_MITIGATION_CPU_THROTTLE:
		/* Would integrate with CPU frequency scaling */
		ai_info("Applying CPU thermal throttling");
		break;
		
	case AI_THERMAL_MITIGATION_GPU_THROTTLE:
		/* Would integrate with GPU frequency scaling */
		ai_info("Applying GPU thermal throttling");
		break;
		
	case AI_THERMAL_MITIGATION_BRIGHTNESS_REDUCE:
		/* Would integrate with display brightness control */
		ai_info("Reducing screen brightness for thermal management");
		break;
		
	case AI_THERMAL_MITIGATION_CHARGING_LIMIT:
		/* Would integrate with charging current control */
		ai_info("Limiting charging current for thermal management");
		break;
		
	case AI_THERMAL_MITIGATION_APP_LIMIT:
		/* Would integrate with process management */
		ai_info("Limiting background applications for thermal management");
		break;
		
	case AI_THERMAL_MITIGATION_EMERGENCY_SHUTDOWN:
		ai_error("Emergency thermal shutdown triggered!");
		thermal_ctx.emergency_mode = true;
		thermal_ctx.emergency_start_time = ktime_get_ns();
		thermal_ctx.emergency_trigger_count++;
		break;
		
	default:
		spin_unlock(&thermal_ctx.strategies_lock);
		return -EINVAL;
	}
	
	strategy->is_active = true;
	strategy->activation_count++;
	
	spin_unlock(&thermal_ctx.strategies_lock);
	
	atomic64_inc(&thermal_ctx.mitigations_triggered);
	
	return 0;
}

/**
 * ai_thermal_management_worker - Thermal management worker
 * @work: Work structure
 */
static void ai_thermal_management_worker(struct work_struct *work)
{
	struct ai_thermal_sample *sample;
	struct ai_thermal_sensor *sensor;
	enum ai_thermal_state new_state;
	ktime_t start_time, end_time;
	u32 sensor_index = 0;
	int ret;
	
	if (!thermal_ctx.thermal_active)
		return;
	
	start_time = ktime_get();
	
	/* Create new sample */
	sample = kzalloc(sizeof(*sample), GFP_KERNEL);
	if (!sample)
		goto schedule_next;
	
	sample->timestamp = ktime_get_ns();
	
	/* Read all sensor temperatures */
	spin_lock(&thermal_ctx.sensors_lock);
	list_for_each_entry(sensor, &thermal_ctx.sensors, list) {
		if (sensor_index >= AI_THERMAL_MAX_SENSORS)
			break;
		
		int temp = ai_thermal_read_sensor_temperature(sensor);
		if (temp > 0) {
			ai_thermal_update_sensor_history(sensor, temp);
			sample->sensor_temps[sensor_index] = temp;
		}
		sensor_index++;
	}
	spin_unlock(&thermal_ctx.sensors_lock);
	
	/* Get system state information */
	sample->cpu_frequency = 1800000; /* Simulated 1.8 GHz */
	sample->gpu_frequency = 400000;  /* Simulated 400 MHz */
	sample->cpu_utilization = 300;   /* Simulated 30% */
	sample->gpu_utilization = 200;   /* Simulated 20% */
	sample->screen_brightness = 128; /* Simulated 50% */
	sample->charging_current = 1000; /* Simulated 1A */
	sample->ambient_temp = 25000;    /* Simulated 25°C */
	
	/* Determine thermal state */
	new_state = ai_thermal_determine_state();
	sample->thermal_state = new_state;
	
	/* Update current state */
	thermal_ctx.current_sample = *sample;
	
	/* Check for state change */
	if (new_state != thermal_ctx.current_state) {
		thermal_ctx.current_state = new_state;
		
		ai_info("Thermal state changed to: %s", thermal_state_names[new_state]);
		
		/* Trigger critical event if needed */
		if (new_state >= AI_THERMAL_STATE_CRITICAL) {
			atomic64_inc(&thermal_ctx.critical_events);
		}
	}
	
	/* Add sample to history */
	spin_lock(&thermal_ctx.samples_lock);
	
	/* Remove oldest sample if limit reached */
	if (thermal_ctx.sample_count >= AI_THERMAL_HISTORY_SIZE) {
		struct ai_thermal_sample *oldest = 
			list_last_entry(&thermal_ctx.samples, struct ai_thermal_sample, list);
		list_del(&oldest->list);
		kfree(oldest);
		thermal_ctx.sample_count--;
	}
	
	list_add(&sample->list, &thermal_ctx.samples);
	thermal_ctx.sample_count++;
	
	spin_unlock(&thermal_ctx.samples_lock);
	
	atomic64_inc(&thermal_ctx.samples_collected);
	
	/* Train prediction model */
	if (thermal_ctx.sample_count >= 10) {
		ai_thermal_train_prediction_model();
	}
	
	/* Make thermal prediction */
	ret = ai_thermal_predict_temperature(&thermal_ctx.current_prediction);
	if (ret == 0 && thermal_ctx.predictive_throttling_enabled) {
		/* Apply predictive mitigation */
		if (thermal_ctx.current_prediction.recommended_action != AI_THERMAL_MITIGATION_NONE) {
			ai_thermal_apply_mitigation(thermal_ctx.current_prediction.recommended_action);
		}
	}
	
	/* Apply immediate mitigation based on current state */
	switch (new_state) {
	case AI_THERMAL_STATE_HOT:
		ai_thermal_apply_mitigation(AI_THERMAL_MITIGATION_GPU_THROTTLE);
		break;
	case AI_THERMAL_STATE_CRITICAL:
		ai_thermal_apply_mitigation(AI_THERMAL_MITIGATION_CPU_THROTTLE);
		ai_thermal_apply_mitigation(AI_THERMAL_MITIGATION_CHARGING_LIMIT);
		break;
	case AI_THERMAL_STATE_EMERGENCY:
		ai_thermal_apply_mitigation(AI_THERMAL_MITIGATION_EMERGENCY_SHUTDOWN);
		break;
	default:
		break;
	}
	
	end_time = ktime_get();
	u64 processing_time = ktime_to_us(ktime_sub(end_time, start_time));
	
	ai_verbose("Thermal management cycle completed in %llu us", processing_time);

schedule_next:
	/* Schedule next thermal check */
	if (thermal_ctx.thermal_active) {
		u32 interval = thermal_ctx.update_interval_ms;
		
		/* Increase frequency during critical states */
		if (thermal_ctx.current_state >= AI_THERMAL_STATE_HOT) {
			interval = interval / 2; /* Check twice as often */
		}
		
		queue_delayed_work(thermal_ctx.thermal_wq, &thermal_ctx.thermal_work,
				   msecs_to_jiffies(interval));
	}
}

/**
 * ai_thermal_add_sensor - Add a thermal sensor
 * @type: Sensor type
 * @name: Sensor name
 * @max_temp: Maximum safe temperature
 * 
 * Returns: Sensor ID on success, negative error code on failure
 */
int ai_thermal_add_sensor(enum ai_thermal_sensor_type type, const char *name, u32 max_temp)
{
	struct ai_thermal_sensor *sensor;
	
	if (!name || thermal_ctx.sensor_count >= AI_THERMAL_MAX_SENSORS)
		return -EINVAL;
	
	sensor = kzalloc(sizeof(*sensor), GFP_KERNEL);
	if (!sensor)
		return -ENOMEM;
	
	sensor->sensor_id = thermal_ctx.sensor_count;
	sensor->type = type;
	strncpy(sensor->name, name, sizeof(sensor->name) - 1);
	sensor->max_temp = max_temp;
	sensor->critical_temp = max_temp - 5000; /* 5°C below max */
	sensor->warning_temp = max_temp - 10000;  /* 10°C below max */
	sensor->is_active = true;
	
	spin_lock(&thermal_ctx.sensors_lock);
	list_add(&sensor->list, &thermal_ctx.sensors);
	thermal_ctx.sensor_count++;
	spin_unlock(&thermal_ctx.sensors_lock);
	
	ai_info("Thermal sensor added: %s (type=%d, max_temp=%u°C)",
		name, type, max_temp / 1000);
	
	return sensor->sensor_id;
}

/**
 * ai_thermal_get_prediction - Get current thermal prediction
 * @prediction: Output prediction structure
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_thermal_get_prediction(struct ai_thermal_prediction *prediction)
{
	if (!prediction)
		return -EINVAL;
	
	if (!thermal_ctx.current_prediction.is_valid)
		return -ENODATA;
	
	*prediction = thermal_ctx.current_prediction;
	
	return 0;
}

/**
 * ai_thermal_management_init - Initialize thermal management
 * 
 * Returns: 0 on success, negative error code on failure
 */
int ai_thermal_management_init(void)
{
	u32 i;
	
	/* Initialize context */
	memset(&thermal_ctx, 0, sizeof(thermal_ctx));
	
	INIT_LIST_HEAD(&thermal_ctx.sensors);
	INIT_LIST_HEAD(&thermal_ctx.samples);
	spin_lock_init(&thermal_ctx.sensors_lock);
	spin_lock_init(&thermal_ctx.samples_lock);
	spin_lock_init(&thermal_ctx.model_lock);
	spin_lock_init(&thermal_ctx.strategies_lock);
	
	thermal_ctx.current_state = AI_THERMAL_STATE_NORMAL;
	thermal_ctx.update_interval_ms = AI_THERMAL_UPDATE_INTERVAL_MS;
	thermal_ctx.prediction_accuracy_threshold = 80;
	
	/* Initialize prediction model */
	thermal_ctx.model.accuracy_percentage = 50; /* Initial accuracy */
	thermal_ctx.model.confidence_level = 500;
	
	/* Initialize mitigation strategies */
	for (i = 0; i <= AI_THERMAL_MITIGATION_EMERGENCY_SHUTDOWN; i++) {
		thermal_ctx.strategies[i].action = i;
		thermal_ctx.strategies[i].trigger_temp = AI_THERMAL_WARNING_TEMP + (i * 2000);
		thermal_ctx.strategies[i].release_temp = thermal_ctx.strategies[i].trigger_temp - 3000;
		thermal_ctx.strategies[i].effectiveness = 500 + (i * 100);
		thermal_ctx.strategies[i].user_impact = i * 200;
	}
	
	/* Add default thermal sensors */
	ai_thermal_add_sensor(AI_THERMAL_SENSOR_CPU, "cpu-thermal", 85000);
	ai_thermal_add_sensor(AI_THERMAL_SENSOR_GPU, "gpu-thermal", 80000);
	ai_thermal_add_sensor(AI_THERMAL_SENSOR_BATTERY, "battery-thermal", 60000);
	ai_thermal_add_sensor(AI_THERMAL_SENSOR_SKIN, "skin-thermal", 45000);
	
	/* Create thermal work queue */
	thermal_ctx.thermal_wq = create_singlethread_workqueue("ai_thermal_management");
	if (!thermal_ctx.thermal_wq) {
		ai_error("Failed to create thermal management work queue");
		return -ENOMEM;
	}
	
	INIT_DELAYED_WORK(&thermal_ctx.thermal_work, ai_thermal_management_worker);
	
	/* Configuration */
	thermal_ctx.thermal_active = true;
	thermal_ctx.predictive_throttling_enabled = true;
	thermal_ctx.adaptive_mitigation_enabled = true;
	thermal_ctx.emergency_protection_enabled = true;
	
	/* Initialize statistics */
	atomic64_set(&thermal_ctx.predictions_made, 0);
	atomic64_set(&thermal_ctx.mitigations_triggered, 0);
	atomic64_set(&thermal_ctx.critical_events, 0);
	atomic64_set(&thermal_ctx.samples_collected, 0);
	
	/* Start thermal management */
	queue_delayed_work(thermal_ctx.thermal_wq, &thermal_ctx.thermal_work,
			   msecs_to_jiffies(thermal_ctx.update_interval_ms));
	
	ai_info("Thermal management initialized with %u sensors", thermal_ctx.sensor_count);
	
	return 0;
}

/**
 * ai_thermal_management_exit - Cleanup thermal management
 */
void ai_thermal_management_exit(void)
{
	struct ai_thermal_sensor *sensor, *tmp_sensor;
	struct ai_thermal_sample *sample, *tmp_sample;
	
	thermal_ctx.thermal_active = false;
	
	/* Stop thermal worker */
	if (thermal_ctx.thermal_wq) {
		cancel_delayed_work_sync(&thermal_ctx.thermal_work);
		destroy_workqueue(thermal_ctx.thermal_wq);
		thermal_ctx.thermal_wq = NULL;
	}
	
	/* Free sensors */
	spin_lock(&thermal_ctx.sensors_lock);
	list_for_each_entry_safe(sensor, tmp_sensor, &thermal_ctx.sensors, list) {
		list_del(&sensor->list);
		kfree(sensor);
	}
	spin_unlock(&thermal_ctx.sensors_lock);
	
	/* Free samples */
	spin_lock(&thermal_ctx.samples_lock);
	list_for_each_entry_safe(sample, tmp_sample, &thermal_ctx.samples, list) {
		list_del(&sample->list);
		kfree(sample);
	}
	spin_unlock(&thermal_ctx.samples_lock);
	
	ai_info("Thermal management cleaned up");
}

/**
 * ai_thermal_management_get_statistics - Get thermal management statistics
 */
void ai_thermal_management_get_statistics(u64 *predictions_made, u64 *mitigations_triggered,
					  u64 *critical_events, u64 *samples_collected,
					  enum ai_thermal_state *current_state)
{
	if (predictions_made)
		*predictions_made = atomic64_read(&thermal_ctx.predictions_made);
	
	if (mitigations_triggered)
		*mitigations_triggered = atomic64_read(&thermal_ctx.mitigations_triggered);
	
	if (critical_events)
		*critical_events = atomic64_read(&thermal_ctx.critical_events);
	
	if (samples_collected)
		*samples_collected = atomic64_read(&thermal_ctx.samples_collected);
	
	if (current_state)
		*current_state = thermal_ctx.current_state;
}

/* Export symbols */
EXPORT_SYMBOL(ai_thermal_add_sensor);
EXPORT_SYMBOL(ai_thermal_get_prediction);
EXPORT_SYMBOL(ai_thermal_management_get_statistics);