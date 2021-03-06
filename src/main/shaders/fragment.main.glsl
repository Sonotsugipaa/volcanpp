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



layout(set = 0, binding = 0) uniform StaticUbo {
	mat4 proj;
	float outlineSize;
	float outlineDepth;
	float outlineRnd;
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

layout(set = 1, binding = 1) uniform sampler2D tex_dfsSampler;
layout(set = 1, binding = 2) uniform sampler2D tex_nrmSampler;



layout(location = 0) in vec2 frg_tex;
layout(location = 1) in vec3 frg_eyedirTan;
layout(location = 2) in vec3 frg_lightdirTan;
layout(location = 3) in vec3 frg_nrmTan;
layout(location = 4) in vec4 frg_col;

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
		normalize(texture(tex_nrmSampler, frg_tex).rgb)
		- 0.5
	) * 2.0;
}


float computeDiffusion(vec3 lightdirTan, vec3 normalTanspace) {
	float r = unnormalize(
		modelUbo.minDiffuse, modelUbo.maxDiffuse,
			max(0, dot(-frg_nrmTan, normalTanspace * lightdirTan)));
	r = max(0, r);
	return r;
}


float computeSpecular(vec3 lightdirTan, vec3 normalTanspace) {
	float r = unnormalize(
		modelUbo.minSpecular, modelUbo.maxSpecular,
		max(0, dot(frg_eyedirTan, normalTanspace * lightdirTan)));
	r = max(0, r);
	return r;
}



// Everything minus cel shading and normal maps
void main_0() {
	float diffuse = max(0, unnormalize(
		modelUbo.minDiffuse, modelUbo.maxDiffuse,
		max(0, dot(-frg_lightdirTan, frg_nrmTan))));
	out_col =
		texture(tex_dfsSampler, frg_tex) * frg_col
		* (diffuse + computeSpecular(frg_lightdirTan, vec3(0.5, 0.5, 1)));
}


// Light diffusion
void main_1() {
	float diffusion = computeDiffusion(frg_lightdirTan, get_normal_tanspace());

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col *= diffusion;
}


// Cel shading, diffuse lighting
void main_2() {
	float diffusion = computeDiffusion(frg_lightdirTan, get_normal_tanspace());

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= shave(diffusion, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Specular light
void main_3() {
	float reflection = computeSpecular(frg_lightdirTan, get_normal_tanspace());

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Cel shading, specular light
void main_4() {
	float reflection = computeSpecular(frg_lightdirTan, get_normal_tanspace());

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= shave(reflection, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Everything minus cel shading
void main_5() {
	float reflection =
		computeDiffusion(frg_lightdirTan, get_normal_tanspace())
		+ computeSpecular(frg_lightdirTan, get_normal_tanspace());

	out_col = texture(tex_dfsSampler, frg_tex) * frg_col;
	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Everything
void main_6() {
	float reflection =
		computeDiffusion(frg_lightdirTan, get_normal_tanspace())
		+ computeSpecular(frg_lightdirTan, get_normal_tanspace());

	out_col = texture(tex_dfsSampler, frg_tex) * frg_col;
	out_col.xyz *= shave(reflection, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}



/* -- Selector values --
 * 0: Diffuse and specular lighting without using normal maps
 * 1: Diffuse lighting
 * 2: Diffuse lighting with cel shading and outline
 * 3: Specular lighting
 * 4: Specular lighting with cel shading and outline
 * 5: Diffuse and specular lighting
 * 6: Diffuse and specular lighting with cel shading and outline */
void main() {
	switch(frameUbo.shaderSelector) {
		case 0:  main_0();  break;
		case 1:  main_1();  break;
		case 2:  main_2();  break;
		case 3:  main_3();  break;
		case 4:  main_4();  break;
		case 5:  main_5();  break;
		case 6:
		default:  main_6();  break;
	}
}
