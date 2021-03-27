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
		case 1: {
			// NOP: diffuse reflection doesn't need the eye direction
		} break;
		case 2:
		case 3:
		case 4:
		case 5:
		default: {
			frg_eyedir_tan = normalize(tbnInverse * normalize(viewPos.xyz));
		} break;
	}
}