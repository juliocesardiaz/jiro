//
// SPStream.cpp
//
#include "SPStream.hpp"

namespace sonicpatch {

// Non-Apple builds: nothing to implement; StreamFormat is a POD describing the
// intended stream. The Apple build will define SPStream's libASPL overrides here.
//
// TODO(Phase 5): implement aspl::Stream overrides:
//   * report the AudioStreamBasicDescription derived from StreamFormat
//   * advertise the single available format in the format list
//   * apply volume/mute if the stream exposes per-stream controls

} // namespace sonicpatch
