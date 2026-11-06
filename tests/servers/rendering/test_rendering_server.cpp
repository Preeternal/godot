/**************************************************************************/
/*  test_rendering_server.cpp                                             */
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

#include "tests/test_macros.h"

TEST_FORCE_LINK(test_rendering_server)

#include "core/extension/gdextension.h"
#include "core/object/class_db.h"
#include "core/object/method_bind.h"

namespace TestRenderingServer {

#ifndef DISABLE_DEPRECATED
TEST_CASE("[RenderingServer] Boot image legacy filters remain bool arguments") {
	auto get_method_bind = reinterpret_cast<GDExtensionInterfaceClassdbGetMethodBind>(GDExtension::get_interface_function("classdb_get_method_bind"));
	REQUIRE(get_method_bind != nullptr);
	if (!get_method_bind) {
		return;
	}
	const StringName class_name = "RenderingServer";
	const struct {
		const char *name;
		uint64_t hash;
	} legacy_methods[] = {
		{ "set_boot_image", 3759744527 },
		{ "set_boot_image", 2244367877 },
		{ "set_boot_image_with_stretch", 1104470771 },
	};
	for (const auto &legacy : legacy_methods) {
		INFO(String(legacy.name), " hash ", legacy.hash);
		const StringName method_name = legacy.name;
		const MethodBind *method = static_cast<const MethodBind *>(get_method_bind(&class_name, &method_name, legacy.hash));
		REQUIRE(method != nullptr);
		if (!method) {
			continue;
		}
		REQUIRE_EQ(method->get_argument_count(), 4);
		CHECK_EQ(method->get_argument_type(3), Variant::BOOL);
		CHECK_EQ(method->get_default_argument(3).get_type(), Variant::BOOL);
		CHECK(bool(method->get_default_argument(3)));
	}
	for (const char *name : { "set_boot_image", "set_boot_image_with_stretch" }) {
		MethodBind *method = ClassDB::get_method("RenderingServer", name);
		REQUIRE(method != nullptr);
		if (!method) {
			continue;
		}
		REQUIRE_EQ(method->get_argument_count(), 5);
		CHECK_EQ(method->get_argument_type(3), Variant::INT);
		CHECK_EQ(method->get_argument_type(4), Variant::BOOL);
		CHECK_EQ(int64_t(method->get_default_argument(3)), 0);
		CHECK(bool(method->get_default_argument(4)));
	}
}
#endif // DISABLE_DEPRECATED

} // namespace TestRenderingServer
