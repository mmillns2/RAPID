#pragma once

#include <cstdint>
#include <memory>
#include <vector>

/*
A point-in-time copy of one stage's output buffer, taken at prescale rate.
Allocated on the heap and passed via shared_ptr so both the pipeline thread
(producer) and the writer thread (consumer) can hold a reference without
copying again. The pipeline thread allocates and fills the snapshot, then
releases its reference after pushing to the tap queue. The writer thread
holds the last reference until the batch is flushed to disk.
*/

template <typename SampleT>
struct StageSnapshot {
    uint8_t              stage_id  = 0;
    uint64_t             timestamp = 0;
    std::vector<SampleT> I;
    std::vector<SampleT> Q;   // empty for real-valued stages

    bool has_q() const noexcept { return !Q.empty(); }
};

template <typename SampleT>
using snapshot_ptr = std::shared_ptr<StageSnapshot<SampleT>>;
