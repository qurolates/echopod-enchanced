#pragma once

#include "codec/flac.h"
#include "core/replay_gain.h"
#include "device/audio_engine.h"

void replay_gain_probe_source(FlacInput input, AudioCodec codec, ReplayGainValues *out);
