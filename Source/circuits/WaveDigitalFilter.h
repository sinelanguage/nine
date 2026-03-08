#pragma once
/**
 * Wave Digital Filter (WDF) framework for TR909 circuit modelling.
 *
 * WDF theory (Fettweis, 1986) maps analog circuit elements to wave
 * quantities a (incident wave) and b (reflected wave):
 *   a = V + R_port * I
 *   b = V - R_port * I
 *
 * Each element exposes:
 *   setIncidentWave(a)  – receives the incoming wave
 *   double reflected()  – returns the reflected wave
 *   double voltage()    – V at the port
 *   double current()    – I flowing into the port
 *   double portR()      – port resistance
 */

#include <cmath>
#include <cassert>

namespace wdf {

// ---------------------------------------------------------------------------
// Base port interface
// ---------------------------------------------------------------------------
struct Port {
    double a = 0.0; // incident wave
    double b = 0.0; // reflected wave
    double Rp = 1.0; // port resistance [Ω]

    double voltage() const noexcept { return 0.5 * (a + b); }
    double current() const noexcept { return 0.5 * (a - b) / Rp; }
};

// ---------------------------------------------------------------------------
// Resistor  R
// ---------------------------------------------------------------------------
struct Resistor : Port {
    explicit Resistor(double R) { Rp = R; }

    void setIncidentWave(double inc) noexcept { a = inc; b = 0.0; }
    double reflected() const noexcept { return b; }
};

// ---------------------------------------------------------------------------
// Capacitor  C  (bilinear transform, T = 1/fs)
// ---------------------------------------------------------------------------
struct Capacitor : Port {
    double state = 0.0;

    Capacitor(double C, double sampleRate) {
        Rp = 1.0 / (2.0 * C * sampleRate);
    }

    void setIncidentWave(double inc) noexcept {
        a = inc;
        b = state; // previous state
        state = inc; // update state for next sample
    }
    double reflected() const noexcept { return b; }
};

// ---------------------------------------------------------------------------
// Inductor  L  (bilinear transform)
// ---------------------------------------------------------------------------
struct Inductor : Port {
    double state = 0.0;

    Inductor(double L, double sampleRate) {
        Rp = 2.0 * L * sampleRate;
    }

    void setIncidentWave(double inc) noexcept {
        a = inc;
        b = -state;
        state = inc;
    }
    double reflected() const noexcept { return b; }
};

// ---------------------------------------------------------------------------
// Voltage source  Vs
// ---------------------------------------------------------------------------
struct VoltageSource : Port {
    double Vs = 0.0;

    explicit VoltageSource(double R = 1.0) { Rp = R; }

    void setVoltage(double v) noexcept { Vs = v; }
    void setIncidentWave(double inc) noexcept { a = inc; }
    double reflected() const noexcept { return 2.0 * Vs - a; }
};

// ---------------------------------------------------------------------------
// Current source  Is
// ---------------------------------------------------------------------------
struct CurrentSource : Port {
    double Is = 0.0;

    explicit CurrentSource(double R = 1e9) { Rp = R; }

    void setCurrentSource(double i) noexcept { Is = i; }
    void setIncidentWave(double inc) noexcept { a = inc; }
    double reflected() const noexcept { return 2.0 * Is * Rp + a; }
};

// ---------------------------------------------------------------------------
// Ideal diode (one-sided rectifier via Newton-Raphson, Shockley model)
// ---------------------------------------------------------------------------
struct Diode : Port {
    double Is  = 2.52e-9; // saturation current (1N4148-like)
    double Vt  = 0.02585; // thermal voltage at 300 K
    double nVt = 1.0 * 0.02585; // ideality * Vt

    explicit Diode(double portR = 1.0) { Rp = portR; }

