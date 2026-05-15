#pragma once

#include <CAEN_FELib.h>

#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
// #include <print>
#include "midas-traits.h"
#include <iostream>
#include <span>
#include <string.h>
#include <string>
#include <thread>
#include <vector>

// Currently, signal should the 6MHz with a phase of pi/2

// I = CH0
// Q = CH1

template <typename SampleT, size_t N, double f0, double Fs, uint32_t Prescale,
          uint32_t AccumulatorSize, bool AccumulatorNorm>
class CaenDigitiser {

  static_assert(f0 <= Fs / 2);

  // Constants
  static constexpr bool CONVERT_VOLTS = true;
  static constexpr size_t N_CHANNELS = 2;
  static constexpr size_t N_TOTAL_CHANNELS = 64;
  static constexpr size_t RECORD_LEN = N;
  static constexpr int DECIMATION = 2;
  static constexpr int TIMEOUT_MS = 100;
  static constexpr int TESTPULSE_WIDTH = 32; // width in ns
  static constexpr size_t CHI = 0;
  static constexpr size_t CHQ = 1;

public:
  explicit CaenDigitiser()
      : waveforms_{std::vector<std::vector<uint16_t>>(
            N_TOTAL_CHANNELS, std::vector<uint16_t>(RECORD_LEN))},
        waveform_sizes_{std::vector<size_t>(N_TOTAL_CHANNELS)} {
    init();
    start();
  }

  ~CaenDigitiser() {
    if (running_)
      stop();
    if (dev_)
      CAEN_FELib_Close(dev_);
    // std::println("caen polls: {}", m_polls);
  }

  /*
    void poll(uint64_t &ts, std::vector<SampleT> &I_out) {
      if (read()) {
        ts = static_cast<double>(timestamp_);
        I_out.assign(waveform_ptrs_[CHI], waveform_ptrs_[CHI] + N);
      }
    }
  */

  void poll(uint64_t &ts, std::vector<SampleT> &I_out,
            std::vector<SampleT> &Q_out) {
    if (read()) {
      ts = static_cast<double>(timestamp_);
      // std::println("read");
      if constexpr (CONVERT_VOLTS) {
        for (size_t i{0}; i < N; ++i) {
          I_out[i] =
              static_cast<SampleT>(waveform_ptrs_[CHI][i] * m_adc_to_volts_I);
          Q_out[i] =
              static_cast<SampleT>(waveform_ptrs_[CHQ][i] * m_adc_to_volts_Q);
        }
      } else {
        I_out.assign(waveform_ptrs_[0], waveform_ptrs_[0] + N);
        Q_out.assign(waveform_ptrs_[1], waveform_ptrs_[1] + N);
      }
      // std::cout << "I: " << I_out[N/2] << '\n';
      // std::cout << "Q: " << Q_out[N/2] << '\n';
    } else {
      // std::println("not read");
    }
    // std::cout << "dig\n";
  }

  static consteval size_t static_output_size() noexcept { return N; }

  static consteval size_t get_static_prescale() noexcept { return Prescale; }

  static consteval uint32_t static_accumulator_size() noexcept {
    return AccumulatorSize;
  }

  static consteval bool static_accumulator_norm() noexcept {
    return AccumulatorNorm;
  }

  //static constexpr int get_bank_tid() { return TID_UINT16; }

private:
  std::string uri_{"dig2://caendgtz-usb-25476/"};
  uint64_t dev_{0};
  uint64_t ep_{0};
  bool running_{false};
  double record_time_s_{0.0};
  double m_adc_to_volts_I{};
  double m_adc_to_volts_Q{};

  // Data returned by ReadData
  size_t event_size_{};
  uint64_t timestamp_{};
  // std::array<uint16_t *, N_CHANNELS> waveforms_{};
  // std::array<size_t, N_CHANNELS> waveform_sizes_{};

  std::vector<std::vector<uint16_t>> waveforms_;
  std::vector<uint16_t *> waveform_ptrs_;
  std::vector<size_t> waveform_sizes_;

  // Storage
  // static inline std::array<uint16_t, N_TOTAL_CHANNELS * RECORD_LEN>
  //     waveform_storage_{};

