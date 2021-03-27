/* MIT License
 *
 * Copyright (c) 2021 Parola Marco
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. */



#version 450
#extension GL_ARB_separate_shader_objects : enable



layout(push_constant) uniform ObjectPushConstant {
	mat4 world; // Model space to world space
	vec4 col;
	float rnd;
} objectPc;

layout(set = 0, binding = 0) uniform StaticUbo {
	mat4 proj;
	float outlineSize;
	float outlineDepth;
	uint lightLevels;
} staticUbo;

layout(set = 1, binding = 0) uniform ModelUbo {
	float minDiffuse;
	float maxDiffuse;
	float minSpecular;
	float maxSpecular;
	float rnd;
} modelUbo;

layout(set = 2, binding = 0) uniform FrameUbo {
	mat4 view;
	vec3 lightDirection;
	float rnd;
	uint shaderSelector;
} frameUbo;

layout(set = 1, binding = 1) uniform sampler2D tex_dfs_sampler;
layout(set = 1, binding = 2) uniform sampler2D tex_nrm_sampler;



layout(location = 0) in vec2 frg_tex;
layout(location = 1) in vec3 frg_eyedir_tan;
layout(location = 2) in vec3 frg_lightdir_tan;
layout(location = 3) in vec3 frg_nrm_tan;

layout(location = 0) out vec4 out_col;



float unnormalize(float low, float high, float normalized) {
	return (low * (1 - normalized)) + (high * normalized);
}


float shave(float value, float stride, float bias) {
	float valueMul = value / stride;
	float valueFloor = floor(valueMul + bias);
	return valueFloor * stride;
}


float rnd_f1(float seed) {
	return fract(sin(seed * 78.233) * 43758.5453);
}

vec2 rnd_f2(vec2 seed) {
	return vec2(rnd_f1(seed.x), rnd_f1(seed.y));
}

vec3 rnd_f3(vec3 seed) {
	return vec3(rnd_f1(seed.x), rnd_f1(seed.y), rnd_f1(seed.z));
}

vec3 get_normal_tanspace() {
	return (
		normalize(texture(tex_nrm_sampler, frg_tex).rgb)
		- 0.5
	) * 2.0;
}


float computeDiffusion(vec3 lightdirTan) {
	float r = unnormalize(
		modelUbo.minDiffuse, modelUbo.maxDiffuse,
			dot(-frg_nrm_tan, get_normal_tanspace() * lightdirTan));
	r = max(0, min(1, r));
	return r;
}


float computeSpecular(vec3 lightdirTan) {
	float r = unnormalize(
		modelUbo.minSpecular, modelUbo.maxSpecular,
		dot(frg_eyedir_tan, get_normal_tanspace() * lightdirTan));
	r = max(0, min(1, r));
	return r;
}



// Light diffusion
void main_0() {
	float diffusion = computeDiffusion(frg_lightdir_tan);
	diffusion *= unnormalize(modelUbo.minDiffuse, modelUbo.maxDiffuse, diffusion);

	out_col =
		texture(tex_dfs_sampler, frg_tex)
		* objectPc.col;

	out_col *= diffusion;
}


// Cel shading, diffuse lighting
void main_1() {
	float diffusion = computeDiffusion(frg_lightdir_tan);
	diffusion *= unnormalize(modelUbo.minDiffuse, modelUbo.maxDiffuse, diffusion);

	out_col =
		texture(tex_dfs_sampler, frg_tex)
		* objectPc.col;

	out_col.xyz *= shave(diffusion, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Specular light
void main_2() {
	float reflection = computeSpecular(frg_lightdir_tan);

	out_col =
		texture(tex_dfs_sampler, frg_tex)
		* objectPc.col;

	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Cel shading, specular light
void main_3() {
	float reflection = computeSpecular(frg_lightdir_tan);

	out_col =
		texture(tex_dfs_sampler, frg_tex)
		* objectPc.col;

	out_col.xyz *= shave(reflection, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Everything minus cel shading
void main_4() {
	float reflection = computeDiffusion(frg_lightdir_tan) + computeSpecular(frg_lightdir_tan);

	out_col = texture(tex_dfs_sampler, frg_tex) * objectPc.col;
	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Everything
void main_5() {
	float reflection = computeDiffusion(frg_lightdir_tan) + computeSpecular(frg_lightdir_tan);

	out_col = texture(tex_dfs_sampler, frg_tex) * objectPc.col;
	out_col.xyz *= shave(reflection, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}



void main() {
	switch(frameUbo.shaderSelector) {
		case 0:  main_0();  break;
		case 1:  main_1();  break;
		case 2:  main_2();  break;
		case 3:  main_3();  break;
		case 4:  main_4();  break;
		case 5:
		default:  main_5();  break;
	}
}
