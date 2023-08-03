#version 330 core

out vec4 FragColor;

uniform float u_RocketRow;
uniform vec2 u_Resolution;
uniform vec3 r_CamPos;

#define EPSILON 0.001

float sdSphere( vec3 p, float s ) {
    return length(p)-s;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz)-t.x,p.y);
    return length(q)-t.y;
}

float sdf(vec3 p) {
    return sdTorus(p, vec2(1., 0.3));
    //return sdSphere(p, 0.5);
}

vec3 calcNormal(vec3 pos) {
	const float h = EPSILON;
	#define ZERO (min(int(u_RocketRow),0))
	vec3 n = vec3(0.0);
	for( int i=ZERO; i<4; i++ ) {
		vec3 e = 0.5773*(2.0*vec3((((i+3)>>1)&1),((i>>1)&1),(i&1))-1.0);
		n += e*sdf(pos+e*h);
	}
	return normalize(n);
}

float march(vec3 o, vec3 d) {
    float t = EPSILON;
    float dist = 0.;
    for (int i = 0; i < 128; i++) {
        dist = sdf(o + d * t);
        t += dist * (1. - EPSILON);
        if (dist < EPSILON) {
            break;
        }
    }
    return t;
}

void main() {
    vec2 screen_pos = (gl_FragCoord.xy * 2. / u_Resolution) - vec2(1.);
    screen_pos.x *= u_Resolution.x / u_Resolution.y;

    vec3 dir = normalize(vec3(screen_pos, -1.));

    float t = march(r_CamPos, dir);
    vec3 pos = r_CamPos + dir * t;

    vec3 normal = calcNormal(pos);
    float ndotl = max(dot(-normal, vec3(0., -1., 0)), 0.);
    
    FragColor = vec4(normal * ndotl, 1.);
}
