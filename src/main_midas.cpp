#include "mfe.h"
#include "midas.h"
#include "odbxx.h"

#include <cstdint>
#include <memory>

#include "caen-digitiser.h" // provides CaenFrontend + RECORD_LEN
#include "daq-controller.h"
#include "dsp-controller.h"
#include "filter.h"
#include "hamming-window.h"
#include "hann-window.h"
#include "mixer.h"
#include "psd.h"

namespace axion {

using SampleT = float;
using StoreT = float; // on-disk / in-event quantisation type

// Stage parameters
static constexpr std::size_t N = 80000;
static constexpr double Fs = 62.5e6;
static constexpr double f0 = 6e6;
static constexpr double fmix = 5e6;
static constexpr std::size_t Taps = 128;
static constexpr std::size_t Decim = 8;
static constexpr double Fc = 3e6;
static constexpr std::size_t FFTSize = 1000;
static constexpr std::size_t Overlap = 100;
static constexpr double FsD = Fs / Decim;
static constexpr std::size_t PSD_In = N / Decim;

// Prescale parameters
// 0 = disabled, 1 = every buffer, K = every K-th buffer.
static constexpr uint32_t DIG_Prescale = 100;
static constexpr uint32_t MIX_Prescale = 100;
static constexpr uint32_t FIR_Prescale = 100;
static constexpr uint32_t PSD_Prescale = 1;

// Accumulate parameters
static constexpr uint32_t DIG_AccumulateSize = 1;
static constexpr uint32_t MIX_AccumulateSize = 1;
static constexpr uint32_t FIR_AccumulateSize = 1;
static constexpr uint32_t PSD_AccumulateSize = 100;

static constexpr bool DIG_AccumulateNorm = false;
static constexpr bool MIX_AccumulateNorm = false;
static constexpr bool FIR_AccumulateNorm = false;
static constexpr bool PSD_AccumulateNorm = false;

using Digitiser = CaenDigitiser<SampleT, N, f0, Fs, DIG_Prescale,
                                DIG_AccumulateSize, DIG_AccumulateNorm>;

// DSP stages
using Mixer = DownMixer<SampleT, Fs, fmix, N>;
using FIR = DecimatingFIR<SampleT, Taps, Decim, Fc, Fs, N, HammingWindow>;
using PSD = WelchPSD<SampleT, FFTSize, Overlap, FsD, PSD_In, HannWindow>;

// using DSP = DSPController<SampleT, Mixer, FIR, PSD>;
using DSP_MIX = DSPController<SampleT, MIX_Prescale, MIX_AccumulateSize,
                              MIX_AccumulateNorm, Mixer>;
using DSP_FIR = DSPController<SampleT, FIR_Prescale, FIR_AccumulateSize,
                              FIR_AccumulateNorm, FIR>;
using DSP_PSD = DSPController<SampleT, PSD_Prescale, PSD_AccumulateSize,
                              PSD_AccumulateNorm, PSD>;

using DAQ =
    DAQController<SampleT, StoreT, Digitiser, DSP_MIX, DSP_FIR, DSP_PSD>;

// DAQ instance: pointer constructed in frontend_init, destroyed in
// frontend_exit
static std::unique_ptr<DAQ> daq;
static Digitiser dig;
static DSP_MIX dsp_mix(Mixer{});
static DSP_FIR dsp_fir(FIR{});
static DSP_PSD dsp_psd(PSD{});

} // namespace main

// MIDAS boilerplate
INT display_period = 1000;
INT max_event_size = 4 * 1024 * 1024;
INT max_event_size_frag = 5 * 1024 * 1024;
INT event_buffer_size = 10 * max_event_size;
BOOL equipment_common_overwrite = FALSE;

const char *frontend_name = "DAQ Frontend";
const char *frontend_file_name = __FILE__;

INT read_trigger_event(char *pevent, INT off);
INT poll_event(INT source, INT count, BOOL test);

