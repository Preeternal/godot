/**************************************************************************/
/*  test_accessibility_server.cpp                                         */
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

// Tests for accessibility backend registry lifetime.
#include "servers/display/accessibility_server.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_accessibility_server)

TEST_CASE("[AccessibilityServer] Instance destruction preserves backend registration") {
	const int backend_count = AccessibilityServer::get_create_function_count();
	REQUIRE(backend_count >= 2); // test_main registers an extra backend, as AccessKit does.
	for (int cycle = 0; cycle < 2; ++cycle) {
		REQUIRE(String(AccessibilityServer::get_create_function_name(backend_count - 1)) == "dummy");
		Error error = ERR_UNAVAILABLE;
		AccessibilityServer *server = AccessibilityServer::create(backend_count - 1, error);
		REQUIRE(error == OK);
		REQUIRE(server != nullptr);
		CHECK(AccessibilityServer::get_singleton() == server);
		memdelete(server);
		CHECK(AccessibilityServer::get_singleton() == nullptr);
		REQUIRE(AccessibilityServer::get_create_function_count() == backend_count);
	}
}
