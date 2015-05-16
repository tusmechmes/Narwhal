#include "Marlin.h"
#include "planner.h"
#include "temperature.h"
#include "ultralcd.h"
#include "ConfigurationStore.h"
#include "SystemInfo.h"

void _EEPROM_writeData(int &pos, uint8_t* value, uint8_t size)
{
    do
    {
        eeprom_write_byte((unsigned char*)pos, *value);
        pos++;
        value++;
    }while(--size);
}
#define EEPROM_WRITE_VAR(pos, value) _EEPROM_writeData(pos, (uint8_t*)&value, sizeof(value))
void _EEPROM_readData(int &pos, uint8_t* value, uint8_t size)
{
    do
    {
        *value = eeprom_read_byte((unsigned char*)pos);
        pos++;
        value++;
    }while(--size);
}
#define EEPROM_READ_VAR(pos, value) _EEPROM_readData(pos, (uint8_t*)&value, sizeof(value))
//======================================================================================


#define EEPROM_OFFSET 100


// IMPORTANT:  Whenever there are changes made to the variables stored in EEPROM
// in the functions below, also increment the version number. This makes sure that
// the default values are used whenever there is a change to the data, to prevent
// wrong data being written to the variables.
// ALSO:  always make sure the variables in the Store and retrieve sections are in the same order.
#define EEPROM_VERSION "V01"


//----- Save Settings -----//
#ifdef EEPROM_SETTINGS
void Config_StoreSettings() 
{
  char ver[4]= "000";
  int i=EEPROM_OFFSET;
  EEPROM_WRITE_VAR(i,ver); // invalidate data first 
  EEPROM_WRITE_VAR(i,axis_steps_per_unit); 
  EEPROM_WRITE_VAR(i,e1_steps_per_unit);
  //EEPROM_WRITE_VAR(i,extruder_offset);
  EEPROM_WRITE_VAR(i,max_feedrate);  
  EEPROM_WRITE_VAR(i,max_acceleration_units_per_sq_second);
  EEPROM_WRITE_VAR(i,acceleration);
  EEPROM_WRITE_VAR(i,retract_acceleration);
  EEPROM_WRITE_VAR(i,minimumfeedrate);
  EEPROM_WRITE_VAR(i,mintravelfeedrate);
  EEPROM_WRITE_VAR(i,minsegmenttime);
  EEPROM_WRITE_VAR(i,max_xy_jerk);
  EEPROM_WRITE_VAR(i,max_z_jerk);
  EEPROM_WRITE_VAR(i,max_e_jerk);
  EEPROM_WRITE_VAR(i,add_homeing);
  #ifdef DELTA
  EEPROM_WRITE_VAR(i,endstop_adj);
  #endif

  // write all fillament constants
  _EEPROM_writeData(i, (uint8_t*)fillamentConfig, FILLAMENT_CONFIG_ARRAY_SIZE);

  // write the extruders config
  for (int e = 0; e < MAX_EXTRUDERS; e++)
  {
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->type);
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->activeFillament);
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->nozzleDiameter);
        // PID: do not need to un-scale PID values we save the as is to the EEPROM
        #ifdef PIDTEMP
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->work_Kp);
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->work_Ki);
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->work_Kd);
        EEPROM_WRITE_VAR(i, systemInfo.Extruders[e]->work_Kc);
        #endif
  }

  EEPROM_WRITE_VAR(i,zprobe_zoffset);
  #ifndef DOGLCD
    int lcd_contrast = 32;
  #endif
  EEPROM_WRITE_VAR(i,lcd_contrast);
  char ver2[4]=EEPROM_VERSION;
  i=EEPROM_OFFSET;
  EEPROM_WRITE_VAR(i,ver2); // validate data
  SERIAL_ECHO_START;
  SERIAL_ECHOLNPGM("Settings Stored");
}
#endif //EEPROM_SETTINGS


