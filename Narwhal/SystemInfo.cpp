// This file contains the Printer HW setup including Extruders, Fans, Bed, Sensors etc.

#include "Configuration.h"
#include "language.h"
#include "SystemInfo.h"
#include "temperature.h"

const char *fillament_tostr(const int &x);
const char *fillament_tostr_short(const int &x);
const char *extruder_tostr(const int &x);
void digipot_current(uint8_t driver, int current);

extern float homing_feedrate[];
extern float max_feedrate[4]; // set the max speeds
//extern float extruder_offset[NUM_EXTRUDER_OFFSETS][MAX_EXTRUDERS];
extern bool isInitialized;


// ExtruderInfo implementation
ExtruderInfo::ExtruderInfo(uint8_t type)
{
    this->type = type;
    LoadDefaults();
}

// Helper string to represent the extruder name and fillament: Extruder (Fill)
// Name[4] + ' ('[2] + fillament[5] + ')'[1] + 0[1]   example: "Buda (ABS)"
char tostring[13] = { 0 };

const char* ExtruderInfo::ToString()
{
    if (this->type == EXTRUDER_TYPE_NOT_INSTALLED)
        return MSG_EXTRUDER_NOTINSTALLED;

    const char* name = extruder_tostr(this->type);
    const char* fillament = fillament_tostr_short(this->activeFillament);
    tostring[0] = name[0];
    tostring[1] = name[1];
    tostring[2] = name[2];
    tostring[3] = name[3];
    tostring[4] = ' ';
    tostring[5] = '(';
    uint8_t n = 0;
    while (fillament[n] != 0)
        tostring[6 + n] = fillament[n++];
    tostring[6 + n] = ')';
    tostring[6 + n + 1] = 0;
    return tostring;
}

// call this every time the type of this extruder changes to reaload all the relevant settings
void ExtruderInfo::Update()
{
    // update all PID values
    updatePID();

    switch (this->type)
    {
        // Buda 2.0 on 24V
    case EXTRUDER_TYPE_BUDASCHNOZZLE:
        // point the current fillament values to the right place
        this->pFillamentHotEndTemp = ((int*)&fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][this->activeFillament][ID_HOTEND_TEMP]);
        this->pFillamentHPBTemp = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][this->activeFillament][ID_HPB_TEMP];
        this->pFillamentFanSpeed = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][this->activeFillament][ID_FAN_SPEED];
        this->pFillamentInfo = &fillamentConfig[EXTRUDER_TYPE_BUDASCHNOZZLE][this->activeFillament][ID_INFO];
        break;

        // AO-Hexagon (24V)
    case EXTRUDER_TYPE_HEXAGON:
        // point the current fillament values to the right place
        this->pFillamentHotEndTemp = ((int*)&fillamentConfig[EXTRUDER_TYPE_HEXAGON][this->activeFillament][ID_HOTEND_TEMP]);
        this->pFillamentHPBTemp = &fillamentConfig[EXTRUDER_TYPE_HEXAGON][this->activeFillament][ID_HPB_TEMP];
        this->pFillamentFanSpeed = &fillamentConfig[EXTRUDER_TYPE_HEXAGON][this->activeFillament][ID_FAN_SPEED];
        this->pFillamentInfo = &fillamentConfig[EXTRUDER_TYPE_HEXAGON][this->activeFillament][ID_INFO];
        break;

        // BUGBUG - need the actual settings of the flexystruder, at the moment it is an exact copy of the budaschnozzle
    case EXTRUDER_TYPE_FLEXYSTRUDER:
        // point the current fillament values to the right place
        this->pFillamentHotEndTemp = ((int*)&fillamentConfig[EXTRUDER_TYPE_FLEXYSTRUDER][this->activeFillament][ID_HOTEND_TEMP]);
        this->pFillamentHPBTemp = &fillamentConfig[EXTRUDER_TYPE_FLEXYSTRUDER][this->activeFillament][ID_HPB_TEMP];
        this->pFillamentFanSpeed = &fillamentConfig[EXTRUDER_TYPE_FLEXYSTRUDER][this->activeFillament][ID_FAN_SPEED];
        this->pFillamentInfo = &fillamentConfig[EXTRUDER_TYPE_FLEXYSTRUDER][this->activeFillament][ID_INFO];
        break;
    }
}

