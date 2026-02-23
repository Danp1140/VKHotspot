#version 460

#define MAX_STEPS 64

layout (location = 0) in vec2 uv;

layout (push_constant) uniform Constants {
	mat4 vp_c, vp_l, vp_c_inv;
	vec3 c_p;
	float t;
} k;

layout (set = 0, binding = 0) uniform sampler2D i;
layout (set = 0, binding = 1) uniform sampler2D d;
layout (set = 0, binding = 2) uniform sampler2D sm;

layout (location = 0) out vec4 c;

void frustCol(in vec3 looking_at_ws, out float a, out float b) {
	bool a_found = false;
	float temp_val;
	float denom;

	vec4 ls_c_p = k.vp_l * vec4(k.c_p, 1); ls_c_p.xyz /= ls_c_p.w;
	vec4 ls_la = k.vp_l * vec4(looking_at_ws, 1); ls_la.xyz /= ls_la.w;

	for (uint dim_idx = 0; dim_idx < 3; dim_idx++) {
		for (float pm = -1; pm < 2; pm += 2) {
			float denom = ls_la[dim_idx] - ls_c_p[dim_idx];
			if (denom != 0) { // technically could make an early escape based on the implied ortho of a zero denom...
				temp_val = (pm - ls_c_p[dim_idx]) / denom;
				vec3 test_reproj = (1 - temp_val) * ls_c_p.xyz + temp_val * ls_la.xyz;
				if (abs(test_reproj[(dim_idx + 1) % 3]) < 1 && abs(test_reproj[(dim_idx + 2) % 3]) < 1) {
					temp_val = clamp(temp_val, 0, 1);
					if (a_found) {
						b = temp_val;
						return;
					}
					a = temp_val;
					a_found = true;
				}
			}
		}
	}
	a = 0;
	b = 0;
}

float voxelValChecker(in vec3 v) {
	if (int(floor(v.x)) % 2 == 0 && int(floor(v.y)) % 2 == 0 && int(floor(v.z)) % 2 == 0) return 1;
	return 0;
}

float voxelValRain(in vec3 v) {
	const float drop_width = 0.01, drop_height = 0.5, velocity = 5, z_vel = 0.5;
	if (mod(v.x, 0.5) < drop_width && mod(v.z, 0.5) < drop_width && mod(v.y + velocity * k.t, 1) < drop_height) return 1;
	return 0;
}

float voxelValShadow(in vec3 v) {
	vec4 v_ls = k.vp_l * vec4(v, 1); v_ls.xyz /= v_ls.w;
	if (v_ls.z < texture(sm, v_ls.xy / 2 + vec2(0.5)).x) return 1;
	return 0;
}

float expDist(in float a, in float b, in float x) {
	const float n = 4;
	const float m = log((exp(1*n)-exp(0*n))/(1-0))/n;
	return mix(a, b, exp(n*(x-m)) - exp(n*(0-m)) + 0);
}

float linFog(in vec3 c, in vec3 f, in float near, in float far, in float fog_dens) {
	const float scale = 0.01;
	float fog_amt = 0;
	float factor = near;
	vec3 vol_coord;
	for (uint i = 0; i < MAX_STEPS; i++) {
		vol_coord = mix(c, f, factor);
		fog_amt += fog_dens * distance(c, f) * abs(near-far) / MAX_STEPS * voxelValShadow(vol_coord);
		factor += scale;
		if (factor > far) break;
	}
	return fog_amt;
}

float expFog(in vec3 c, in vec3 f, in float near, in float far, in float fog_dens) {
	const float scale = 0.01;
	float pos = near + scale;
	float fog = 0;
	vec3 v_c, last_v_c = mix(c, f, near);
	for (uint i = 0; i < MAX_STEPS; i++) {
		v_c = mix(c, f, pos);
		fog += fog_dens * distance(last_v_c, v_c) * voxelValShadow(v_c);
		pos *= 1 + scale;
		if (pos > far) break;
		last_v_c = v_c;
	}
	return fog;
}

void main() {
	vec3 dev_coord = vec3(uv * 2 - 1, texture(d, uv).x);
	vec4 wc = k.vp_c_inv * vec4(dev_coord, 1); wc.xyz /= wc.w;

	const float fog_density = 0.01;
	vec3 fog_color = vec3(1);

	float a, b;
	frustCol(wc.xyz, a, b);
	float near, far;
	if (a > b) {
		near = b;
		far = a;
	}
	else {
		near = a;
		far = b;
	}

	float fog_amt = linFog(k.c_p, wc.xyz, near, far, fog_density);
	// float fog_amt = expFog(k.c_p, wc.xyz, near, far, fog_density);

	c.xyz = mix(texture(i, uv).xyz, fog_color, fog_amt);
	c.a = 1;
}
