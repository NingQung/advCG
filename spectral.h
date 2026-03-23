#ifndef SPECTRAL_H
#define SPECTRAL_H

const double WL_MIN = 390.0;
const double WL_MAX = 830.0;

struct Wavelengths {
    double lambda[4];
    
    Wavelengths() {}
    
    static Wavelengths sample() {
        Wavelengths wl;
        for (int i = 0; i < 4; i++) {
            wl.lambda[i] = WL_MIN + random_double() * (WL_MAX - WL_MIN);
        }
        return wl;
    }
};

struct SpectralEnergy {
    double energy[4];
    
    SpectralEnergy() {
        for(int i=0; i<4; ++i) energy[i] = 0.0;
    }
    
    SpectralEnergy(double e0, double e1, double e2, double e3) {
        energy[0] = e0; energy[1] = e1;
        energy[2] = e2; energy[3] = e3;
    }

    SpectralEnergy& operator+=(const SpectralEnergy& v) {
        energy[0] += v.energy[0]; energy[1] += v.energy[1];
        energy[2] += v.energy[2]; energy[3] += v.energy[3];
        return *this;
    }

    SpectralEnergy& operator*=(double t) {
        energy[0] *= t; energy[1] *= t;
        energy[2] *= t; energy[3] *= t;
        return *this;
    }
};

inline SpectralEnergy operator+(const SpectralEnergy& u, const SpectralEnergy& v) {
    return SpectralEnergy(u.energy[0] + v.energy[0], u.energy[1] + v.energy[1],
                          u.energy[2] + v.energy[2], u.energy[3] + v.energy[3]);
}

inline SpectralEnergy operator*(const SpectralEnergy& u, const SpectralEnergy& v) {
    return SpectralEnergy(u.energy[0] * v.energy[0], u.energy[1] * v.energy[1],
                          u.energy[2] * v.energy[2], u.energy[3] * v.energy[3]);
}

inline SpectralEnergy operator*(double t, const SpectralEnergy& v) {
    return SpectralEnergy(t*v.energy[0], t*v.energy[1], t*v.energy[2], t*v.energy[3]);
}

inline SpectralEnergy operator*(const SpectralEnergy& v, double t) {
    return t * v;
}

inline SpectralEnergy operator/(const SpectralEnergy& v, double t) {
    return (1/t) * v;
}

#endif