// call this to reset the settings to the currently selected extruder type
void ExtruderInfo::LoadDefaults()
{
    switch (this->type)
    {
        // Buda 2.0 on 24V
    case EXTRUDER_TYPE_BUDASCHNOZZLE:
        //----- Defaults -----//
        this->heater_MaxTemp = EXTRUDER_BUDASCHNOZZLE_MAX_TEMP;
        this->bang_Max = 70;    // limits current to nozzle while in bang-bang mode; 255=full current
        this->PID_Max = 74;     // limits current to nozzle while PID is active (see PID_FUNCTIONAL_RANGE below); 255=full current
#ifdef PIDTEMP
        this->PID_Integral_Drive_Max = 70;
        this->default_Kp = 6.0;
        this->default_Ki = 0.3;
        this->default_Kd = 125.0;
#endif
        this->default_Max_E_FeedRate = 50;      // (mm/sec)

        //----- Dynamic settings -----//
        this->nozzleDiameter = INFO_NOZZLE_0_35;
        this->stepsPerUnit = DEFAULT_EXTRUDER_STEPS_PER_UNIT;
        this->activeFillament = FILLAMENT_ABS;
#ifdef PIDTEMP
        this->work_Kp = default_Kp;
        this->work_Ki = default_Ki;
        this->work_Kd = default_Kd;
        this->work_Kc = 1.0;
#endif
        break;

        // AO-Hexagon (24V)
    case EXTRUDER_TYPE_HEXAGON:
        //----- Defaults -----//
        this->heater_MaxTemp = EXTRUDER_HEXAGON_MAX_TEMP;
        this->bang_Max = 220;
        this->PID_Max = 225;
#ifdef PIDTEMP
        this->PID_Integral_Drive_Max = 220;
        this->default_Kp = 28.79;
        this->default_Ki = 1.91;
        this->default_Kd = 108.51;
#endif
        this->default_Max_E_FeedRate = 40;

        //----- Dynamic settings -----//
        this->nozzleDiameter = INFO_NOZZLE_0_35;
        this->stepsPerUnit = DEFAULT_EXTRUDER_STEPS_PER_UNIT;
        this->activeFillament = FILLAMENT_ABS;
#ifdef PIDTEMP
        this->work_Kp = default_Kp;
        this->work_Ki = default_Ki;
        this->work_Kd = default_Kd;
        this->work_Kc = 1.0;
#endif
        break;

        // BUGBUG - need the actual settings of the flexystruder, at the moment it is an exact copy of the budaschnozzle
    case EXTRUDER_TYPE_FLEXYSTRUDER:
        //----- Defaults -----//
        this->heater_MaxTemp = EXTRUDER_FLEXYSTRUDER_MAX_TEMP;
        this->bang_Max = 70;    // limits current to nozzle while in bang-bang mode; 255=full current
        this->PID_Max = 74;     // limits current to nozzle while PID is active (see PID_FUNCTIONAL_RANGE below); 255=full current
#ifdef PIDTEMP
        this->PID_Integral_Drive_Max = 70;
        this->default_Kp = 6;
        this->default_Ki = 0.3;
        this->default_Kd = 125;
#endif
        this->default_Max_E_FeedRate = 50;      // (mm/sec)

        //----- Dynamic settings -----//
        this->nozzleDiameter = INFO_NOZZLE_0_50;
        this->stepsPerUnit = DEFAULT_EXTRUDER_STEPS_PER_UNIT;
        this->activeFillament = FILLAMENT_FLEXIBLE;
#ifdef PIDTEMP
        this->work_Kp = default_Kp;
        this->work_Ki = default_Ki;
        this->work_Kd = default_Kd;
        this->work_Kc = 1.0;
#endif
        break;
    }

    // lastly, update the fillament settings
    this->Update();
}

SystemInfo::SystemInfo()
{
    Extruders[0] = new ExtruderInfo(EXTRUDER_TYPE_BUDASCHNOZZLE);
#if MAX_EXTRUDERS > 1
    Extruders[1] = new ExtruderInfo(EXTRUDER_TYPE_NOT_INSTALLED);
#endif
#if MAX_EXTRUDERS > 2
    Extruders[2] = new ExtruderInfo(EXTRUDER_TYPE_NOT_INSTALLED);
#endif
}

void SystemInfo::LoadDefaults()
{
    for (int i = 0; i < MAX_EXTRUDERS; i++)
    {
        // refresh the exturder settings
        if (this->Extruders[i] != NULL)
        {
            this->Extruders[i]->LoadDefaults();
        }
    }
    
    // lastly, update the syste (this will cause a second update on the extruders, oh well)
    this->Update();
}

