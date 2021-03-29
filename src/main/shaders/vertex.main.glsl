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
	mat4 world; // This should be refactored to "model", for consistency
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
	float expSpecular;
	float maxSpecular;
	float rnd;
} modelUbo;

layout(set = 2, binding = 0) uniform FrameUbo {
	mat4 view;
	vec3 lightDirection;
	float rnd;
	uint shaderSelector;
} frameUbo;



layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_nrm;
layout(location = 2) in vec3 in_nrm_smooth;
layout(location = 3) in vec3 in_tanu;
layout(location = 4) in vec3 in_tanv;
layout(location = 5) in vec2 in_tex;

layout(location = 0) out vec2 frg_tex;
layout(location = 1) out vec3 frg_eyedir_tan;
layout(location = 2) out vec3 frg_lightdir_tan;
layout(location = 3) out vec3 frg_nrm_tan;



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
 * 0: Raw diffuse texture
 * 1: Diffuse lighting
 * 2: Diffuse lighting with cel shading and outline
 * 3: Specular lighting
 * 4: Specular lighting with cel shading and outline
 * 5: Diffuse and specular lighting
 * 6: Diffuse and specular lighting with cel shading and outline */
void main() {
	mat4 modelViewMat = frameUbo.view * objectPc.world;
	mat3 modelViewMat3 = mat3(modelViewMat);
	vec4 worldPos = objectPc.world * vec4(in_pos, 1.0);
	vec3 worldNrm = inverse(transpose(mat3(objectPc.world))) * normalize(in_nrm);
	vec4 viewPos = modelViewMat * vec4(in_pos, 1.0);
	vec3 viewTanU = modelViewMat3 * normalize(in_tanu);
	vec3 viewTanV = modelViewMat3 * normalize(in_tanv);
	vec3 viewNrm = inverse(transpose(modelViewMat3)) * normalize(in_nrm);
	mat3 tbnInverse = transpose(mat3(viewTanU, viewTanV, viewNrm));

	gl_Position = staticUbo.proj * viewPos;

	frg_tex = in_tex;
	frg_nrm_tan = tbnInverse * viewNrm;
	frg_lightdir_tan = normalize(tbnInverse * mat3(frameUbo.view) * frameUbo.lightDirection);

	switch(frameUbo.shaderSelector) {
		case 0:
		case 1:
		case 2: {
			// NOP: diffuse reflection doesn't need the eye direction
		} break;
		case 3:
		case 4:
		case 5:
		case 6:
		default: {
			frg_eyedir_tan = normalize(tbnInverse * normalize(viewPos.xyz));
		} break;
	}
}
