vec3 normal(vec3 p) {
    return normalize(vec3(
        sdf(vec3(p.x + EPSILON, p.y, p.z)) - sdf(vec3(p.x - EPSILON, p.y, p.z)),
        sdf(vec3(p.x, p.y + EPSILON, p.z)) - sdf(vec3(p.x, p.y - EPSILON, p.z)),
        sdf(vec3(p.x, p.y, p.z  + EPSILON)) - sdf(vec3(p.x, p.y, p.z - EPSILON))
    ));
}

vec2 march(vec3 o, vec3 d, vec3 param) {
    float t = param.x;
    float dist = 0.;
    float shadow = 1.;
    for (int i = 0; i < 128; i++) {
        dist = sdf(o + d * t);
        t += dist * (1. - EPSILON);
        shadow = min(shadow, param.z * dist / t);
        if (dist < EPSILON) {
            shadow = 0.;
            break;
        }
        if (t > param.y) {
            break;
        }
    }
    return vec2(t, shadow);
}
