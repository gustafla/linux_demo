vec3 normal(vec3 p, float f) {
    return normalize(vec3(
            sdf(vec3(p.x + EPSILON, p.y, p.z), f).x - sdf(vec3(p.x - EPSILON, p.y, p.z), f).x,
            sdf(vec3(p.x, p.y + EPSILON, p.z), f).x - sdf(vec3(p.x, p.y - EPSILON, p.z), f).x,
            sdf(vec3(p.x, p.y, p.z + EPSILON), f).x - sdf(vec3(p.x, p.y, p.z - EPSILON), f).x
        ));
}

vec3 march(vec3 o, vec3 d, vec3 param, float f) {
    float t = param.x;
    vec2 dist = vec2(0.);
    float shadow = 1.;
    for (int i = 0; i < 256; i++) {
        dist = sdf(o + d * t, f);
        t += dist.x * 0.8;
        shadow = min(shadow, param.z * dist.x / t);
        if (dist.x < EPSILON) {
            shadow = 0.;
            break;
        }
        if (t > param.y) {
            break;
        }
    }
    return vec3(t, dist.y, shadow);
}