EQUIPMENT equipment[] = {{"DAQ DSP",
                          {1, 0, "SYSTEM", EQ_POLLED, 0, "MIDAS", TRUE,
                           RO_RUNNING, 100, 0, 0, 0, "", "", "", "", "", 0},
                          read_trigger_event},
                         {""}};

INT frontend_init() {
  try {
    axion::daq = std::make_unique<axion::DAQ>(std::move(axion::dig), std::move(axion::dsp_mix),
                                std::move(axion::dsp_fir), std::move(axion::dsp_psd));

    // setup odb
    midas::odb meta;
    meta.connect("/Equipment/DAQ DSP/Settings");

    // stage 0 - Digitiser
    meta["00"]["type"] = "waveform";
    meta["00"]["title"] = "Raw Waveforms";
    meta["00"]["x_unit"] = "Sample";
    meta["00"]["y_unit"] = "V";
    meta["00"]["sample_rate"] = axion::Fs;

    // stage 1 - MIX
    meta["01"]["type"] = "waveform";
    meta["01"]["title"] = "Downmixed Waveforms";
    meta["01"]["x_unit"] = "Sample";
    meta["01"]["y_unit"] = "V";
    meta["01"]["sample_rate"] = axion::Fs;

    // stage 2 - FIR
    meta["02"]["type"] = "waveform";
    meta["02"]["title"] = "Filtered & Decimated Waveforms";
    meta["02"]["x_unit"] = "Sample";
    meta["02"]["y_unit"] = "V";
    meta["02"]["sample_rate"] = axion::FsD;

    // stage 3 - PSD
    meta["03"]["type"] = "psd";
    meta["03"]["title"] = "Power Spectral Density";
    meta["03"]["x_unit"] = "MHz";
    meta["03"]["y_unit"] = "V^2/Hz";
    meta["03"]["sample_rate"] = axion::FsD;

  } catch (const std::exception &e) {
    cm_msg(MERROR, "frontend_init", "Init failed: %s", e.what());
    return FE_ERR_HW;
  }
  return SUCCESS;
}

INT frontend_exit() {
  if (axion::daq) {
    axion::daq->stop();
    axion::daq.reset();
  }
  return SUCCESS;
}

INT begin_of_run(INT /*run_number*/, char * /*error*/) {

  // start daq controller
  try {
    axion::daq->run(/*cpu_offset=*/10);
  } catch (...) {
    return FE_ERR_HW;
  }
  return SUCCESS;
}

INT end_of_run(INT /*run_number*/, char * /*error*/) {
  if (axion::daq)
    axion::daq->stop();
  return SUCCESS;
}

INT pause_run(INT /*run_number*/, char * /*error*/) {
  if (axion::daq)
    axion::daq->stop();
  return SUCCESS;
}

INT resume_run(INT /*run_number*/, char * /*error*/) {
  try {
    axion::daq->run(/*cpu_offset=*/1);
  } catch (...) {
    return FE_ERR_HW;
  }
  return SUCCESS;
}

// Poll / read
// poll_event - called by MIDAS to check for data.
// Returns TRUE when the final DSP stage has produced at least one event.
INT poll_event(INT /*source*/, INT count, BOOL test) {
  for (int i = 0; i < count; ++i)
    if (axion::daq && axion::daq->has_event())
      return TRUE;
  return FALSE;
}

// read_trigger_event - called by MIDAS immediately after poll_event returns
// TRUE. Delegates entirely to DAQController, which pops the final pipeline
// buffer, taps it, drains all tap queues, and writes MIDAS banks.
INT read_trigger_event(char *pevent, INT /*off*/) {
  if (!axion::daq)
    return 0;
  return axion::daq->fill_midas_event(pevent);
}

// Unused stubs required by mfe.h
INT interrupt_configure(INT, INT, POINTER_T) { return SUCCESS; }
INT frontend_loop() { return SUCCESS; }
BOOL frontend_call_loop = FALSE;
