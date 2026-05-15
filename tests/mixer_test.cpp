#include <cmath>
//#include <print>
#include <vector>
#include <iostream>

#include "helpers.h"
#include "timer.h"

#include "../src/mixer.h"

namespace test {

void mixer_test() {
  constexpr size_t N{80000};
  constexpr double Fs{62.55e6};
  constexpr double f0{6e6};
  constexpr double fmix{5e6};

  using SampleT = float;

  std::vector<SampleT> I(N), Q(N);
  std::vector<SampleT> I_out(N), Q_out(N);

  // Generate test tone
  for (size_t n = 0; n < N; ++n) {
    double phase{2.0 * std::numbers::pi * f0 * n / Fs};
    I[n] = std::cos(phase);
    Q[n] = std::sin(phase);
  }

  DownMixer<SampleT, Fs, fmix, N> mixer;

  double time_passed{};
  Timer t;

  mixer.process(I, Q, I_out, Q_out);

  time_passed = t.elapsed();

  std::cout << "Time taken to process: " << time_passed * 1e6 << " us\n";

  double F_est_i{help::estimate_frequency<SampleT>(I, Q, Fs)};
  double F_est_f{help::estimate_frequency<SampleT>(I_out, Q_out, Fs)};

  std::cout << "Initial Frequency Estimate: " << F_est_i * 1e-6 << " MHz\n";
  std::cout << "Final Frequency Estimate:   " << F_est_f * 1e-6 << " MHz\n";
}

} // namespace test

int main() { test::mixer_test(); }
