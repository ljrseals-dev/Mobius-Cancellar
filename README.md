# Möbius Spectral Canceller (MSC)

**A deterministic, number-theoretic spectral processor**  
Derived from the *Collective Gain* operator and the restricted Rayleigh quotient of Modular Waveform Mechanics.

---

## Concept

The collective gain

\[
\Gamma(N) = \frac{M(N)^2}{G(N)}
\]

measures the net arithmetic cancellation present in the Möbius sequence \(\mu(n)\).  
This plugin projects that structure into the frequency domain:

| \(\mu(n)\) | Action on FFT bin |
|------------|-------------------|
| \(-1\)     | Phase inversion / attenuation (cancellation) |
| \(+1\)     | Boost (constructive interference) |
| \(0\)      | Mute (non-square-free “silence”) |

The result is a sparse, fractal-like spectral envelope that is **signal-independent** and audibly distinct from any statistical resonator or noise-reduction algorithm.

## Parameters

| ID     | Range          | Default | Description |
|--------|----------------|---------|-------------|
| Depth  | \([-1, 1]\)    | 0       | Intensity & polarity of the Möbius mask. 0 = bypass. |
| Scale  | \([0.01, 10]\) | 1       | Maps bin index \(k\) → integer \(n = \mathrm{round}(k\cdot\mathrm{scale})\). |
| Mix    | \([0, 1]\)     | 1       | Dry/Wet blend. |

## Architecture

- **STFT**: 2048-point real FFT, Hann window, 75 % overlap (hop = 512).
- **Möbius table**: linear sieve pre-computed at construction up to \(N=16384\).
- **Real-time safety**: only table look-ups and complex multiplies on the audio thread.
- **Framework**: JUCE 8, VST3 + Standalone.

## Build

```bash
git clone https://github.com/<your-user>/Mobius-Spectral-Canceller.git
cd Mobius-Spectral-Canceller
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The VST3 will appear in `build/MobiusSpectralCanceller_artefacts/Release/VST3/`.

Requires a C++17 compiler and CMake ≥ 3.22. JUCE is fetched automatically.

## Why this is new

Existing spectral tools decide which bins to cut from statistical or psycho-acoustic models of the *input*.  
The Möbius Spectral Canceller imposes a **fixed arithmetic constellation** derived from pure number theory.  
It is a direct sonification of the dual signed reserve / collective-gain operator link introduced in the companion paper on Collective Arithmetic Transformation.

## License

MIT – see [LICENSE](LICENSE).

---

*Modular Waveform Mechanics · November 2026*