void SystemInfo::Update()
{
    // refresh extruders
    this->installedExtruders = 0;
    for (int i = 0; i < MAX_EXTRUDERS; i++)
    {
        if (this->Extruders[i] != NULL)
        {
            // refresh the exturder settings
            this->Extruders[i]->Update();

            // increment the numbe of installed extruders (+1 for each installed extuder)
            if (this->Extruders[i]->type != EXTRUDER_TYPE_NOT_INSTALLED)
                this->installedExtruders++;
        }
    }

    if (this->installedExtruders == 1)  // Single Extruder Configuration
    {
        //----- Configuration -----//
        si_DISABLE_E = false; // For all extruders

        //#define HOMING_FEEDRATE {50*60, 50*60, 8*60, 0}  // set the homing speeds (mm/min)
        homing_feedrate[2] = 8 * 60;

        //#define DEFAULT_MAX_FEEDRATE          {800, 800, 8, 50}      // (mm/sec)
        max_feedrate[2] = 8;

        ////#define EXTRUDER_OFFSET_X {0.0, 0.00} // (in mm) for each extruder, offset of the hotend on the X axis
        //extruder_offset[0][0] = 0.0;
        //extruder_offset[0][1] = 0.0;
        ////#define EXTRUDER_OFFSET_Y {0.0, -52.00}  // (in mm) for each extruder, offset of the hotend on the Y axis
        //extruder_offset[1][0] = 0.0;
        //extruder_offset[1][1] = 0.0;

        si_FAST_PWM_FAN = true;
        si_FAN_SOFT_PWM = false;


        //----- Configuration_adv: -----//
        //#define CASEFAN_SECS 60 //How many seconds, after all motors were disabled, the fan should run before going back to idle speed.
        //#define CASEFAN_SPEED_FULL 130  // Full speed for when motor are active
        //#define CASEFAN_SPEED_IDLE 0  	// Idle speed for when the motor have been inactive	
        //#define CASEFAN_SPEED_MAX 255  	// Maximum limit for the fan speed so it does not burn out. Use 128 for 12v fans with 24V Power Supplies
        //#define CASEFAN_SPEED_MIN 0	// Minimum limit for the fan speed where it will start to spin from a stop without a push.
        si_FAN_KICKSTART_TIME = 0;
        
        //#define DIGIPOT_MOTOR_CURRENT {175,175,240,135,135} // Values 0-255 (RAMBO 135 = ~0.75A, 185 = ~1A)
        // we can update the current settings only after we initialized and enabled all interrupts and SPI.begin()
        if (isInitialized)
        {
            digipot_current(0, 175);
            digipot_current(1, 175);
            digipot_current(2, 240);
            digipot_current(3, 135);
            digipot_current(4, 135);
        }
    }
    else // Dual Exturer Configuration
    {
        //----- Configuration -----//
        si_DISABLE_E = true; // For all extruders

        //#define HOMING_FEEDRATE {50*60, 50*60, 4*60, 0}  // set the homing speeds (mm/min)
        homing_feedrate[2] = 4 * 60;

        //#define DEFAULT_MAX_FEEDRATE          {800, 800, 3, 50}      // (mm/sec)
        max_feedrate[2] = 3;

        //#define EXTRUDER_OFFSET_X {0.0, 0.00} // (in mm) for each extruder, offset of the hotend on the X axis
        //#define EXTRUDER_OFFSET_Y {0.0, 0.00}  // (in mm) for each extruder, offset of the hotend on the Y axis


        si_FAST_PWM_FAN = false;
        si_FAN_SOFT_PWM = true;


        //----- Configuration_adv: -----//
        //#define CASEFAN_SECS 15 //How many seconds, after all motors were disabled, the fan should run before going back to idle speed.
        //#define CASEFAN_SPEED_FULL 100  // Full speed for when motor are active
        //#define CASEFAN_SPEED_IDLE 70  	// Idle speed for when the motor have been inactive	
        //#define CASEFAN_SPEED_MAX 128  	// Maximum limit for the fan speed so it does not burn out. Use 128 for 12v fans with 24V Power Supplies
        //#define CASEFAN_SPEED_MIN 70	// Minimum limit for the fan speed where it will start to spin from a stop without a push.
        si_FAN_KICKSTART_TIME = 100;

        //#define DIGIPOT_MOTOR_CURRENT {175,175,220,160,160} // Values 0-255 (RAMBO 135 = ~0.75A, 185 = ~1A)
        // we can update the current settings only after we initialized and enabled all interrupts and SPI.begin()
        if (isInitialized)
        {
            digipot_current(0, 175);
            digipot_current(1, 175);
            digipot_current(2, 220);
            digipot_current(3, 160);
            digipot_current(4, 160);
        }
    }
}
SystemInfo systemInfo;