//----- Load Settings -----//
#ifdef EEPROM_SETTINGS
void Config_RetrieveSettings()
{
    int i=EEPROM_OFFSET;
    char stored_ver[4];
    char ver[4]=EEPROM_VERSION;
    EEPROM_READ_VAR(i,stored_ver); //read stored version
    //  SERIAL_ECHOLN("Version: [" << ver << "] Stored version: [" << stored_ver << "]");
    if (strncmp(ver,stored_ver,3) == 0)
    {
        // version number match
        EEPROM_READ_VAR(i,axis_steps_per_unit);
        EEPROM_READ_VAR(i,e1_steps_per_unit);
        //EEPROM_READ_VAR(i,extruder_offset);
        EEPROM_READ_VAR(i,max_feedrate);  
        EEPROM_READ_VAR(i,max_acceleration_units_per_sq_second);
        
        // steps per sq second need to be updated to agree with the units per sq second (as they are what is used in the planner)
		reset_acceleration_rates();
        
        EEPROM_READ_VAR(i,acceleration);
        EEPROM_READ_VAR(i,retract_acceleration);
        EEPROM_READ_VAR(i,minimumfeedrate);
        EEPROM_READ_VAR(i,mintravelfeedrate);
        EEPROM_READ_VAR(i,minsegmenttime);
        EEPROM_READ_VAR(i,max_xy_jerk);
        EEPROM_READ_VAR(i,max_z_jerk);
        EEPROM_READ_VAR(i,max_e_jerk);
        EEPROM_READ_VAR(i,add_homeing);
        #ifdef DELTA
        EEPROM_READ_VAR(i,endstop_adj);
        #endif

        // read all fillament constants
        _EEPROM_readData(i, (uint8_t*)fillamentConfig, FILLAMENT_CONFIG_ARRAY_SIZE);

        // read the extruders config
        for (int e = 0; e < MAX_EXTRUDERS; e++)
        {
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->type);
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->activeFillament);
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->nozzleDiameter);
            // PID: do not need to scale PID values as the values in EEPROM are already scaled		
            #ifdef PIDTEMP
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->work_Kp);
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->work_Ki);
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->work_Kd);
            EEPROM_READ_VAR(i, systemInfo.Extruders[e]->work_Kc);
            #endif
        }

        EEPROM_READ_VAR(i,zprobe_zoffset);
        #ifndef DOGLCD
        int lcd_contrast;
        #endif
        EEPROM_READ_VAR(i,lcd_contrast);

		// Call updatePID (similar to when we have processed M301)
        SERIAL_ECHO_START;
        SERIAL_ECHOLNPGM("Stored settings retrieved");

        // refresh the system info settings
        systemInfo.Update();
    }
    else
    {
        Config_ResetDefault();
    }
    #ifdef EEPROM_CHITCHAT
      Config_PrintSettings();
    #endif
}
#endif


//----- Restore default -----//
void Config_ResetDefault()
{
    // init any needed default configurations
    // 1. fillament configurations
    configuration_init();

    // reset the system info settings to default
    systemInfo.LoadDefaults();


    float tmp1[]=DEFAULT_AXIS_STEPS_PER_UNIT;
    float tmp2[]=DEFAULT_MAX_FEEDRATE;
    long tmp3[]=DEFAULT_MAX_ACCELERATION;
    for (short i=0;i<4;i++) 
    {
        axis_steps_per_unit[i]=tmp1[i];  
        max_feedrate[i]=tmp2[i];  
        max_acceleration_units_per_sq_second[i]=tmp3[i];
    }
    
    // steps per sq second need to be updated to agree with the units per sq second
    reset_acceleration_rates();
    
    e1_steps_per_unit=DEFAULT_E1_STEPS_PER_UNIT;
    
    // Extruder offset
    //for(short i=0; i<2; i++)
    //{
        //float e_offset_x[] = EXTRUDER_OFFSET_X;
        //float e_offset_y[] = EXTRUDER_OFFSET_Y;
        
        //extruder_offset[0][i]=e_offset_x[i];
        //extruder_offset[1][i]=e_offset_y[i];
    //}

    acceleration=DEFAULT_ACCELERATION;
    retract_acceleration=DEFAULT_RETRACT_ACCELERATION;
    minimumfeedrate=DEFAULT_MINIMUMFEEDRATE;
    minsegmenttime=DEFAULT_MINSEGMENTTIME;       
    mintravelfeedrate=DEFAULT_MINTRAVELFEEDRATE;
    max_xy_jerk=DEFAULT_XYJERK;
    max_z_jerk=DEFAULT_ZJERK;
    max_e_jerk=DEFAULT_EJERK;
    add_homeing[0] = add_homeing[1] = add_homeing[2] = 0;
#ifdef DELTA
    endstop_adj[0] = endstop_adj[1] = endstop_adj[2] = 0;
#endif
#ifdef ENABLE_AUTO_BED_LEVELING
    zprobe_zoffset = -Z_PROBE_OFFSET_FROM_EXTRUDER;
#endif
#ifdef DOGLCD
    lcd_contrast = DEFAULT_LCD_CONTRAST;
#endif

SERIAL_ECHO_START;
SERIAL_ECHOLNPGM("Hardcoded Default Settings Loaded");

}


