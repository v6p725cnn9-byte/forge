#pragma once

#include "engine/game/actors/actors.hpp"

namespace forge::script {

using Player = game::Player;
using Vehicle = game::Vehicle;
using Marker = game::Marker;
using Label = game::Label;
using Registry = game::Actors;

constexpr int kMaxPlayers = game::kMaxPlayers;
constexpr int kMaxVehicles = game::kMaxVehicles;
constexpr int kMaxMarkers = game::kMaxMarkers;
constexpr int kMaxLabels = game::kMaxLabels;

} // namespace forge::script
