# Möbius Spectral Canceller (MSC)

**A deterministic Möbius-indexed spectral gain processor.**

The current DSP applies a fixed arithmetic gain pattern to non-negative FFT bins. It does **not** calculate the Collective Gain quantity

\[
\Gamma(N) = \frac{M(N)^2}{G(N)}.
\]

That expression is part of the project's mathematical motivation, not an operation currently implemented by the plugin. The accurate implementation description is a Möbius-indexed spectral gain processor.

## Implemented spectral law

For each processed interior FFT bin:

\[
n = \operatorname{round}(k\cdot\mathrm{scale}),\qquad
g(n)=1+d\mu(n).
\]

The implementation applies this real gain to both the real and imaginary components of the bin. It therefore changes magnitude without phase inversion.

| Möbius value | Gain at depth d | Meaning |
|---|---:|---|
| \(\mu(n)=-1\) | \(1-d\) | Attenuation; reaches zero at d=1 |
| \(\mu(n)=0\) | \(1\) | Unchanged |
| \(\mu(n)=+1\) | \(1+d\) | Boost; reaches 2 (+6.02 dB) at d=1 |

Depth remains in [-1, 1]. At maximum positive depth, positive-Möbius bins can receive +6.02 dB per-bin gain. This can create peaks above full scale; the current implementation does not include a limiter or automatic headroom compensation. Set depth conservatively and manage downstream headroom.

DC and Nyquist are intentionally excluded. Only bins k=1 through N/2-1 are processed. The gain is non-negative over the full parameter range, so it does not invert phase.

## Parameters

| Parameter | Range | Default | Description |
|---|---:|---:|---|
| Depth | [-1, 1] | 0 | Intensity of the Möbius-indexed gain |
| Scale | [0.01, 10] | 1 | Maps FFT bin k to integer n = round(k * scale) |
| Mix | [0, 1] | 1 | Delayed dry/wet blend |

## DSP architecture

- 2048-point real FFT.
- Hann analysis and synthesis windows.
- 512-sample hop (75% overlap).
- Möbius linear sieve precomputed through N=16384.
- Independent per-channel input history, overlap-add accumulation, normalization, and dry-delay storage.
- Frame scheduling is driven by each channel's sample counter; host callback sizes do not define FFT frame boundaries.
- The processor reports a fixed 4096-sample latency to cover frame completion and output scheduling.
- FFT work storage is allocated at 2 * fftSize floats for JUCE's non-negative-frequency real transform path.
- juce::ScopedNoDenormals is active at the start of processBlock.

The overlap-add path accumulates the analysis/synthesis window product and divides by the accumulated squared-window weight. This is intended to make the zero-depth wet path reconstruct the delayed input; it still requires the automated identity/null test described below before release.

## Validation status

The repository's DSP changes must not be treated as release-certified until the following are built and run against the exact JUCE 8.0.4 backend:

1. Zero-depth, fully-wet identity/null test against input delayed by 4096 samples.
2. Mix=0 delayed-dry test against the same latency.
3. Block-size invariance for 64, 128, 256, 512, and 1024 sample callbacks.
4. Stereo isolation with different left/right signals.
5. ASan/UBSan runs over short and long streams.
6. Parameter automation and state save/restore.
7. Peak/headroom measurements at positive depth, including broadband and transient material.
8. Real-time allocation/lock instrumentation and CPU profiling.

No benchmark, sanitizer, host-compatibility, or runtime pass is claimed by this source update alone.

## Build

    git clone https://github.com/ljrseals-dev/Mobius-Cancellar.git
    cd Mobius-Cancellar
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release

Requires CMake 3.22 and JUCE 8.0.4 (fetched by CMake). The plugin targets VST3 and Standalone.

## License

MIT — see LICENSE. The copyright holder should be confirmed by the project rights holder before distribution.
