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
	float ambient;
	float diffuse;
	float specular;
	float shininess;
	float rnd;
	uint celLevels;
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
layout(set = 1, binding = 2) uniform sampler2D tex_spcSampler;
layout(set = 1, binding = 3) uniform sampler2D tex_nrmSampler;



layout(location = 0) in vec2 frg_tex;
layout(location = 1) in vec3 frg_lightDirTan;
layout(location = 2) in vec3 frg_nrmTan;
layout(location = 3) in vec4 frg_col;
layout(location = 4) in vec3 frg_worldPos;
layout(location = 8) in mat3 frg_tbnInverse;

layout(location = 0) out vec4 out_col;



struct PointLightInfo {
	vec4 dirTan; // DIRection in TANgent space (4th component is the light-fragment distance)
};

PointLightInfo mkPointLightInfo(vec3 lightPos) {
	return PointLightInfo(vec4(
			normalize(frg_tbnInverse * (lightPos - frg_worldPos)),
			distance(lightPos, frg_worldPos)));
}



float unnormalize(float low, float high, float normalized) {
	return (low * (1 - normalized)) + (high * normalized);
}


float shave(float value, float stride, float bias) {
	float valueMul = value / stride;
	float valueFloor = floor(valueMul + bias);
	return valueFloor * stride;
}


float celShade(float lightLevel) {
	float r = lightLevel;
	float noCelShading = float(modelUbo.celLevels < 1);
	// (modelUbo.celLevels + noCelShading) has no tangible effect, but prevents division by zero
	r =
		(1-noCelShading) * shave(r, 1.0 / float(modelUbo.celLevels + noCelShading), 0.5) +
		noCelShading * r;
	return r;
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


vec3 clampColor(vec3 colRgb) {
	float adjust = max(max(colRgb.r, colRgb.g), colRgb.b);
	adjust = max(1, adjust) - 1;
	colRgb += adjust;
	colRgb = min(colRgb, 1);
	return colRgb;
}


float rayDiffusion(vec3 lightDirTan, vec3 nrmTan) {
	float r;
	r = dot(-lightDirTan, nrmTan);
	r = max(0, r);
	r = unnormalize(modelUbo.ambient, modelUbo.diffuse, r);
	r = max(0, r);
	return r;
}

float pointDiffusion(PointLightInfo pli, vec3 nrmTan, float intensity) {
	return (intensity / pli.dirTan.w) * rayDiffusion(-pli.dirTan.xyz, nrmTan);
}


float raySpecular(vec3 lightDirTan, vec3 nrmTan) {
	vec3 viewDir = frg_tbnInverse * normalize(frameUbo.viewPos - frg_worldPos);
	vec3 reflectDir = reflect(-lightDirTan, nrmTan);
	float r;
	r = dot(-viewDir, reflectDir);
	r = max(0, r);
	r = pow(r, modelUbo.shininess);
	r = r * modelUbo.specular;
	r = max(0, r);
	return r;
}

float pointSpecular(PointLightInfo pli, vec3 nrmTan, float intensity) {
	return (intensity / pli.dirTan.w) * raySpecular(-pli.dirTan.xyz, nrmTan);
}


// Everything minus cel shading and normal maps
void main_0() {
	// Calculate diffuse and specular multipliers, using the fragment normal
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		diffuse =
			pointDiffusion(pli, frg_nrmTan, frameUbo.pointLight.w) +
			rayDiffusion(frg_lightDirTan, frg_nrmTan);
		specular =
			pointSpecular(pli, frg_nrmTan, frameUbo.pointLight.w) +
			raySpecular(frg_lightDirTan, frg_nrmTan);
	}

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Light diffusion
void main_1() {
	// Calculate diffuse multiplier
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse =
			pointDiffusion(pli, tex_nrmTan, frameUbo.pointLight.w) +
			rayDiffusion(frg_lightDirTan, tex_nrmTan);
		specular = 0;
	}

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Cel shading, diffuse lighting
void main_2() {
	// Calculate diffuse multiplier
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse =
			pointDiffusion(pli, tex_nrmTan, frameUbo.pointLight.w) +
			rayDiffusion(frg_lightDirTan, tex_nrmTan);
		specular = 0;
	}

	// Apply cel-shading
	diffuse = celShade(diffuse);
	specular = celShade(specular);

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Specular light
void main_3() {
	// Calculate specular multiplier
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse = 0;
		specular =
			pointSpecular(pli, tex_nrmTan, frameUbo.pointLight.w) +
			raySpecular(frg_lightDirTan, tex_nrmTan);
	}

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Cel shading, specular light
void main_4() {
	// Calculate specular multiplier
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse = 0;
		specular =
			pointSpecular(pli, tex_nrmTan, frameUbo.pointLight.w) +
			raySpecular(frg_lightDirTan, tex_nrmTan);
	}

	// Apply cel-shading
	diffuse = celShade(diffuse);
	specular = celShade(specular);

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Everything minus cel shading
void main_5() {
	// Calculate diffuse and specular multipliers
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse =
			pointDiffusion(pli, tex_nrmTan, frameUbo.pointLight.w) +
			rayDiffusion(frg_lightDirTan, tex_nrmTan);
		specular =
			pointSpecular(pli, tex_nrmTan, frameUbo.pointLight.w) +
			raySpecular(frg_lightDirTan, tex_nrmTan);
	}

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

	if(out_col.w < 0.01)  discard;
}


// Everything
void main_6() {
	// Calculate diffuse and specular multipliers
	float diffuse, specular;
	{
		PointLightInfo pli = mkPointLightInfo(frameUbo.pointLight.xyz);
		vec3 tex_nrmTan = get_normal_tanspace();
		diffuse =
			pointDiffusion(pli, tex_nrmTan, frameUbo.pointLight.w) +
			rayDiffusion(frg_lightDirTan, tex_nrmTan);
		specular =
			pointSpecular(pli, tex_nrmTan, frameUbo.pointLight.w) +
			raySpecular(frg_lightDirTan, tex_nrmTan);
	}

	// Apply cel-shading
	diffuse = celShade(diffuse);
	specular = celShade(specular);

	// Compute the color output
	out_col.rgb =
		(texture(tex_dfsSampler, frg_tex).xyz * diffuse) +
		(texture(tex_spcSampler, frg_tex).xyz * specular);
	out_col.a = 1;
	out_col *= frg_col;

	//  Mitigate artifacts if any color component is >1
	out_col.rgb = clampColor(out_col.rgb);

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