  int m_polls{0};

public:
  bool read() {
    // std::println("reading");
    int ec = CAEN_FELib_ReadData(ep_, TIMEOUT_MS, &event_size_, &timestamp_,
                                 waveform_ptrs_.data(), waveform_sizes_.data());
    // std::println("got data");
    if (ec == CAEN_FELib_Timeout)
      return false;
    // std::println("checking");
    check(ec, "ReadData");
    m_polls++;
    return true;
  }

private:
  void init() {
    check(CAEN_FELib_Open(uri_.c_str(), &dev_), "Open");

    char fwtype[64]{};
    check(CAEN_FELib_GetValue(dev_, "/par/FWTYPE", fwtype), "FWTYPE");
    if (strcasecmp(fwtype, "Scope") != 0)
      throw std::runtime_error("Scope firmware required");

    check(CAEN_FELib_SendCommand(dev_, "/cmd/RESET"), "RESET");

    // Endpoint
    check(CAEN_FELib_SetValue(dev_, "/endpoint/par/ACTIVEENDPOINT", "SCOPE"),
          "ACTIVEENDPOINT");
    check(CAEN_FELib_GetHandle(dev_, "/endpoint/SCOPE", &ep_), "Get endpoint");

    // Data format
    const char *fmt =
        "["
        " {\"name\":\"EVENT_SIZE\",\"type\":\"SIZE_T\"},"
        " {\"name\":\"TIMESTAMP\",\"type\":\"U64\"},"
        " {\"name\":\"WAVEFORM\",\"type\":\"U16\",\"dim\":2},"
        " {\"name\":\"WAVEFORM_SIZE\",\"type\":\"SIZE_T\",\"dim\":1}"
        "]";
    check(CAEN_FELib_SetReadDataFormat(ep_, fmt), "SetReadDataFormat");

    // Acquisition parameters
    check(CAEN_FELib_SetValue(dev_, "/par/DECIMATIONFACTOR",
                              std::to_string(DECIMATION).c_str()),
          "DECIMATION");
    check(CAEN_FELib_SetValue(dev_, "/par/RECORDLENGTHS",
                              std::to_string(RECORD_LEN).c_str()),
          "RECORDLENGTHS");
    check(CAEN_FELib_SetValue(dev_, "/par/PRETRIGGERS", "0"), "PRETRIGGERS");

    // Compute record time and use it as TestPulsePeriod
    char fs_str[256]{};
    check(CAEN_FELib_GetValue(dev_, "/par/ADC_SAMPLRATE", fs_str),
          "ADC_SAMPLRATE");
    double fs_adc = std::stod(fs_str) * 1e6; // MHz -> Hz
    record_time_s_ = double(RECORD_LEN * DECIMATION) / fs_adc;
    uint64_t test_pulse_period_ns =
        static_cast<uint64_t>(record_time_s_ * 1e9 * 1);
    std::cout << "Test pulse period: " << test_pulse_period_ns << '\n';

    // Configure internal TestPulse trigger
    check(CAEN_FELib_SetValue(dev_, "/par/TESTPULSEPERIOD",
                              std::to_string(test_pulse_period_ns).c_str()),
          "TestPulsePeriod");
    check(CAEN_FELib_SetValue(dev_, "/par/TESTPULSEWIDTH",
                              std::to_string(TESTPULSE_WIDTH).c_str()),
          "TestPulseWidth");
    check(CAEN_FELib_SetValue(dev_, "/par/ACQTRIGGERSOURCE", "TestPulse"),
          "TRIGSRC");
    check(CAEN_FELib_SetValue(dev_, "/par/ENTRIGGEROVERLAP", "TRUE"),
          "EnTriggerOverlap");

    // Enable only channel 0 and 31
    for (size_t ch = 0; ch < N_TOTAL_CHANNELS; ++ch) {
      std::string path = "/ch/" + std::to_string(ch) + "/par/CHENABLE";
      check(CAEN_FELib_SetValue(dev_, path.c_str(),
                                ch == CHI || ch == CHQ ? "TRUE" : "FALSE"),
            "CHENABLE");
    }

    // Waveform pointers
    // for (size_t ch = 0; ch < N_CHANNELS; ++ch)
    //  waveforms_[ch] = waveform_storage_.data() + ch * RECORD_LEN;
    waveform_ptrs_.resize(N_TOTAL_CHANNELS);
    for (size_t ch = 0; ch < N_TOTAL_CHANNELS; ++ch)
      waveform_ptrs_[ch] = waveforms_[ch].data();

    char value[64]{};

    std::string path_I = "/ch/" + std::to_string(CHI) + "/par/ADCTOVOLTS";
    check(CAEN_FELib_GetValue(dev_, path_I.c_str(), value), "ADCTOVOLTS_I");
    m_adc_to_volts_I = std::stod(std::string(value));

    std::string path_Q = "/ch/" + std::to_string(CHQ) + "/par/ADCTOVOLTS";
    check(CAEN_FELib_GetValue(dev_, path_Q.c_str(), value), "ADCTOVOLTS_Q");
    m_adc_to_volts_Q = std::stod(std::string(value));

    // std::println("I convserion: {}", m_adc_to_volts_I);
    // std::println("Q convserion: {}", m_adc_to_volts_Q);

    std::cout << "Record time: " << record_time_s_ * 1e3 << " ms\n";
  }
  void start() {
    check(CAEN_FELib_SendCommand(dev_, "/cmd/ARMACQUISITION"), "ARM");
    check(CAEN_FELib_SendCommand(dev_, "/cmd/SWSTARTACQUISITION"), "START");
    running_ = true;
  }

  void stop() {
    CAEN_FELib_SendCommand(dev_, "/cmd/DISARMACQUISITION");
    running_ = false;
    std::cout << "caen polls: " << m_polls << '\n';
  }

  void check(int ec, const char *what) {
    if (ec == CAEN_FELib_Success)
      return;

    char name[256]{};
    char last[1024]{};
    CAEN_FELib_GetErrorDescription(static_cast<CAEN_FELib_ErrorCode>(ec), name);
    CAEN_FELib_GetLastError(last);

    std::cerr << "[FATAL] " << what << ": " << name << "\n" << last << "\n";
    std::terminate();
  }
};
