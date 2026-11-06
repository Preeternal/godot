/**************************************************************************/
/*  api.cpp                                                               */
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

#include "api.h"

#include "core/object/class_db.h"
#include "drivers/apple/rendering_native_surface_apple.h"

#if !defined(MACOS_ENABLED) && !defined(IOS_ENABLED) && !defined(VISIONOS_ENABLED)
// Match the Android API stub: expose cross-platform metadata without native UI.
void RenderingNativeSurfaceApple::_bind_methods() {
	ClassDB::bind_static_method("RenderingNativeSurfaceApple", D_METHOD("create", "layer"), &RenderingNativeSurfaceApple::create_api);
	ClassDB::bind_method(D_METHOD("get_layer"), &RenderingNativeSurfaceApple::get_layer);
}

Ref<RenderingNativeSurfaceApple> RenderingNativeSurfaceApple::create_api(uint64_t p_layer) {
	return nullptr;
}

Ref<RenderingNativeSurfaceApple> RenderingNativeSurfaceApple::create(void *p_layer) {
	return nullptr;
}

uint64_t RenderingNativeSurfaceApple::get_layer() {
	return 0;
}

RenderingContextDriver *RenderingNativeSurfaceApple::create_rendering_context(const String &p_driver_name) {
	return nullptr;
}

GLManager *RenderingNativeSurfaceApple::create_gl_manager(const String &p_driver_name) {
	return nullptr;
}

void *RenderingNativeSurfaceApple::get_native_id() const {
	return nullptr;
}

RenderingNativeSurfaceApple::RenderingNativeSurfaceApple() {
}

RenderingNativeSurfaceApple::~RenderingNativeSurfaceApple() {
}
#endif

void register_core_macos_api() {
#if !defined(IOS_ENABLED) && !defined(VISIONOS_ENABLED)
	GDREGISTER_ABSTRACT_CLASS(RenderingNativeSurfaceApple);
#endif
}

void unregister_core_macos_api() {
}

void register_macos_api() {
}

void unregister_macos_api() {
}
