// From https://urs.nerd2nerd.org/saturn.glsl

// Mie-scattering, grainsize in meters
// (according to http://dx.doi.org/10.1364/JOSAA.2.000156 )
vec3 mie(float vdotl, float grainsize, float refractiveIndex) {

        //return vec3(.5) * (1 + vdotl*vdotl);

        // representative sRGB wavelengths in meters
        // TODO: use more than 3
        vec3 wavelengths = vec3(607,538,465)*1e-9;
       
        vec3 ka = 2 * 3.14159 * grainsize / wavelengths;
       
        float m = refractiveIndex;
        vec3 x = ka*sqrt(1 + m*m - 2*m*vdotl);
        //vec3 j1 = (sin(x) - x*cos(x))/(x*x);
        vec3 j1 = .5/(x*x);
        vec3 A = abs(3 * j1)/x + 1./sqrt(x*x*x);
        return .5 * A*A * (1 + vdotl*vdotl);
}

// Rayleigh-scattering
vec3 rayleigh(float vdotl, float grainsize, float refractiveIndex) {

        vec3 wavelengths = vec3(607,538,465)*1e-9;
       
        // Wavelengthfactor
        vec3 Wl4 = 2 * 3.14159 / wavelengths;
        Wl4 *= Wl4; Wl4 *= Wl4; // ^4
       
        // refractive factor
        float N2 = (refractiveIndex*refractiveIndex-1)/(refractiveIndex*refractiveIndex+2);
        N2 *= N2;
       
        // grainsize-factor
        float d6 = grainsize * 0.5;
        d6 *= d6; d6 *= d6*d6;
       
        return .5*(1+vdotl*vdotl) * Wl4 * N2 * d6;
}
