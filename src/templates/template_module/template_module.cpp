/****************************************************************************
 *
 *   Copyright (c) 2018 PX4 Development Team. All rights reserved.
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

#include "template_module.h"

#include <px4_platform_common/getopt.h>
#include <px4_platform_common/log.h>
#include <px4_platform_common/posix.h>

#include <uORB/topics/parameter_update.h>
#include <uORB/topics/sensor_combined.h>

ModuleBase::Descriptor TemplateModule::desc{task_spawn, custom_command, print_usage};

int TemplateModule::print_status()
{
	PX4_INFO("Running");
	// TODO: 打印模块运行时的更多状态信息，便于调试与观测
	// TODO: print additional runtime information about the state of the module

	return 0;
}

int TemplateModule::custom_command(int argc, char *argv[])
{
	// 这里可以扩展自定义 CLI 子命令，默认模板未启用具体功能
	/*
	if (!is_running(desc)) {
		print_usage("not running");
		return 1;
	}

	// additional custom commands can be handled like this:
	if (!strcmp(argv[0], "do-something")) {
		get_instance<TemplateModule>(desc)->do_something();
		return 0;
	}
	 */

	return print_usage("unknown command");
}


int TemplateModule::run_trampoline(int argc, char *argv[])
{
	return ModuleBase::run_trampoline_impl(desc, [](int ac, char *av[]) -> ModuleBase * {
		return TemplateModule::instantiate(ac, av);
	}, argc, argv);
}

int TemplateModule::task_spawn(int argc, char *argv[])
{
	// 启动独立任务线程，入口为 run_trampoline
	desc.task_id = px4_task_spawn_cmd("module",
					  SCHED_DEFAULT,
					  SCHED_PRIORITY_DEFAULT,
					  1024,
					  (px4_main_t)&run_trampoline,
					  (char *const *)argv);

	if (desc.task_id < 0) {
		desc.task_id = -1;
		return -errno;
	}

	return 0;
}

TemplateModule *TemplateModule::instantiate(int argc, char *argv[])
{
	// 示例参数与开关，展示如何从命令行传入配置
	int example_param = 0;
	bool example_flag = false;
	bool error_flag = false;

	int myoptind = 1;
	int ch;
	const char *myoptarg = nullptr;

	// 解析 CLI 参数
	// -p <value>：示例整型参数
	// -f：示例布尔开关
	while ((ch = px4_getopt(argc, argv, "p:f", &myoptind, &myoptarg)) != EOF) {
		switch (ch) {
		case 'p':
			example_param = (int)strtol(myoptarg, nullptr, 10);
			break;

		case 'f':
			example_flag = true;
			break;

		case '?':
			error_flag = true;
			break;

		default:
			PX4_WARN("unrecognized flag");
			error_flag = true;
			break;
		}
	}

	if (error_flag) {
		// 参数非法时返回空指针，框架会据此判定启动失败
		return nullptr;
	}

	TemplateModule *instance = new TemplateModule(example_param, example_flag);

	if (instance == nullptr) {
		// 内存分配失败时记录错误日志
		PX4_ERR("alloc failed");
	}

	return instance;
}

TemplateModule::TemplateModule(int example_param, bool example_flag)
	: ModuleParams(nullptr)
{
}

void TemplateModule::run()
{
	// 示例：通过订阅 sensor_combined，将主循环与传感器发布节奏同步
	// Example: run the loop synchronized to the sensor_combined topic publication
	int sensor_combined_sub = orb_subscribe(ORB_ID(sensor_combined));

	px4_pollfd_struct_t fds[1];
	fds[0].fd = sensor_combined_sub;
	fds[0].events = POLLIN;

	// 初始化参数，首次进入时强制拉取一次
	parameters_update(true);

	while (!should_exit()) {

		// 最多等待 1000ms 新数据到达
		int pret = px4_poll(fds, (sizeof(fds) / sizeof(fds[0])), 1000);

		if (pret == 0) {
			// 超时不退出循环，允许模块继续执行周期性逻辑
			// Timeout: let the loop run anyway, don't do `continue` here

		} else if (pret < 0) {
			// poll 出错时短暂休眠，避免错误状态下忙等
			// this is undesirable but not much we can do
			PX4_ERR("poll error %d, %d", pret, errno);
			px4_usleep(50000);
			continue;

		} else if (fds[0].revents & POLLIN) {

			struct sensor_combined_s sensor_combined;
			orb_copy(ORB_ID(sensor_combined), sensor_combined_sub, &sensor_combined);
			// 在此处理新到达的传感器数据
			// TODO: do something with the data...

		}

		// 周期性检查参数更新，支持运行时调参
		parameters_update();
	}

	orb_unsubscribe(sensor_combined_sub);
}

void TemplateModule::parameters_update(bool force)
{
	// 检查参数是否更新；force=true 时无条件更新一次
	// check for parameter updates
	if (_parameter_update_sub.updated() || force) {
		// 读取并清除参数更新通知
		// clear update
		parameter_update_s update;
		_parameter_update_sub.copy(&update);

		// 从参数系统同步到当前模块成员
		// update parameters from storage
		updateParams();
	}
}

int TemplateModule::print_usage(const char *reason)
{
	if (reason) {
		PX4_WARN("%s\n", reason);
	}

	PRINT_MODULE_DESCRIPTION(
		R"DESCR_STR(
### Description
Section that describes the provided module functionality.

This is a template for a module running as a task in the background with start/stop/status functionality.

### Implementation
Section describing the high-level implementation of this module.

### Examples
CLI usage example:
$ module start -f -p 42

)DESCR_STR");

	PRINT_MODULE_USAGE_NAME("module", "template");
	PRINT_MODULE_USAGE_COMMAND("start");
	PRINT_MODULE_USAGE_PARAM_FLAG('f', "Optional example flag", true);
	PRINT_MODULE_USAGE_PARAM_INT('p', 0, 0, 1000, "Optional example parameter", true);
	PRINT_MODULE_USAGE_DEFAULT_COMMANDS();

	return 0;
}

int template_module_main(int argc, char *argv[])
{
	return ModuleBase::main(TemplateModule::desc, argc, argv);
}
