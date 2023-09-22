vec3 normal(vec3 p) {
    return normalize(vec3(
        sdf(vec3(p.x + EPSILON, p.y, p.z)) - sdf(vec3(p.x - EPSILON, p.y, p.z)),
        sdf(vec3(p.x, p.y + EPSILON, p.z)) - sdf(vec3(p.x, p.y - EPSILON, p.z)),
        sdf(vec3(p.x, p.y, p.z  + EPSILON)) - sdf(vec3(p.x, p.y, p.z - EPSILON))
    ));
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
        if (t > 1024.) {
            break;
        }
    }
    return t;
}
