float sdSea(vec3 p) {
    p.y += sin(p.z * 0.225 + r_AnimationTime) * 1.;
    p.y += sin(p.z * 0.15 + r_AnimationTime) * 0.6;
    p.y += sin(p.x * 0.125 + r_AnimationTime) * 1.;
    p.xz = rotation(.1) * p.xz;
    p.y += abs(sin(p.x * 0.25 + r_AnimationTime)) * 0.5;
    p.xz = rotation(.6) * p.xz;
    p.y += abs(sin(p.x * 0.5 + r_AnimationTime)) * 0.25;
    p.xz = rotation(.9) * p.xz;
    p.y += abs(sin(p.x + r_AnimationTime)) * 0.125;
    return p.y;
}

vec3 normalSea(vec3 p) {
    return normalize(vec3(
        sdSea(vec3(p.x + EPSILON, p.y, p.z)) - sdSea(vec3(p.x - EPSILON, p.y, p.z)),
        sdSea(vec3(p.x, p.y + EPSILON, p.z)) - sdSea(vec3(p.x, p.y - EPSILON, p.z)),
        sdSea(vec3(p.x, p.y, p.z  + EPSILON)) - sdSea(vec3(p.x, p.y, p.z - EPSILON))
    ));
}

float marchSea(vec3 o, vec3 d) {
    float t = EPSILON;
    float dist = 0.;
    for (int i = 0; i < 256; i++) {
        dist = sdSea(o + d * t);
        t += dist * 0.8;
        if (dist < EPSILON) {
            break;
        }
        if (t > 1024.) {
            break;
        }
    }
    return t;
}
