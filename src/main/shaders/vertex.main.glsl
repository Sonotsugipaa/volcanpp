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



layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_nrm;
layout(location = 2) in vec3 in_nrmSmooth;
layout(location = 3) in vec3 in_tanu;
layout(location = 4) in vec3 in_tanv;
layout(location = 5) in vec2 in_tex;

layout(location = 6) in mat4 in_modelMat;
layout(location = 10) in vec4 in_col;
layout(location = 11) in float in_rnd;

layout(location = 0) out vec2 frg_tex;
layout(location = 1) out vec3 frg_lightDirTan;
layout(location = 2) out vec3 frg_nrmTan;
layout(location = 3) out vec4 frg_col;
layout(location = 4) out vec3 frg_worldPos;
layout(location = 5) out mat3 frg_tbn;
layout(location = 8) out mat3 frg_tbnInverse;



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



/* -- Selector values --
 * 0: Diffuse and specular lighting without using normal maps
 * 1: Diffuse lighting
 * 2: Diffuse lighting with cel shading and outline
 * 3: Specular lighting
 * 4: Specular lighting with cel shading and outline
 * 5: Diffuse and specular lighting
 * 6: Diffuse and specular lighting with cel shading and outline */
void main() {
	mat4 modelViewMat = frameUbo.view * in_modelMat;
	mat3 modelViewMat3 = mat3(modelViewMat);
	vec4 worldPos = in_modelMat * vec4(in_pos, 1.0);
	vec3 worldNrm = inverse(transpose(mat3(in_modelMat))) * normalize(in_nrm);
	vec4 viewPos = modelViewMat * vec4(in_pos, 1.0);
	vec4 viewPosNormalized = normalize(viewPos);
	vec3 worldTanU = mat3(in_modelMat) * normalize(in_tanu);
	vec3 worldTanV = mat3(in_modelMat) * normalize(in_tanv);
	mat3 tbnInverse = frg_tbnInverse = transpose(frg_tbn = mat3(worldTanU, worldTanV, worldNrm));

	gl_Position = staticUbo.proj * viewPos;

	frg_tex = in_tex;
	frg_nrmTan = tbnInverse * worldNrm;
	frg_lightDirTan = tbnInverse * normalize(frameUbo.lightDirection);
	frg_col = in_col;
	frg_worldPos = worldPos.xyz;
}
