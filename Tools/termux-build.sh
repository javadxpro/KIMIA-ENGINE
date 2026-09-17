#!/bin/sh
# Kimia Engine — Termux / Linux build script
#
# Builds the headless Street Soccer demo without CMake, Gradle, or SDL.
# Just g++ + this script. Outputs ./kimia_street_soccer.
#
#   pkg install clang (or apt install g++)
#   chmod +x Tools/termux-build.sh
#   ./Tools/termux-build.sh
#   ./kimia_street_soccer --frames 600

set -e

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -Wall -Wno-unused-parameter}"
ENGINE_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

INCLUDES="-I ${ENGINE_ROOT}/Engine/Core/include \
          -I ${ENGINE_ROOT}/Engine/Math/include \
          -I ${ENGINE_ROOT}/Engine/Physics/include \
          -I ${ENGINE_ROOT}/Engine/StreetSoccer/include"

SOURCES="Engine/Core/src/Time.cpp \
         Engine/Physics/src/Physics.cpp \
         Engine/StreetSoccer/src/StreetSoccer.cpp \
         Engine/StreetSoccer/src/StreetSoccerPhysics.cpp \
         Engine/StreetSoccer/src/StreetAIController.cpp \
         Engine/StreetSoccer/src/SkillGatedPower.cpp \
         Engine/StreetSoccer/src/PlayerTraits.cpp \
         Engine/StreetSoccer/src/ReplaySystem.cpp \
         Engine/StreetSoccer/src/HighlightCapture.cpp \
         Engine/StreetSoccer/src/PhotoMode.cpp \
         Engine/StreetSoccer/src/StorySequence.cpp \
         Engine/StreetSoccer/src/TermuxConsole.cpp \
         Engine/StreetSoccer/src/TermuxInput.cpp \
         Engine/StreetSoccer/src/DensityMass.cpp \
         Engine/StreetSoccer/src/KimiaPhysics.cpp \
         Engine/StreetSoccer/src/CinematicCamera.cpp \
         Engine/StreetSoccer/src/CurrencySystem.cpp \
         Engine/StreetSoccer/src/GameMode.cpp \
         Engine/StreetSoccer/src/DialogueSystem.cpp \
         Engine/StreetSoccer/src/RandomEvents.cpp \
         Engine/StreetSoccer/src/DdaPolicy.cpp \
         Engine/StreetSoccer/src/StreetActions.cpp \
         Engine/StreetSoccer/src/HelicopterCamera.cpp \
         Engine/StreetSoccer/src/WebSnapshot.cpp \
         Examples/StreetSoccerDemo.cpp"

echo "Building with: $CXX"
echo "Sources:"
for s in $SOURCES; do echo "  $s"; done
echo

"$CXX" $CXXFLAGS $INCLUDES $SOURCES -o kimia_street_soccer

echo "OK -> ./kimia_street_soccer"
echo "Run with:  ./kimia_street_soccer --frames 600"
