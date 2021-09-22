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
	vec3 viewPos;
	vec4 pointLight;
	vec3 lightDirection;
	float rnd;
	uint shaderSelector;
} frameUbo;

layout(set = 1, binding = 1) uniform sampler2D tex_dfsSampler;
layout(set = 1, binding = 2) uniform sampler2D tex_nrmSampler;



layout(location = 0) in vec2 frg_tex;
layout(location = 1) in vec3 frg_lightDirTan;
layout(location = 2) in vec3 frg_nrmTan;
layout(location = 3) in vec4 frg_col;
layout(location = 4) in vec3 frg_worldPos;
layout(location = 5) in mat3 frg_tbn;
layout(location = 8) in mat3 frg_tbnInverse;

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
		texture(tex_nrmSampler, frg_tex).rgb
		- 0.5
	) * 2.0;
}


float rayDiffusion(vec3 lightDirTan, vec3 nrmTan) {
	float r = modelUbo.minDiffuse;
	if(-0.5 < dot(-lightDirTan, frg_nrmTan)) {
		r = dot(-lightDirTan, nrmTan);
		r = unnormalize(modelUbo.minDiffuse, modelUbo.maxDiffuse, r);
		r = max(0, r);
	}
	return r;
}

float pointDiffusion(vec3 lightPos, vec3 nrmTan, float intensity) {
	float dist = distance(lightPos, frg_worldPos);
	vec3 dirTan = -normalize(frg_tbnInverse * (lightPos - frg_worldPos.xyz));
	return (intensity / dist) * rayDiffusion(dirTan, nrmTan);
}


float raySpecular(vec3 lightDirTan, vec3 nrmTan) {
	float r = modelUbo.minSpecular;
	if(-0.5 < dot(-lightDirTan, frg_nrmTan)) {
		float SHININESS = pow(2, 4); // Should be configurable
		vec3 viewDir = frg_tbnInverse * normalize(frameUbo.viewPos - frg_worldPos);
		vec3 reflectDir = reflect(-lightDirTan, nrmTan);
		r = dot(-viewDir, reflectDir);
		r = pow(r, SHININESS);
		r = unnormalize(modelUbo.minSpecular, modelUbo.maxSpecular, r);
		r = max(0, r);
	}
	return r;
}

float pointSpecular(vec3 lightPos, vec3 nrmTan, float intensity) {
	vec3 dirTan = -normalize(frg_tbnInverse * (lightPos - frg_worldPos));
	float dist = distance(lightPos, frg_worldPos);
	return (intensity / dist) * raySpecular(dirTan, nrmTan);
}


// Everything minus cel shading and normal maps
void main_0() {
	float reflection =
		pointDiffusion(frameUbo.pointLight.xyz, frg_nrmTan, frameUbo.pointLight.w) +
		rayDiffusion(frg_lightDirTan, frg_nrmTan) +
		pointSpecular(frameUbo.pointLight.xyz, frg_nrmTan, frameUbo.pointLight.w) +
		raySpecular(frg_lightDirTan, frg_nrmTan);

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Light diffusion
void main_1() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float diffusion =
		pointDiffusion(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		rayDiffusion(frg_lightDirTan, tex_nrmTan);

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= diffusion;

	if(out_col.w < 0.01)  discard;
}


// Cel shading, diffuse lighting
void main_2() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float diffusion =
		pointDiffusion(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		rayDiffusion(frg_lightDirTan, tex_nrmTan);

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= shave(diffusion, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Specular light
void main_3() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float reflection =
		pointSpecular(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		raySpecular(frg_lightDirTan, tex_nrmTan);

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Cel shading, specular light
void main_4() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float reflection =
		pointSpecular(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		raySpecular(frg_lightDirTan, tex_nrmTan);

	out_col =
		texture(tex_dfsSampler, frg_tex)
		* frg_col;

	out_col.xyz *= shave(reflection, 1.0 / float(staticUbo.lightLevels), 0.5);

	if(out_col.w < 0.01)  discard;
}


// Everything minus cel shading
void main_5() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float reflection =
		pointDiffusion(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		rayDiffusion(frg_lightDirTan, tex_nrmTan) +
		pointSpecular(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		raySpecular(frg_lightDirTan, tex_nrmTan);

	out_col = texture(tex_dfsSampler, frg_tex) * frg_col;
	out_col.xyz *= reflection;

	if(out_col.w < 0.01)  discard;
}


// Everything
void main_6() {
	vec3 tex_nrmTan = get_normal_tanspace();

	float reflection =
		pointDiffusion(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		rayDiffusion(frg_lightDirTan, tex_nrmTan) +
		pointSpecular(frameUbo.pointLight.xyz, tex_nrmTan, frameUbo.pointLight.w) +
		raySpecular(frg_lightDirTan, tex_nrmTan);

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
