/**************************************************************************/
/*  test_shader_language.cpp                                              */
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

TEST_FORCE_LINK(test_shader_language)

#include "servers/rendering/shader_language.h"
#include "servers/rendering/shader_types.h"

namespace TestShaderLanguage {

TEST_CASE("[ShaderLanguage] Argument copies initialize texture metadata") {
	using Argument = ShaderLanguage::FunctionNode::Argument;
	// Parser arguments may reuse nonzero stack storage. Copying them into the
	// argument vector must not read an uninitialized Hint or bool.
	alignas(Argument) uint8_t storage[sizeof(Argument)];
	memset(storage, 0xa5, sizeof(storage));
	Argument *argument = memnew_placement(storage, Argument);
	argument->qualifier = ShaderLanguage::ARGUMENT_QUALIFIER_IN;
	argument->type = ShaderLanguage::TYPE_FLOAT;
	argument->precision = ShaderLanguage::PRECISION_DEFAULT;
	argument->tex_argument_filter = ShaderLanguage::FILTER_DEFAULT;
	argument->tex_argument_repeat = ShaderLanguage::REPEAT_DEFAULT;
	Vector<Argument> arguments;
	arguments.push_back(*argument);
	argument->~Argument();

	CHECK_EQ(arguments[0].tex_hint, ShaderLanguage::ShaderNode::Uniform::HINT_NONE);
	CHECK_FALSE(arguments[0].tex_argument_check);
	CHECK_FALSE(arguments[0].tex_builtin_check);
	CHECK_FALSE(arguments[0].is_const);
	CHECK_EQ(arguments[0].array_size, 0);
}

TEST_CASE("[ShaderLanguage] Scalar and sampler function arguments compile") {
	ShaderLanguage language;
	ShaderLanguage::ShaderCompileInfo info;
	ShaderTypes *types = ShaderTypes::get_singleton();
	REQUIRE(types != nullptr);
	info.functions = types->get_functions(RSE::SHADER_SPATIAL);
	info.render_modes = types->get_modes(RSE::SHADER_SPATIAL);
	info.stencil_modes = types->get_stencil_modes(RSE::SHADER_SPATIAL);
	info.shader_types = types->get_types();

	const String code =
			"shader_type spatial;\n"
			"uniform sampler2D source : source_color, filter_linear, repeat_disable;\n"
			"float doubled(float value) { return value * 2.0; }\n"
			"vec4 read_color(sampler2D image, vec2 uv) { return texture(image, uv); }\n"
			"void fragment() { ALBEDO = read_color(source, UV).rgb * doubled(0.5); }\n";
	const Error result = language.compile(code, info);
	INFO(language.get_error_text());
	REQUIRE_EQ(result, OK);
	const auto *shader = language.get_shader();
	REQUIRE(shader != nullptr);
	bool found_scalar = false;
	bool found_sampler = false;
	for (const auto &function : shader->vfunctions) {
		if (function.name == "doubled") {
			found_scalar = true;
			REQUIRE_EQ(function.function->arguments.size(), 1);
			CHECK_EQ(function.function->arguments[0].tex_hint, ShaderLanguage::ShaderNode::Uniform::HINT_NONE);
		} else if (function.name == "read_color") {
			found_sampler = true;
			REQUIRE_EQ(function.function->arguments.size(), 2);
			// Source color is sanitized to HINT_NONE for function arguments.
			CHECK_EQ(function.function->arguments[0].tex_hint, ShaderLanguage::ShaderNode::Uniform::HINT_NONE);
			CHECK_EQ(function.function->arguments[0].tex_argument_filter, ShaderLanguage::FILTER_LINEAR);
			CHECK_EQ(function.function->arguments[0].tex_argument_repeat, ShaderLanguage::REPEAT_DISABLE);
			CHECK_EQ(function.function->arguments[1].tex_hint, ShaderLanguage::ShaderNode::Uniform::HINT_NONE);
		}
	}
	CHECK(found_scalar);
	CHECK(found_sampler);
}

} // namespace TestShaderLanguage
