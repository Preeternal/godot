/**************************************************************************/
/*  lifecycle_host.cc                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

// Standalone macOS host for the public LibGodot C ABI, with Metal/Vulkan binds.
// Build separately from the engine. The .cc suffix excludes this executable
// from the engine's recursive .cpp unit-test discovery.
#include "core/extension/libgodot.h"

#include <dlfcn.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

static GDExtensionInterfaceGetProcAddress get_proc = nullptr;
static unsigned int log_calls = 0;
static unsigned int unexpected_errors = 0;
static bool expecting_failure = false;

static void check(bool p_condition, const char *p_message) {
	if (!p_condition) {
		fprintf(stderr, "FAIL: %s\n", p_message);
		exit(1);
	}
}

static void initialize_level(void *, GDExtensionInitializationLevel) {}
static void deinitialize_level(void *, GDExtensionInitializationLevel) {}

static GDExtensionBool initialize_host(GDExtensionInterfaceGetProcAddress p_get_proc, GDExtensionClassLibraryPtr, GDExtensionInitialization *r_init) {
	get_proc = p_get_proc;
	r_init->minimum_initialization_level = GDEXTENSION_INITIALIZATION_CORE;
	r_init->userdata = nullptr;
	r_init->initialize = initialize_level;
	r_init->deinitialize = deinitialize_level;
	return true;
}

static void log_message(LogCallbackData, const char *p_message, bool p_error) {
	++log_calls;
	if (p_error && strstr(p_message, "ERROR") && !(expecting_failure && strstr(p_message, "GDExtension initialization function"))) {
		++unexpected_errors;
	}
	fputs(p_message, p_error ? stderr : stdout);
}

static GDExtensionBool reject_host(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *) {
	return false;
}

static GDExtensionMethodBindPtr method(const char *p_name, GDExtensionInt p_hash, const char *p_class = "GodotInstance") {
	auto new_name = reinterpret_cast<GDExtensionInterfaceStringNameNewWithUtf8Chars>(get_proc("string_name_new_with_utf8_chars"));
	auto get_destructor = reinterpret_cast<GDExtensionInterfaceVariantGetPtrDestructor>(get_proc("variant_get_ptr_destructor"));
	auto get_method = reinterpret_cast<GDExtensionInterfaceClassdbGetMethodBind>(get_proc("classdb_get_method_bind"));
	check(new_name && get_destructor && get_method, "resolve public interfaces");
	alignas(void *) unsigned char class_name[sizeof(void *)] = {};
	alignas(void *) unsigned char method_name[sizeof(void *)] = {};
	new_name(class_name, p_class);
	new_name(method_name, p_name);
	GDExtensionMethodBindPtr result = get_method(class_name, method_name, p_hash);
	auto destroy_name = get_destructor(GDEXTENSION_VARIANT_TYPE_STRING_NAME);
	destroy_name(method_name);
	destroy_name(class_name);
	check(result, p_name);
	return result;
}

int main(int p_argc, char **p_argv) {
	check(p_argc == 3, "usage: libgodot_lifecycle <dylib> <project-directory>");
	void *library = dlopen(p_argv[1], RTLD_NOW | RTLD_LOCAL);
	if (!library) {
		fprintf(stderr, "%s\n", dlerror());
		return 1;
	}
	auto create = reinterpret_cast<decltype(&libgodot_create_godot_instance)>(dlsym(library, "libgodot_create_godot_instance"));
	auto destroy = reinterpret_cast<decltype(&libgodot_destroy_godot_instance)>(dlsym(library, "libgodot_destroy_godot_instance"));
	check(create && destroy, "resolve factory and destroy symbols");
	char executable[] = "g1-lifecycle";
	char headless[] = "--headless";
	char path[] = "--path";
	char *args[] = { executable, headless, path, p_argv[2] };
	destroy(nullptr);
	expecting_failure = true;
	check(!create(0, nullptr, initialize_host, log_message, nullptr), "reject invalid arguments");
	check(!create(4, args, nullptr, log_message, nullptr), "reject null initialization callback");
	check(!create(4, args, reject_host, log_message, nullptr), "extension initialization failure");
	expecting_failure = false;
	puts("G1_FAILURE_CLEANUP_OK");
	GDExtensionObjectPtr unstarted = create(4, args, initialize_host, log_message, nullptr);
	check(unstarted, "create without start");
	destroy(unstarted);
	get_proc = nullptr;
	puts("G1_UNSTARTED_CLEANUP_OK");
	for (int cycle = 0; cycle < 3; ++cycle) {
		GDExtensionObjectPtr instance = create(4, args, initialize_host, log_message, nullptr);
		check(instance, "create instance");
		auto ptrcall = reinterpret_cast<GDExtensionInterfaceObjectMethodBindPtrcall>(get_proc("object_method_bind_ptrcall"));
		check(ptrcall, "resolve ptrcall");
		auto start = method("start", 2240911060);
		auto iteration = method("iteration", 2240911060);
		auto is_started = method("is_started", 2240911060);
		auto stop = method("stop", 3218959716);
		GDExtensionBool result = false;
		ptrcall(iteration, instance, nullptr, &result);
		check(result, "iteration before start");
		ptrcall(start, instance, nullptr, &result);
		check(result, "start instance");
		method("create", 4233771684, "RenderingNativeSurfaceApple");
		method("create", 1056151817, "RenderingNativeSurfaceVulkan");
		method("set_native_surface", 2894024005, "Window");
		method("detach_native_surface", 3067735520, "DisplayServerEmbedded");
		method("set_boot_image", 3759744527, "RenderingServer");
		method("set_boot_image", 2244367877, "RenderingServer");
		method("set_boot_image_with_stretch", 1104470771, "RenderingServer");
		ptrcall(is_started, instance, nullptr, &result);
		check(result, "started state");
		ptrcall(iteration, instance, nullptr, &result);
		if (cycle < 2) {
			ptrcall(stop, instance, nullptr, nullptr);
			ptrcall(stop, instance, nullptr, nullptr);
			ptrcall(is_started, instance, nullptr, &result);
			check(!result, "stopped state");
		}
		destroy(instance);
		get_proc = nullptr;
		printf("G1_LIFECYCLE_CYCLE_%d_OK\n", cycle + 1);
	}
	char missing_driver[] = "--rendering-driver";
	char *invalid_args[] = { executable, missing_driver };
	check(!create(2, invalid_args, initialize_host, log_message, nullptr), "reject incomplete engine arguments");
	GDExtensionObjectPtr recovered = create(4, args, initialize_host, log_message, nullptr);
	check(recovered, "create after argument failure");
	destroy(recovered);
	puts("G1_ARGUMENT_FAILURE_CLEANUP_OK");
	destroy(nullptr);
	check(log_calls > 0, "log callback accepts null user data");
	check(unexpected_errors == 0, "no unexpected engine errors");
	puts("G1_LIFECYCLE_OK");
	// Keep the engine image loaded: unloading Objective-C classes is unsupported.
	return 0;
}
