/****************************************************************************
 *
 *   Copyright (c) 2021 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "SafeDetector.hpp"

SafeDetector::SafeDetector() :
	ModuleParams(nullptr),
	ScheduledWorkItem(MODULE_NAME, px4::wq_configurations::test1)
{
}

SafeDetector::~SafeDetector()
{
	perf_free(_loop_perf);
	perf_free(_loop_interval_perf);
}

bool SafeDetector::init()
{
	// execute Run() on every sensor_accel publication
	// if (!_sensor_accel_sub.registerCallback()) {
		// PX4_ERR("sensor_accel callback registration failed");
		// return false;
	// }

	// alternatively, Run on fixed interval
	ScheduleOnInterval(1000000_us); // 2000 us interval, 200 Hz rate

	return true;
}

void SafeDetector::Run()
{
	if (should_exit()) {
		ScheduleClear();
		exit_and_cleanup();
		return;
	}

	// reschedule backup

	perf_begin(_loop_perf);
	perf_count(_loop_interval_perf);

	_safedetector.timestamp = hrt_absolute_time();

	// Check if parameters have changed
	if (_parameter_update_sub.updated()) {
		// clear update
		parameter_update_s param_update;
		_parameter_update_sub.copy(&param_update);
		updateParams(); // update module parameters (in DEFINE_PARAMETERS)
	}

	//获取无人机状态（是否解锁）
	if (_vehicle_status_sub.updated()) {
		vehicle_status_s vehicle_status;

		if (_vehicle_status_sub.copy(&vehicle_status)) {

			const bool armed = (vehicle_status.arming_state == vehicle_status_s::ARMING_STATE_ARMED);

			_armed = armed;
		}
	}

	distance_sensor_s distance_sensor;

	//获取激光传感器数值
	for (unsigned i = 0; i < _distance_sensor_subs.size(); i++) {
		if (_distance_sensor_subs[i].copy(&distance_sensor)) {
			_down = distance_sensor.current_distance;
		}
	}

	_safedetector.flag = true;
	//发送警告信息
	if(_armed && (_down > 5.0f)){
		// PX4_INFO("高度过高！");
		_safedetector.flag = false;

	}

	// double data = rand()/(double)RAND_MAX;

	// //输出显示
	// // if(_down < 1.f )
	// if(_armed){
	// 	PX4_INFO("%f",data);
	// }


	_safe_detector_pub.publish(_safedetector);
	perf_end(_loop_perf);
}

int SafeDetector::task_spawn(int argc, char *argv[])
{
	SafeDetector *instance = new SafeDetector();

	if (instance) {
		_object.store(instance);
		_task_id = task_id_is_work_queue;

		if (instance->init()) {
			return PX4_OK;
		}

	} else {
		PX4_ERR("alloc failed");
	}

	delete instance;
	_object.store(nullptr);
	_task_id = -1;

	return PX4_ERROR;
}

int SafeDetector::print_status()
{
	perf_print_counter(_loop_perf);
	perf_print_counter(_loop_interval_perf);
	return 0;
}

int SafeDetector::custom_command(int argc, char *argv[])
{
	return print_usage("unknown command");
}

int SafeDetector::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Example of a simple module running out of a work queue.

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("safe_detector", "safe");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

extern "C" __EXPORT int safe_detector_main(int argc, char *argv[])
{
	return SafeDetector::main(argc, argv);
}
