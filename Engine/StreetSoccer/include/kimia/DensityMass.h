#pragma once
// =============================================================================
//  DensityMass — derive physics mass from real-world material density.
//
//  Required by phase 1: "سنگینی وزن با استفاده از ابعاد اجسام".
//
//  In football, the ball mass matters (FIFA size 5 ≈ 0.43 kg), and heavier
//  dummies on the street (کیسه آرد 25 کیلویی) feel completely different
//  from a plastic bottle. Rather than hard-coding masses, we compute
//  mass = density × volume.
//
//  Examples:
//    Plastic ball  ρ=950 kg/m³ r=0.11 m  -> ~5.3 kg (hollow water-ball)
//    Football ball ρ=270 kg/m³ r=0.11 m  -> 1.5 kg (regulation-like)
//    Tin water-bottle ρ=9000 kg/m³ r=0.03 m -> 1.0 kg
//    Cardboard box ρ=80 kg/m³ halfExt=0.15 -> ~0.43 kg
//    Kids' crate ρ=600 kg/m³ halfExt=0.20 -> 3.8 kg
//    Steel bollard ρ=7850 kg/m³ r=0.07 m -> 5.6 kg (the "post")
//
//  Pure helper functions. Volume formulas assume idealised geometry.
// =============================================================================

#include <kimia/Types.h>
#include <kimia/Vec.h>

namespace kimia::street {

// Material densities (kg/m³). Approximate real-world values.
struct Material {
  static constexpr f64 Plastic     = 950.0;    // cheap toy balls
  static constexpr f64 Football    = 270.0;    // regulation 5-a-side
  static constexpr f64 Tin         = 9000.0;   // cans/bollards
  static constexpr f64 Cardboard   = 80.0;
  static constexpr f64 Steel       = 7850.0;
  static constexpr f64 Concrete    = 2300.0;
  static constexpr f64 Foam        = 35.0;
  static constexpr f64 Wood        = 700.0;
  static constexpr f64 Air         = 1.225;
  static constexpr f64 HumanBody   = 985.0;    // average adult
  static constexpr f64 KidBody     = 950.0;
  static constexpr f64 StreetSlipper = 700.0; // rubber sole
};

// Volume of a sphere (m³) given radius (m).
inline constexpr f64 sphereVolume(f64 radius) {
  // V = 4/3 π r³
  return (4.0 / 3.0) * 3.14159265358979323846 * radius * radius * radius;
}

// Volume of an axis-aligned box (m³) given half extents (m).
inline constexpr f64 boxVolume(const Vec3& halfExtents) {
  return 8.0 * halfExtents.x * halfExtents.y * halfExtents.z;
}

// Compute mass (kg) from density (kg/m³) and volume (m³).
inline constexpr f64 massFromDensity(f64 density, f64 volume) {
  return density * volume;
}

// Convenience for a sphere: density × volume.
inline f64 sphereMass(f64 density, f64 radius) {
  return massFromDensity(density, sphereVolume(radius));
}

// Convenience for a box.
inline f64 boxMass(f64 density, const Vec3& halfExtents) {
  return massFromDensity(density, boxVolume(halfExtents));
}

// Suggested:
inline f64 footballBallMass()   { return sphereMass(Material::Football, 0.11); }  // ≈1.5 kg
inline f64 tinBottleMass()      { return sphereMass(Material::Tin,      0.03); }  // ≈1.0 kg
inline f64 steelBollardMass()   { return sphereMass(Material::Steel,    0.07); }  // ≈5.6 kg
inline f64 kidMassApprox()      { return boxMass(Material::KidBody, Vec3(0.18, 0.40, 0.10)); }  // torso

}  // namespace kimia::street