//----- Print Settings -----//
#ifndef DISABLE_M503
void Config_PrintSettings()
{  
    // Always have this function, even with EEPROM_SETTINGS disabled, the current values will be shown
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Steps per unit:");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M92 X", axis_steps_per_unit[0]);
    SERIAL_ECHOPAIR(" Y", axis_steps_per_unit[1]);
    SERIAL_ECHOPAIR(" Z", axis_steps_per_unit[2]);
    SERIAL_ECHOPAIR(" E", axis_steps_per_unit[3]);
    SERIAL_ECHOPAIR(" E1", e1_steps_per_unit);
    SERIAL_ECHOLN("");

    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Maximum feedrates (mm/s):");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M203 X", max_feedrate[0]);
    SERIAL_ECHOPAIR(" Y", max_feedrate[1]);
    SERIAL_ECHOPAIR(" Z", max_feedrate[2]);
    SERIAL_ECHOPAIR(" E", max_feedrate[3]);
    SERIAL_ECHOLN("");

    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Maximum Acceleration (mm/s2):");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M201 X", max_acceleration_units_per_sq_second[0]);
    SERIAL_ECHOPAIR(" Y", max_acceleration_units_per_sq_second[1]);
    SERIAL_ECHOPAIR(" Z", max_acceleration_units_per_sq_second[2]);
    SERIAL_ECHOPAIR(" E", max_acceleration_units_per_sq_second[3]);
    SERIAL_ECHOLN("");
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Acceleration: S=acceleration, T=retract acceleration");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M204 S", acceleration);
    SERIAL_ECHOPAIR(" T", retract_acceleration);
    SERIAL_ECHOLN("");

    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Advanced variables: S=Min feedrate (mm/s), T=Min travel feedrate (mm/s), B=minimum segment time (ms), X=maximum XY jerk (mm/s),  Z=maximum Z jerk (mm/s),  E=maximum E jerk (mm/s)");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M205 S", minimumfeedrate);
    SERIAL_ECHOPAIR(" T", mintravelfeedrate);
    SERIAL_ECHOPAIR(" B", minsegmenttime);
    SERIAL_ECHOPAIR(" X", max_xy_jerk);
    SERIAL_ECHOPAIR(" Z", max_z_jerk);
    SERIAL_ECHOPAIR(" E", max_e_jerk);
    SERIAL_ECHOLN("");

    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Home offset (mm):");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M206 X", add_homeing[0]);
    SERIAL_ECHOPAIR(" Y", add_homeing[1]);
    SERIAL_ECHOPAIR(" Z", add_homeing[2]);
    SERIAL_ECHOLN("");
#ifdef DELTA
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("Endstop adjustement (mm):");
    SERIAL_ECHO_START;
    SERIAL_ECHOPAIR("  M666 X", endstop_adj[0]);
    SERIAL_ECHOPAIR(" Y", endstop_adj[1]);
    SERIAL_ECHOPAIR(" Z", endstop_adj[2]);
    SERIAL_ECHOLN("");
#endif
#ifdef PIDTEMP
    SERIAL_ECHO_START;
    SERIAL_ECHOLNPGM("PID settings:");
    SERIAL_ECHO_START;
    // HACK - report only first extruder, since M301 can't handle different values for different exruder #
    SERIAL_ECHOPAIR("   M301 P", WORK_Kp(0));
    SERIAL_ECHOPAIR(" I", unscalePID_i(WORK_Ki(0)));
    SERIAL_ECHOPAIR(" D", unscalePID_d(WORK_Kd(0)));
    SERIAL_ECHOLN("");
#endif
}
#endif
