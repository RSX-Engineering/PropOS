#ifndef COMMON_BLADE_CONFIG_H
#define COMMON_BLADE_CONFIG_H

// BLADE ITERATORS
// Only installed blades are running, so we need two iterators: one for all compile-supported blades (ONCEPERALLBLADES) and one for runtime-installed blades (ONCEPERBLADE)
  #if NUM_BLADES == 0
  #define ONCEPERBLADE(F)
  #elif NUM_BLADES == 1
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) } 
  #elif NUM_BLADES == 2
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) }    if (current_config->blade2) { F(2) }
  #elif NUM_BLADES == 3
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) }    if (current_config->blade2) { F(2) }    if (current_config->blade3) { F(3) }
  #elif NUM_BLADES == 4
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) }    if (current_config->blade2) { F(2) }    if (current_config->blade3) { F(3) }  if (current_config->blade4) { F(4) }
  #elif NUM_BLADES == 5
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) }    if (current_config->blade2) { F(2) }    if (current_config->blade3) { F(3) }  if (current_config->blade4) { F(4) }  if (current_config->blade5) { F(5) }
  #elif NUM_BLADES == 6
  #define ONCEPERBLADE(F) if (current_config->blade1) { F(1) }    if (current_config->blade2) { F(2) }    if (current_config->blade3) { F(3) }  if (current_config->blade4) { F(4) }  if (current_config->blade5) { F(5) }  if (current_config->blade6) { F(6) }
  #else
  #error NUM_BLADES is too big
  #endif

  #if NUM_BLADES == 0
  #define ONCEPERSUPPORTEDBLADE(F)
  #elif NUM_BLADES == 1
  #define ONCEPERSUPPORTEDBLADE(F) F(1)
  #elif NUM_BLADES == 2
  #define ONCEPERSUPPORTEDBLADE(F) F(1) F(2)
  #elif NUM_BLADES == 3
  #define ONCEPERSUPPORTEDBLADE(F) F(1) F(2) F(3)
  #elif NUM_BLADES == 4
  #define ONCEPERSUPPORTEDBLADE(F) F(1) F(2) F(3) F(4)
  #elif NUM_BLADES == 5
  #define ONCEPERSUPPORTEDBLADE(F) F(1) F(2) F(3) F(4) F(5)
  #elif NUM_BLADES == 6
  #define ONCEPERSUPPORTEDBLADE(F) F(1) F(2) F(3) F(4) F(5) F(6)
  #else
  #error NUM_BLADES is too big
  #endif
  // declarator of NUM_BLADES null blades (pre-install)
  #if NUM_BLADES == 0
  #define DECLARE_NULL_BLADES 
  #elif NUM_BLADES == 1
  #define DECLARE_NULL_BLADES 0
  #elif NUM_BLADES == 2
  #define DECLARE_NULL_BLADES 0, 0
  #elif NUM_BLADES == 3
  #define DECLARE_NULL_BLADES 0, 0, 0
  #elif NUM_BLADES == 4
  #define DECLARE_NULL_BLADES 0, 0, 0, 0
  #elif NUM_BLADES == 5
  #define DECLARE_NULL_BLADES 0, 0, 0, 0, 0
  #elif NUM_BLADES == 6
  #define DECLARE_NULL_BLADES 0, 0, 0, 0, 0, 0
  #else
  #error NUM_BLADES is too big
  #endif


class BladeBase;

struct BladeConfig {
  // Blade identifier resistor.
  int ohm;

  // Blade driver.
#define DEFINE_BLADES(N) BladeBase* blade##N;
    ONCEPERSUPPORTEDBLADE(DEFINE_BLADES);
    vector<Preset>* presets;


  const char* save_dir;
};

extern BladeConfig* current_config;


#endif
