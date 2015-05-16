#ifndef __MACHINEINFO_H__
#define __MACHINEINFO_H__

// This file contains the Printer HW setup including Extruders, Fans, Bed, Sensors etc.

#include "Configuration.h"


// Extruder Type - defaults to Buda
class ExtruderInfo
{
//----- dynamic settings: those need to be stored on the EEPROM if they are to persist -----//
public:
    // defaults to Budaschnozzle to avoid catastrophic issues when instantiating (wrongly) this base class and using it
    char type = EXTRUDER_TYPE_BUDASCHNOZZLE;

    // The currently installed fillament
    unsigned char activeFillament = FILLAMENT_ABS;

    // the currently installed nozzle
    unsigned char nozzleDiameter = INFO_NOZZLE_0_35;    // can't edit this one yet

#ifdef PIDTEMP
    // current settings for PID
    float work_Kp = 6;
    float work_Ki = 0.3;
    float work_Kd = 125.0;
    float work_Kc = 1.0;
#endif

//----- Auto-set variables (according to types): No need to store on the EEPROM -----//
public:
    short heater_MaxTemp = EXTRUDER_BUDASCHNOZZLE_MAX_TEMP;
    uint8_t bang_Max = 70;
    uint8_t PID_Max = 74;
#ifdef PIDTEMP
    uint8_t PID_Integral_Drive_Max = 70;
    float default_Kp = 6.0;
    float default_Ki = 0.3;
    float default_Kd = 125.0;
#endif
    uint8_t default_Max_E_FeedRate = 50;      // (mm/sec)

    // int16_t and int - are those the same here?
    int *pFillamentHotEndTemp = ((int*)&fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][FILLAMENT_ABS][ID_HOTEND_TEMP]);
    unsigned char *pFillamentHPBTemp = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][FILLAMENT_ABS][ID_HPB_TEMP];
    unsigned char *pFillamentFanSpeed = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][FILLAMENT_ABS][ID_FAN_SPEED];
    unsigned char *pFillamentInfo = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][FILLAMENT_ABS][ID_INFO];

//----- Functions -----//
public:
    ExtruderInfo(uint8_t type);
    const char* ToString();

    void LoadDefaults();
    void Update();
};


//===== System Info =====//
class SystemInfo
{
//----- Properties -----//
public:
    int installedExtruders = 1;

    ExtruderInfo* Extruders[MAX_EXTRUDERS];

public:
    // Configuration changes according to # of extruders
    // defaults to single extruder
    bool si_FAST_PWM_FAN = true;
    bool si_FAN_SOFT_PWM = false;
    bool si_DISABLE_E = false;
    int si_FAN_KICKSTART_TIME = 0;

//----- Methods -----//
public:
    SystemInfo();

    void LoadDefaults();
    void Update();
};
extern SystemInfo systemInfo;


#endif // __MACHINEINFO_H__