    void setIncidentWave(double inc) noexcept {
        a = inc;
        // Newton-Raphson to solve: b = a - 2*Rp * Is*(exp((a-b)/(2*Rp*nVt)) - 1)
        double vD = voltage();
        for (int iter = 0; iter < 50; ++iter) {
            double eArg  = std::exp(vD / nVt);
            double f     = vD - (a - 2.0 * Rp * Is * (eArg - 1.0));
            double df    = 1.0 + 2.0 * Rp * Is / nVt * eArg;
            double delta = f / df;
            vD -= delta;
            if (std::abs(delta) < 1e-12) break;
        }
        b = 2.0 * vD - a;
    }
    double reflected() const noexcept { return b; }
};

// ---------------------------------------------------------------------------
// NPN BJT (Ebers-Moll simplified, common-emitter)
// Used for TR909 transistors (2SC1000 / 2SC945 type)
// ---------------------------------------------------------------------------
struct NPN_BJT {
    // Ebers-Moll parameters for small-signal NPN (2SC945 approx.)
    double IS  = 1e-14;   // saturation current
    double VT  = 0.02585; // thermal voltage
    double BF  = 200.0;   // forward beta
    double BR  = 5.0;     // reverse beta

    // Solve for Ic, Ib given Vbe, Vce
    void solve(double Vbe, double Vce,
               double& Ic, double& Ib, double& Ie) const noexcept
    {
        double Vbc = Vbe - Vce;
        double Iff = IS * (std::exp(Vbe / VT) - 1.0);
        double Irr = IS * (std::exp(Vbc / VT) - 1.0);
        Ic = Iff - Irr * (1.0 + 1.0 / BR);
        Ib = Iff / BF + Irr / BR;
        Ie = -(Ic + Ib);
    }
};

// ---------------------------------------------------------------------------
// Series adaptor (two-port, no free parameter port)
// Connects port1 and port2 in series; port0 is the upstream connection.
// Rp0 = Rp1 + Rp2
// ---------------------------------------------------------------------------
struct SeriesAdaptor {
    Port* p1 = nullptr;
    Port* p2 = nullptr;
    double Rp0 = 0.0; // upstream port resistance

    SeriesAdaptor(Port* port1, Port* port2)
        : p1(port1), p2(port2)
    {
        Rp0 = p1->Rp + p2->Rp;
    }

    // Call after setting incident waves on p1 and p2
    // b0 is what this adaptor returns to upstream
    double reflected() const noexcept
    {
        return -(p1->b + p2->b);
    }

    // Propagate incident wave from upstream (a0) into sub-ports
    void propagate(double a0) noexcept
    {
        double b1 = p1->b, b2 = p2->b;
        double a1 = a0 - b2 - b1 + b1;
        double a2 = a0 - b1 - b2 + b2;
        // Series: a1 = -a0 + 2*p1->b ... derive properly:
        double ratio1 = p1->Rp / Rp0;
        double ratio2 = p2->Rp / Rp0;
        a1 = a0 * (1.0 - 2.0 * ratio1) + 2.0 * ratio1 * b1 - 2.0 * ratio2 * b2;
        a2 = a0 * (1.0 - 2.0 * ratio2) + 2.0 * ratio2 * b2 - 2.0 * ratio1 * b1;
        p1->setIncidentWave(a1);
        p2->setIncidentWave(a2);
    }
};

// ---------------------------------------------------------------------------
// Parallel adaptor
// Rp0 = Rp1*Rp2 / (Rp1+Rp2)
// ---------------------------------------------------------------------------
struct ParallelAdaptor {
    Port* p1 = nullptr;
    Port* p2 = nullptr;
    double Rp0 = 0.0;

    ParallelAdaptor(Port* port1, Port* port2)
        : p1(port1), p2(port2)
    {
        Rp0 = (p1->Rp * p2->Rp) / (p1->Rp + p2->Rp);
    }

    double reflected() const noexcept
    {
        double g1 = 1.0 / p1->Rp, g2 = 1.0 / p2->Rp;
        double gSum = g1 + g2;
        return (g1 * p1->b + g2 * p2->b) / gSum * 2.0
               - (p1->b * g1 + p2->b * g2) / gSum; // simplifies to weighted sum
    }

    void propagate(double a0) noexcept
    {
        double g1 = 1.0 / p1->Rp, g2 = 1.0 / p2->Rp;
        double gSum = g1 + g2;
        double bIn1 = p1->b, bIn2 = p2->b;
        double a1 = (2.0 * g2 * (bIn2 - a0) / gSum) + a0;
        double a2 = (2.0 * g1 * (bIn1 - a0) / gSum) + a0;
        p1->setIncidentWave(a1);
        p2->setIncidentWave(a2);
    }
};

} // namespace wdf
