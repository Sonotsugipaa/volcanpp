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
} staticUbo;

layout(set = 1, binding = 0) uniform ModelUbo {
	float minDiffuse;
	float maxDiffuse;
	float minSpecular;
	float maxSpecular;
	float shininess;
	uint celLevels;
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



layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_nrm;
layout(location = 2) in vec3 in_nrmSmooth;
layout(location = 3) in vec3 in_tanu;
layout(location = 4) in vec3 in_tanv;
layout(location = 5) in vec2 in_tex;

layout(location = 6) in mat4 in_modelMat;
layout(location = 10) in vec4 in_col;
layout(location = 11) in float in_rnd;



float unnormalize(float low, float high, float normalized) {
	return (low * (1 - normalized)) + (high * normalized);
}


float falloff(float x, float falloffAmount) {
	return ((falloffAmount * x) + x) - falloffAmount;
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



void main_0() {
	gl_Position = vec4(0, 0, 0, 0);
}


// Cel shading, reflection
void main_1() {
	mat4 modelViewMat = frameUbo.view * in_modelMat;
	mat3 modelViewMat3 = mat3(modelViewMat);
	vec4 worldPos = in_modelMat * vec4(in_pos, 1.0);
	vec3 worldNrm = inverse(transpose(mat3(in_modelMat))) * normalize(in_nrmSmooth);

	float rnd_mul = unnormalize(
		1 - staticUbo.outlineRnd, 1 + staticUbo.outlineRnd,
		rnd_f1(in_pos.x + in_pos.y + in_pos.z + modelUbo.rnd));

	worldPos.xyz += rnd_mul * worldNrm * staticUbo.outlineSize;

	vec4 viewPos = frameUbo.view * worldPos;
	vec3 viewNrm = mat3(frameUbo.view) * worldNrm;
	vec4 projPos = staticUbo.proj * viewPos;

	projPos.z += staticUbo.outlineDepth;

	gl_Position = projPos;
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
		case 0:
		case 1:  main_0();  break;
		case 2:  main_1();  break;
		case 3:  main_0();  break;
		case 4:  main_1();  break;
		case 5:  main_0();  break;
		case 6:
		default:  main_1();  break;
	}
}
