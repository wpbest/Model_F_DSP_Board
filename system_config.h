// system_config.h

#ifndef _system_config_h_ // inclusion guard
#define _system_config_h_

#include "F2806x_90MHZBOOT.h"

#define VERSION ("Base Version 1.0.7")
#define VERSION_DATE ("01-04-2019")

#define CONFIG_FA00342_2KVA_1_4KW_5BAT "system_config_FA00342_2000VA_1400W_120in_120out_5bat.h"
#define CONFIG_FA00350_3KVA_2_7KW_8BAT "system_config_FA00350_3000VA_2700W_115in_115out_8bat.h"
#define CONFIG_FA00365_1_5KVA_1_35KW_4BAT "system_config_FA00365_1500VA_1350W_115in_115out_4bat.h"
#define CONFIG_FA00405_2_5KVA_2KW_7BAT "system_config_FA00405_2500VA_2KW_50-60Hz-WRPFC_in_120_out_7bat.h"

// uncomment only one for the desired model

#define SYSTEM_CONFIG CONFIG_FA00342_2KVA_1_4KW_5BAT
//#define SYSTEM_CONFIG CONFIG_FA00350_3KVA_2_7KW_8BAT
//#define SYSTEM_CONFIG CONFIG_FA00365_1_5KVA_1_35KW_4BAT
//#define SYSTEM_CONFIG CONFIG_FA00405_2_5KVA_2KW_7BAT

// While testing initially, flash option LED so we can tell if micro-board is old or new DSP
//#define FLASH_OPTION_LED_FOR_DSP
#define DSP_INTERFACE
#endif // inclusion guard
