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



layout(location = 0) in vec3 in_pos;
layout(location = 1) in vec3 in_nrm;
layout(location = 2) in vec3 in_nrm_smooth;
layout(location = 3) in vec3 in_tanu;
layout(location = 4) in vec3 in_tanv;
layout(location = 5) in vec2 in_tex;



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
	mat4 modelViewMat = frameUbo.view * objectPc.world;
	mat3 modelViewMat3 = mat3(modelViewMat);
	vec4 worldPos = objectPc.world * vec4(in_pos, 1.0);
	vec3 worldNrm = inverse(transpose(mat3(objectPc.world))) * normalize(in_nrm_smooth);

	worldPos.xyz += worldNrm * staticUbo.outlineSize;

	vec4 viewPos = frameUbo.view * worldPos;
	vec3 viewNrm = mat3(frameUbo.view) * worldNrm;
	vec4 projPos = staticUbo.proj * viewPos;

	projPos.z += staticUbo.outlineDepth;

	gl_Position = projPos;
}



void main() {
	switch(frameUbo.shaderSelector) {
		case 0:  main_0();  break;
		case 1:  main_1();  break;
		case 2:  main_0();  break;
		case 3:  main_1();  break;
		case 4:  main_0();  break;
		case 5:
		default:  main_1();  break;
	}
}
