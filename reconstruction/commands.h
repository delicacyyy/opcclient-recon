#pragma once

#include "cli.h"

namespace opcclient {

void Discover(const Options& options, const RuntimeSettings& settings);
void Status(const Options& options, const RuntimeSettings& settings);
void Browse(const Options& options, const RuntimeSettings& settings);
void Interactive(const Options& options, const RuntimeSettings& settings);
void Read(const Options& options, const RuntimeSettings& settings);
void Subscribe(const Options& options, const RuntimeSettings& settings);
void Write(const Options& options, const RuntimeSettings& settings);

} // namespace opcclient
