/**************************************************************************/
/*  libgodot_android.cpp                                                  */
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

#include "libgodot_android.h"

#include "dir_access_jandroid.h"
#include "file_access_android.h"
#include "file_access_filesystem_jandroid.h"
#include "java_godot_io_wrapper.h"
#include "net_socket_android.h"
#include "os_android.h"
#include "thread_jandroid.h"

#include "core/extension/godot_instance.h"
#include "core/io/libgodot_logger.h"
#include "main/main.h"
#include "servers/movie_writer/movie_writer.h"

#include <jni.h>

static OS_Android *os = nullptr;

static GodotInstance *instance = nullptr;
static GodotIOJavaWrapper *io_wrapper = nullptr;

// Called only after Main has released every user of the Java services.
// Each terminate routine tolerates partial setup and clears its static state.
static void cleanup_android_session() {
	delete os;
	os = nullptr;
	delete io_wrapper;
	io_wrapper = nullptr;
	NetSocketAndroid::terminate();
	FileAccessFilesystemJAndroid::terminate();
	DirAccessJAndroid::terminate();
	FileAccessAndroid::terminate();
}

extern LIBGODOT_API GDExtensionObjectPtr libgodot_create_godot_instance_android(int p_argc, char *p_argv[], GDExtensionInitializationFunction p_init_func, LogCallbackFunction p_log_func, LogCallbackData p_log_data, JNIEnv *env, jobject p_asset_manager, jobject p_net_utils, jobject p_directory_access_handler, jobject p_file_access_handler, jobject p_godot_io_wrapper) {
	ERR_FAIL_COND_V_MSG(instance != nullptr, nullptr, "Only one Godot Instance may be created.");
	ERR_FAIL_COND_V(!env || p_argc < 1 || !p_argv || !p_argv[0] || !p_init_func, nullptr);
	ERR_FAIL_COND_V(!p_asset_manager || !p_net_utils || !p_directory_access_handler || !p_file_access_handler || !p_godot_io_wrapper, nullptr);

	JavaVM *jvm = nullptr;
	ERR_FAIL_COND_V(env->GetJavaVM(&jvm) != JNI_OK, nullptr);

	init_thread_jandroid(jvm, env);

	FileAccessAndroid::setup(p_asset_manager);
	if (env->ExceptionCheck()) {
		cleanup_android_session();
		return nullptr;
	}
	DirAccessJAndroid::setup(p_directory_access_handler);
	if (env->ExceptionCheck()) {
		cleanup_android_session();
		return nullptr;
	}
	FileAccessFilesystemJAndroid::setup(p_file_access_handler);
	if (env->ExceptionCheck()) {
		cleanup_android_session();
		return nullptr;
	}
	NetSocketAndroid::setup(p_net_utils);
	if (env->ExceptionCheck()) {
		cleanup_android_session();
		return nullptr;
	}
	io_wrapper = new GodotIOJavaWrapper(env, p_godot_io_wrapper);
	if (env->ExceptionCheck()) {
		cleanup_android_session();
		return nullptr;
	}

	os = new OS_Android(nullptr, io_wrapper, false);
	if (p_log_func != nullptr) {
		LibGodotLogger *logger = memnew(LibGodotLogger);
		logger->set_callback_function(p_log_func, p_log_data);
		os->add_logger(logger);
	}

	Error err = Main::setup(p_argv[0], p_argc - 1, &p_argv[1], false);
	if (err != OK) {
		cleanup_android_session();
		return nullptr;
	}

	instance = memnew(GodotInstance);
	if (!instance->initialize(p_init_func)) {
		memdelete(instance);
		instance = nullptr;
		Main::cleanup(true);
		cleanup_android_session();
		return nullptr;
	}

	return (GDExtensionObjectPtr)instance;
}

extern LIBGODOT_API void libgodot_destroy_godot_instance(GDExtensionObjectPtr p_godot_instance) {
	GodotInstance *godot_instance = (GodotInstance *)p_godot_instance;
	if (instance && instance == godot_instance) {
		godot_instance->stop();
		memdelete(godot_instance);
		instance = nullptr;
		Main::cleanup(true);
		cleanup_android_session();
	}
}
