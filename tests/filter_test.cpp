#include <cmath>
#include <numbers>
// #include <print>
#include <iostream>
#include <vector>

#include "helpers.h"
#include "timer.h"

#include "../src/filter.h"
#include "../src/hamming-window.h"

namespace test {

void fir_test(double f0) {
  constexpr size_t N{80000};
  constexpr double Fs{62.5e6};
  // constexpr double f0{0e6};

  constexpr size_t Decimation{8};
  constexpr size_t Taps{128};
  constexpr double Fc{3e6};
  constexpr double Fnyquist{Fs / (2*Decimation)};

  constexpr double Fs_out{Fs / Decimation};

  using SampleT = float;

  using FIR =
      DecimatingFIR<SampleT, Taps, Decimation, Fc, Fs, N, HammingWindow>;

  FIR fir;

  constexpr size_t N_out{FIR::static_output_size()};

  std::vector<SampleT> I(N), Q(N);
  std::vector<SampleT> I_out(N_out), Q_out(N_out);

  // Generate test tone
  for (size_t n = 0; n < N; ++n) {
    double phase{2.0 * std::numbers::pi * f0 * n / Fs};
    I[n] = 0.3*static_cast<SampleT>(std::cos(phase));
    Q[n] = 0.3*static_cast<SampleT>(std::sin(phase));
  }

  double time_passed{};
  Timer t;

  fir.process(I, Q, I_out, Q_out);

  time_passed = t.elapsed();

  std::cout << "Time taken to process: " <<  time_passed * 1e6 << " us\n";

  std::cout << "Decimation:       " << Decimation << '\n';
  std::cout << "Signal Frequency: " << f0 * 1e-6 << " MHz\n";
  std::cout << "Cutoff Frequency: " << Fc * 1e-6 << " MHz\n";
  std::cout << "Length of I:      " << I.size() << '\n';
  std::cout << "Length of I_out:  " << I_out.size() << '\n';
  std::cout << "Taps:             " << Taps << '\n';

  double A_i{help::signal_amplitude<SampleT>(I, Q)};
  double A_f{help::signal_amplitude<SampleT>(I_out, Q_out)};
  double f_f{help::estimate_frequency<SampleT>(I_out, Q_out, Fs / Decimation)};

  std::cout << "Initial Amplitude: " << A_i << " V\n";
  std::cout << "Final Amplitude:   " << A_f << " V\n";
  std::cout << "Final Frequency:   " << f_f * 1e-6 << " MHz\n";
}

void fir_tests() {
  // constexpr size_t N{10};
  // constexpr std::array<double, N>
  // f0s{1.8e6, 1.9e6, 2.0e6, 2.1e6, 2.2e6, 2.3e6, 2.4e6, 2.5e6, 2.6e6, 2.7e6};
  constexpr double Fnyquist{62.5e6 / (2*8)};
  constexpr size_t N{9};
  constexpr std::array<double, N> f0s{1e6, 2e6, 3e6, 4e6, 5e6, 6e6, 7e6, 8e6, Fnyquist};
  for (double f : f0s) {
    fir_test(f);
    std::cout << "-------------------------\n";
  }
}

} // namespace test

int main() { test::fir_tests(); }
