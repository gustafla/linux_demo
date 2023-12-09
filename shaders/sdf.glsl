float sdPlaneXZ(vec3 p) {
    return p.y;
}

float sdPlane(vec3 p, vec3 n, float h) {
    return dot(p, n) + h;
}

float sdSphere(vec3 p, float s) {
    return length(p) - s;
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

vec2 opUnion(vec2 a, vec2 b, float f) {
    if (a.x < b.x && a.y + EPSILON >= f) {
        return a;
    }
    return b;
}
