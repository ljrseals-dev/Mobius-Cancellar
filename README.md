# Möbius Spectral Canceller (MSC)

**A deterministic Möbius-indexed spectral gain processor.**

The current DSP applies a Möbius-indexed gain law to interior bins of a short-time Fourier transform (STFT). It does **not** calculate the Collective Gain quantity

\[
\Gamma(N)=\frac{M(N)^2}{G(N)}.
\]

That expression is part of the mathematical motivation, not an operation implemented by the plugin. The accurate description is a Möbius-indexed spectral gain processor.

## Spectral law

For each interior positive-frequency bin:

\[
n=\operatorname{round}(k\cdot\mathrm{scale}),\qquad
g(n)=1+d\mu(n).
\]

The same real scalar is applied to the positive-frequency complex coefficient and its conjugate negative-frequency partner. Phase is preserved; no phase inversion is performed.

| Möbius value | Gain at d=1 | Meaning |
|---|---:|---|
| \(\mu(n)=-1\) | 0 | Fully attenuated |
| \(\mu(n)=0\) | 1 | Unchanged |
| \(\mu(n)=+1\) | 2 (+6.02 dB) | Boosted |

Depth is clamped to [-1, 1]. Positive depth can create peaks above full scale; there is no limiter or automatic headroom compensation. The default depth is zero. Use conservative settings and manage downstream headroom.

DC and Nyquist are intentionally excluded. Only bins k=1 through N/2-1 are processed.

## Parameters

| Parameter | Range | Default |
|---|---:|---:|
| Depth | [-1, 1] | 0 |
| Bin Scale | [0.01, 10] | 1 |
| Dry/Wet | [0, 1] | 1 |

## DSP implementation

- 2048-point real-signal radix-2 FFT, implemented locally with fixed-size arrays and precomputed twiddles/bit-reversal indices.
- No JUCE FFT backend, transform-time heap allocation, or backend spinlock.
- Hann analysis and synthesis windows; 512-sample hop (75% overlap).
- Per-channel circular input history, overlap-add accumulation, normalization, and delayed-dry storage.
- Frame events are scheduled on each channel's sample clock, independently of host callback boundaries.
- OLA normalization divides by the accumulated squared Hann window, making the zero-depth wet path an identity reconstruction after latency.
- Reported plugin latency: 2048 samples.
- `juce::ScopedNoDenormals` is active in `processBlock`.
- All DSP working storage is allocated before audio processing begins.

## Verification

The CTest regression executable covers:
1. Möbius sieve known values and gain-law endpoints.
2. APVTS state save/restore.
3. Zero-depth, fully-wet identity reconstruction.
4. Delayed dry-path identity.
5. Stereo separation using distinct left/right signals.
6. Callback sizes 1, 64, 127, 128, 256, 512, and 1024 samples.
7. Exact reported latency check.

CI build and CTest results must pass before a release is considered. Sanitizer, plugin-host, long-duration, automation, and peak/headroom validation remain separate release checks; no such pass is claimed by the source changes alone.

## Build

    git clone https://github.com/ljrseals-dev/Mobius-Cancellar.git
    cd Mobius-Cancellar
    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMSC_BUILD_TESTS=ON
    cmake --build build --parallel
    ctest --test-dir build --output-on-failure

Requires CMake 3.22, a C/C++17 toolchain, and JUCE 8.0.4 fetched by CMake. Targets VST3 and Standalone.

## License

MIT — see LICENSE. Confirm the copyright holder and distribution rights before release.
