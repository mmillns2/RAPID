#pragma once

#include <cstdint>
#include <array>

#include "midas.h"

// MIDAS TID mapping
template <typename T> struct midas_tid;
template <> struct midas_tid<uint8_t>   { static constexpr int value = TID_UINT8;  };
template <> struct midas_tid<uint16_t>  { static constexpr int value = TID_UINT16; };
template <> struct midas_tid<uint32_t>  { static constexpr int value = TID_UINT32; };
template <> struct midas_tid<uint64_t>  { static constexpr int value = TID_UINT64; };
template <> struct midas_tid<int16_t>   { static constexpr int value = TID_INT16;  };
template <> struct midas_tid<int32_t>   { static constexpr int value = TID_INT32;  };
template <> struct midas_tid<float>     { static constexpr int value = TID_FLOAT;  };
template <> struct midas_tid<double>    { static constexpr int value = TID_DOUBLE; };

/*
Bank-name helpers:
Each stage I gets two 4-char bank names: one for I-channel, one for Q.
Format: "SiNN" and "SqNN" where NN is the zero padded stage index.
E.g. stage 0 → "Si00" / "Sq00", stage 12 -> "Si12" / "Sq12".
Timestamp banks: "Ts00", "Ts01", ...

MIDAS bank names are exactly 4 chars. Build them at compile time as
a constexpr std::array<char,5> (null terminated for bk_create).
*/

constexpr std::array<char, 5> make_bank_name(char prefix0, char prefix1,
                                              std::size_t idx) {
    return {prefix0, prefix1,
            static_cast<char>('0' + (idx / 10) % 10),
            static_cast<char>('0' + idx % 10),
            '\0'};
}


