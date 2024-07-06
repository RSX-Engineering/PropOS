#ifndef SOUND_SMOOTH_SWING_CONFIG_H
#define SOUND_SMOOTH_SWING_CONFIG_H

class SmoothSwingConfigFile : public ConfigFile {
public:
  void iterateVariables(VariableOP *op) override {
    CONFIG_VARIABLE2(Version, 2);
    CONFIG_VARIABLE2(SwingSensitivity, 450.0f);
    CONFIG_VARIABLE2(MaximumHumDucking, 75.0f);
    CONFIG_VARIABLE2(SwingSharpness, 1.75f);
    CONFIG_VARIABLE2(SwingStrengthThreshold, 20.0f);
    CONFIG_VARIABLE2(Transition1Degrees, 45.0f);
    CONFIG_VARIABLE2(Transition2Degrees, 160.0f);
    CONFIG_VARIABLE2(MaxSwingVolume, 3.0f);
    CONFIG_VARIABLE2(AccentSwingSpeedThreshold, 0.0f);
    CONFIG_VARIABLE2(AccentSlashAccelerationThreshold, 260.0f);
  };

  int  Version;
  float SwingSensitivity;
  float MaximumHumDucking;
  float SwingSharpness;
  float SwingStrengthThreshold;
  float Transition1Degrees;
  float Transition2Degrees;
  float MaxSwingVolume;
  float AccentSwingSpeedThreshold;
  float AccentSlashAccelerationThreshold;


  // Apply swing sensitivity to smooth swing parameters. 
  // Call ApplySensitivity(0) once to store unscaled values!
  void ApplySensitivity(SwSensitivity* sensitivity) {
      static float SwingSensitivity_ = 0;
      static float SwingSharpness_ = 0;
      static float AccentSwingSpeedThreshold_ = 0;
      static float MaxSwingVolume_ = 0;

      if (!sensitivity) { // just store current values, assummed unscaled
        SwingSensitivity_ = SwingSensitivity; // STDOUT.print("[smooth_swing_config] Stored SwingSensitivity = "); STDOUT.println(SwingSensitivity_);
        SwingSharpness_ = SwingSharpness; // STDOUT.print("[smooth_swing_config] Stored SwingSharpness = "); STDOUT.println(SwingSharpness_);
        AccentSwingSpeedThreshold_ = AccentSwingSpeedThreshold; // STDOUT.print("[smooth_swing_config] Stored AccentSwingSpeedThreshold = "); STDOUT.println(AccentSwingSpeedThreshold_);
        MaxSwingVolume_ = MaxSwingVolume; // STDOUT.print("[smooth_swing_config] Stored MaxSwingVolume = "); STDOUT.println(MaxSwingVolume_);

      }
      else { // scale stored parameters to get new current values
        SwingSensitivity = SwingSensitivity_ * sensitivity->SwingSensitivity_multiplier; // STDOUT.print("[smooth_swing_config] Applied SwingSensitivity = "); STDOUT.println(SwingSensitivity);
        SwingSharpness = SwingSharpness_ * sensitivity->SwingSharpness_multiplier; // STDOUT.print("[smooth_swing_config] Applied SwingSharpness = "); STDOUT.println(SwingSharpness);
        AccentSwingSpeedThreshold = AccentSwingSpeedThreshold_ * sensitivity->AccentSwingSpeedThreshold_multiplier; // STDOUT.print("[smooth_swing_config] Applied AccentSwingSpeedThreshold = "); STDOUT.println(AccentSwingSpeedThreshold);
        MaxSwingVolume = MaxSwingVolume_ * sensitivity->MaxSwingVolume_multiplier; // STDOUT.print("[smooth_swing_config] Applied MaxSwingVolume = "); STDOUT.println(MaxSwingVolume);

      }

  }


};

SmoothSwingConfigFile smooth_swing_config;

#endif
