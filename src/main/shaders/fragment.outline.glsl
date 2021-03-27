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



layout(location = 0) out vec4 out_col;



float unnormalize(float low, float high, float normalized) {
	return (low * (1 - normalized)) + (high * normalized);
}


float shave(float value, float stride) {
	return floor(value / stride) * stride;
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
	discard;
}

void main_1() {
	out_col = vec4(0,0,0, 1);
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
