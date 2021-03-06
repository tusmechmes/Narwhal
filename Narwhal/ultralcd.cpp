#include "temperature.h"
#include "ultralcd.h"
#ifdef ULTRA_LCD
#include "Marlin.h"
#include "language.h"
#include "cardreader.h"
#include "temperature.h"
#include "stepper.h"
#include "ConfigurationStore.h"
#include "SystemInfo.h"

int8_t encoderDiff; /* encoderDiff is updated from interrupt context and added to encoderPosition every LCD update */

/* Configuration settings */


#ifdef ULTIPANEL
static float manual_feedrate[] = MANUAL_FEEDRATE;
#endif // ULTIPANEL

/* !Configuration settings */

//Function pointer to menu functions.
typedef void(*menuFunc_t)();
void do_nothing() {}

uint8_t lcd_status_message_level;
char lcd_status_message[LCD_WIDTH + 1] = WELCOME_MSG;

#ifdef DOGLCD
#include "dogm_lcd_implementation.h"
#else
#include "ultralcd_implementation_hitachi_HD44780.h"
#endif

/** forward declerations **/

void copy_and_scalePID_i();
void copy_and_scalePID_d();
void refresh_system_settings();
void send_command_T0();

/* Different menus */
static void lcd_status_screen();
#ifdef ULTIPANEL
extern bool powersupply;
static void lcd_main_menu();
static void lcd_tune_menu();
static void lcd_prepare_menu();
static void lcd_move_menu();
static void lcd_control_menu();
static void lcd_control_temperature_menu();
static void lcd_control_temperature_filament_settings_menu();
static void lcd_control_temperature_filament_settings_values_menu();
static void lcd_control_temperature_preheat_abs_settings_menu();
static void lcd_control_motion_menu();

// System Setup menu
static void lcd_system_menu();
static void lcd_system_extruder();

#ifdef DOGLCD
static void lcd_set_contrast();
#endif
static void lcd_control_retract_menu();
static void lcd_sdcard_menu();

static void lcd_quick_feedback();//Cause an LCD refresh, and give the user visual or audiable feedback that something has happend



/** Used variables to keep track of the menu */
#ifndef REPRAPWORLD_KEYPAD
volatile uint8_t buttons;//Contains the bits of the currently pressed buttons.
#else
volatile uint8_t buttons_reprapworld_keypad; // to store the reprapworld_keypad shiftregister values
#endif
#ifdef LCD_HAS_SLOW_BUTTONS
volatile uint8_t slow_buttons;//Contains the bits of the currently pressed buttons.
#endif
uint8_t currentMenuViewOffset;              /* scroll offset in the current menu */
uint32_t blocking_enc;
uint8_t lastEncoderBits;
int encoderPosition;
#if (SDCARDDETECT > 0)
bool lcd_oldcardstatus;
#endif
#endif//ULTIPANEL

menuFunc_t currentMenu = lcd_status_screen; // function pointer to the currently active menu
int currentMenu_param1 = 0;
int currentMenu_param2 = 0;
char* currentMenu_filename = NULL;

uint32_t lcd_next_update_millis;
uint8_t lcd_status_update_delay;
uint8_t lcdDrawUpdate = 2;                  // Set to none-zero when the LCD needs to draw, decreased after every draw. Set to 2 in LCD routines so the LCD gets atleast 1 full redraw (first redraw is partial)

float raw_Ki, raw_Kd;   // placeholders for Ki and Kd edits
uint8_t extruderId;     // currently edited extruder id


#define ENCODER_FEEDRATE_DEADZONE 10

#if !defined(LCD_I2C_VIKI)
#ifndef ENCODER_STEPS_PER_MENU_ITEM
#define ENCODER_STEPS_PER_MENU_ITEM 5
#endif
#ifndef ENCODER_PULSES_PER_STEP
#define ENCODER_PULSES_PER_STEP 1
#endif
#else
#ifndef ENCODER_STEPS_PER_MENU_ITEM
#define ENCODER_STEPS_PER_MENU_ITEM 2 // VIKI LCD rotary encoder uses a different number of steps per rotation
#endif
#ifndef ENCODER_PULSES_PER_STEP
#define ENCODER_PULSES_PER_STEP 1
#endif
#endif



#define IS_INLINE_EDIT              (inlineEditMenu != NULL)
#define RELEVANT_ENCODER_POSITION   (IS_INLINE_EDIT ? savedEncoderPosition : encoderPosition)
#define IS_MENU_SELECTED            ((RELEVANT_ENCODER_POSITION / ENCODER_STEPS_PER_MENU_ITEM) == _menuItemIndex)

/* Helper macros for menus */
#define START_MENU() do { \
    if (!IS_INLINE_EDIT) \
    {\
        if (encoderPosition > 0x8000) \
            encoderPosition = 0; \
    }\
    if (RELEVANT_ENCODER_POSITION / ENCODER_STEPS_PER_MENU_ITEM < currentMenuViewOffset) \
        currentMenuViewOffset = RELEVANT_ENCODER_POSITION / ENCODER_STEPS_PER_MENU_ITEM; \
    uint8_t _menuItemIndex = 0;

#define LCD_DRAW_MENU(type, row, pstrText, args...)             lcd_implementation_drawmenu_ ## type (row, pstrText , args )
#define LCD_DRAW_MENU_SELECTED(type, row, pstrText, args...)    lcd_implementation_drawmenu_ ## type ## _selected (row, pstrText , args )
#define EXECUTE_MENU_ACTION(type, args...)                      menu_action_ ## type ( args );
#define MENU_ITEM(type, label, args...) \
    do \
    {\
        uint8_t _row = _menuItemIndex - currentMenuViewOffset; \
        if (_row >= 0 && _row < LCD_HEIGHT) \
        {\
            const char* _label_pstr = PSTR(label); \
            if (IS_MENU_SELECTED) \
            {\
                LCD_DRAW_MENU_SELECTED(type, _row, _label_pstr, args); \
                if (LCD_CLICKED) \
                {\
                    lcd_quick_feedback(); \
                    EXECUTE_MENU_ACTION(type, args); \
                    return;\
                }\
            }\
            else \
            {\
                LCD_DRAW_MENU(type, _row, _label_pstr, args); \
            }\
        }\
        _menuItemIndex++; \
    } while(0)

#define LCD_DRAW_EDIT_MENU(type, row, pstrText, isEditMode, data, args...)              lcd_implementation_drawmenu_setting_edit_generic(row, pstrText, isEditMode, ' ', type ## _tostr (*(data)))
#define LCD_DRAW_EDIT_MENU_SELECTED(type, row, pstrText, isEditMode, data, args...)     lcd_implementation_drawmenu_setting_edit_generic(row, pstrText, isEditMode, '>', type ## _tostr (*(data)))
#define START_INLINE_EDIT(type, callback, args...)                                      menu_action_start_edit_ ## type ((#callback == "0") ? NULL : callback, args )
#define MENU_ITEM_EDIT_CALLBACK(type, label, callback, args...) \
    do \
    {\
        uint8_t _row = _menuItemIndex - currentMenuViewOffset; \
        if (_row >= 0 && _row < LCD_HEIGHT) \
        {\
            const char* _label_pstr = PSTR(label); \
            if (IS_MENU_SELECTED) \
            {\
                LCD_DRAW_EDIT_MENU_SELECTED(type, _row, _label_pstr, IS_INLINE_EDIT, args); \
                if (LCD_CLICKED && !IS_INLINE_EDIT) \
                {\
                    lcd_quick_feedback(); \
                    START_INLINE_EDIT(type, callback, args); \
                    return;\
                }\
            }\
            else \
            {\
                LCD_DRAW_EDIT_MENU(type, _row, _label_pstr, false, args); \
            }\
        }\
        _menuItemIndex++; \
    } while(0)

#define MENU_ITEM_EDIT(type, label, args...) MENU_ITEM_EDIT_CALLBACK(type, label, 0, ## args )

#define MENU_ITEM_DUMMY() do { _menuItemIndex++; } while(0)

#define END_MENU() \
    if (!IS_INLINE_EDIT) \
        {\
            if (encoderPosition / ENCODER_STEPS_PER_MENU_ITEM >= _menuItemIndex) \
                encoderPosition = _menuItemIndex * ENCODER_STEPS_PER_MENU_ITEM - 1; \
            if ((uint8_t)(encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) >= currentMenuViewOffset + LCD_HEIGHT) \
            { \
                currentMenuViewOffset = (uint8_t)(encoderPosition / ENCODER_STEPS_PER_MENU_ITEM) - LCD_HEIGHT + 1; \
                lcdDrawUpdate = 2; \
            } \
        }\
    } while(0)

// Edit menu actions
#define menu_edit_type(_name, _type, scale) \
    void menu_edit_callback_ ## _name () \
    { \
        if ((int32_t)encoderPosition < minEditValue) \
            encoderPosition = minEditValue; \
        if ((int32_t)encoderPosition > maxEditValue) \
            encoderPosition = maxEditValue; \
        if (*((_type*)editValue) != ((_type)encoderPosition) / scale) \
        {\
            *((_type*)editValue) = ((_type)encoderPosition) / scale; \
            lcd_quick_feedback(); \
        }\
        if (LCD_CLICKED) \
        { \
            encoderPosition = savedEncoderPosition; \
            savedEncoderPosition = -1;  \
            if (callbackFunc != NULL)   \
            (*callbackFunc)();      \
            inlineEditMenu = NULL;      \
            lcd_quick_feedback();       \
        } \
    } \
    static void menu_action_start_edit_ ## _name (menuFunc_t callback, _type* ptr, _type minValue, _type maxValue) \
    { \
        savedEncoderPosition = encoderPosition; \
        \
        lcdDrawUpdate = 2; \
        inlineEditMenu = menu_edit_callback_ ## _name; \
        \
        editValue = ptr;                    \
        minEditValue = minValue * scale;    \
        maxEditValue = maxValue * scale;    \
        encoderPosition = (*ptr) * scale;   \
        callbackFunc = callback;            \
    }


/* Different types of actions that can be used in menuitems. */
static void menu_action_back(menuFunc_t data);
static void menu_action_submenu(menuFunc_t data, int param1 = 0, int param2 = 0);
static void menu_action_submenu_str(const char*, menuFunc_t data, int param1 = 0, int param2 = 0);
static void menu_action_gcode(const char* pgcode);
static void menu_action_function(menuFunc_t data);
static void menu_action_function_str(const char*, menuFunc_t data);
static void menu_action_sdfile(char* filename, char* longFilename);
static void menu_action_sddirectory(const char* filename, char* longFilename);

// Edit actions:
//inlineEditMenu and savedEncoderPosition are used to render an inline editing of a value
menuFunc_t inlineEditMenu = NULL;
uint16_t savedEncoderPosition;

//Variables used when editing values.
void* editValue;
int32_t minEditValue;
int32_t maxEditValue;
menuFunc_t callbackFunc;

// pstr is a prefix (label) to be printed before the value.
static void menu_action_start_edit_filament(menuFunc_t callbackFunc, unsigned char* ptr, unsigned char minValue, unsigned char maxValue);
static void menu_action_start_edit_extruder(menuFunc_t callbackFunc, char* ptr, char minValue, char maxValue);
static void menu_action_start_edit_bool(menuFunc_t callbackFunc, bool* ptr);
static void menu_action_start_edit_uchar(menuFunc_t callbackFunc, unsigned char* ptr, unsigned char minValue, unsigned char maxValue);
static void menu_action_start_edit_int2(menuFunc_t callbackFunc, int* ptr, int minValue, int maxValue);
static void menu_action_start_edit_int3(menuFunc_t callbackFunc, int* ptr, int minValue, int maxValue);
static void menu_action_start_edit_float3(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_float31(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_float32(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_float5(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_float51(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_float52(menuFunc_t callbackFunc, float* ptr, float minValue, float maxValue);
static void menu_action_start_edit_long5(menuFunc_t callbackFunc, unsigned long* ptr, unsigned long minValue, unsigned long maxValue);

menu_edit_type(uchar, unsigned char, 1)
menu_edit_type(int2, int, 1)
menu_edit_type(int3, int, 1)
menu_edit_type(float3, float, 1)
menu_edit_type(float31, float, 10)
menu_edit_type(float32, float, 100)
menu_edit_type(float5, float, 0.01)
menu_edit_type(float51, float, 10)
menu_edit_type(float52, float, 100)
menu_edit_type(long5, unsigned long, 0.01)
menu_edit_type(filament, unsigned char, 2)
menu_edit_type(extruder, char, 2)



/* Main status screen. It's up to the implementation specific part to show what is needed. As this is very display dependend */
static void lcd_status_screen()
{
    if (lcd_status_update_delay)
        lcd_status_update_delay--;
    else
        lcdDrawUpdate = 1;
    if (lcdDrawUpdate)
    {
        lcd_implementation_status_screen();
        lcd_status_update_delay = 10;   /* redraw the main screen every second. This is easier then trying keep track of all things that change on the screen */
    }
#ifdef ULTIPANEL
    if (LCD_CLICKED)
    {
        currentMenu = lcd_main_menu;
        encoderPosition = 0;
        lcd_quick_feedback();
    }

    // Dead zone at 100% feedrate
    if ((feedmultiply < 100 && (feedmultiply + encoderPosition) > 100) ||
        (feedmultiply > 100 && (feedmultiply + encoderPosition) < 100))
    {
        encoderPosition = 0;
        feedmultiply = 100;
    }

    if (feedmultiply == 100 && encoderPosition > ENCODER_FEEDRATE_DEADZONE)
    {
        feedmultiply += encoderPosition - ENCODER_FEEDRATE_DEADZONE;
        encoderPosition = 0;
    }
    else if (feedmultiply == 100 && encoderPosition < -ENCODER_FEEDRATE_DEADZONE)
    {
        feedmultiply += encoderPosition + ENCODER_FEEDRATE_DEADZONE;
        encoderPosition = 0;
    }
    else if (feedmultiply != 100)
    {
        feedmultiply += encoderPosition;
        encoderPosition = 0;
    }

    if (feedmultiply < 10)
        feedmultiply = 10;
    if (feedmultiply > 999)
        feedmultiply = 999;
#endif//ULTIPANEL
}

#ifdef ULTIPANEL
static void lcd_return_to_status()
{
    encoderPosition = 0;
    savedEncoderPosition = 0;
    currentMenu_param1 = 0;
    currentMenu_param2 = 0;
    inlineEditMenu = NULL;
    lcd_status_update_delay = 0;
    currentMenu = lcd_status_screen;
}

static void lcd_sdcard_pause()
{
    card.pauseSDPrint();
}
static void lcd_sdcard_resume()
{
    card.startFileprint();
}

static void lcd_sdcard_stop()
{
    card.sdprinting = false;
    card.closefile();
    quickStop();
    if (SD_FINISHED_STEPPERRELEASE)
    {
        enquecommand_P(PSTR(SD_FINISHED_RELEASECOMMAND));
    }
    autotempShutdown();
}

static void lcd_system_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);


#ifdef PIDTEMP
    // set up temp variables - undo the default scaling
    // NOTE: this has to be outside of the lcd_system_extruder, due to race condition of 2 places trying to update the same variable
    raw_Ki = unscalePID_i(WORK_Ki(0));
    raw_Kd = unscalePID_d(WORK_Kd(0));
#endif
    MENU_ITEM(submenu_str, "X1: ", systemInfo.Extruders[0]->ToString(), lcd_system_extruder, 0);

#if MAX_EXTRUDERS > 1
#ifdef PIDTEMP
    // set up temp variables - undo the default scaling
    // NOTE: this has to be outside of the lcd_system_extruder, due to race condition of 2 places trying to update the same variable
    raw_Ki = unscalePID_i(WORK_Ki(1));
    raw_Kd = unscalePID_d(WORK_Kd(1));
#endif
    MENU_ITEM(submenu_str, "X2: ", systemInfo.Extruders[1]->ToString(), lcd_system_extruder, 1);
#endif

    // provide an option to change the primary extruder before printing
    if (INSTALLED_EXTRUDERS > 1)
    {
        MENU_ITEM_EDIT_CALLBACK(uchar, MSG_PRIMARY_EXTRUDER, send_command_T0, &systemInfo.primaryExtruder, 0, systemInfo.installedExtruders-1);
    }
    
    // offer an option to store the settings
#ifdef EEPROM_SETTINGS
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
#endif

    END_MENU();
}

static void lcd_system_extruder()
{
    extruderId = currentMenu_param1;

    START_MENU();
    MENU_ITEM(back, MSG_SYSTEM, lcd_system_menu);

    MENU_ITEM_EDIT_CALLBACK(extruder, "Type", refresh_system_settings, &systemInfo.Extruders[extruderId]->type, (extruderId == 0 ? 0 : -1), EXTRUDER_TYPE_COUNT - 1);

    // only show more selections if the extruder is installed
    if (systemInfo.Extruders[extruderId]->type != EXTRUDER_TYPE_NOT_INSTALLED)
    {
        MENU_ITEM_EDIT_CALLBACK(filament, MSG_LOADED_FILAMENT, refresh_system_settings, &systemInfo.Extruders[extruderId]->activeFillament, 0, FILAMENT_COUNT - 1);
        MENU_ITEM_EDIT(int3, " HotEnd Temp", systemInfo.Extruders[extruderId]->pFillamentHotEndTemp, 0, systemInfo.Extruders[extruderId]->heater_MaxTemp - 15);
        MENU_ITEM_EDIT(uchar, " Bed Temp", systemInfo.Extruders[extruderId]->pFillamentHPBTemp, 0, BED_MAXTEMP - 15);
        MENU_ITEM_EDIT(uchar, " Fan Speed", systemInfo.Extruders[extruderId]->pFillamentFanSpeed, 0, 255);

        //MENU_ITEM_EDIT_CALLBACK(nozzle_diameter, "Nozzle Diameter", refresh_system_settings, &systemInfo.Extruders[extruderId]->activeFillament, 0, 1);

        // offset: for extruder 1 (id=0) we don't allow the user to edit the offset - it's always set to 0
        MENU_ITEM(function, MSG_NOZZLE_OFFSET, do_nothing);
        MENU_ITEM_EDIT(float31, " X", &EXTRUDER_OFFSET(extruderId)[X_AXIS], (extruderId == 0) ? 0 : -20.0, (extruderId == 0) ? 0 : 20.0);
        MENU_ITEM_EDIT(float31, " Y", &EXTRUDER_OFFSET(extruderId)[Y_AXIS], (extruderId == 0) ? 0 : -80.0, (extruderId == 0) ? 0 : 0);

        MENU_ITEM_EDIT(float51, MSG_ESTEPS, &systemInfo.Extruders[extruderId]->stepsPerUnit, 5, 9999);

#ifdef PIDTEMP
        MENU_ITEM(function, MSG_PID, do_nothing);

        MENU_ITEM_EDIT(float52, MSG_PID_P, &WORK_Kp(extruderId), 1, 9990);
        // i is typically a small value so allows values below 1
        MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_I, copy_and_scalePID_i, &raw_Ki, 0.01, 9990);
        MENU_ITEM_EDIT_CALLBACK(float52, MSG_PID_D, copy_and_scalePID_d, &raw_Kd, 1, 9990);
#ifdef PID_ADD_EXTRUSION_RATE
        MENU_ITEM_EDIT(float3, MSG_PID_C, &WORK_Kc(extruderId), 1, 9990);
#endif//PID_ADD_EXTRUSION_RATE
#endif
    }

    END_MENU();
}


/* Menu implementation */
static void lcd_main_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_WATCH, lcd_status_screen);
    if (movesplanned() || IS_SD_PRINTING)
    {
        MENU_ITEM(submenu, MSG_TUNE, lcd_tune_menu);
    }
    else{
        MENU_ITEM(submenu, MSG_PREPARE, lcd_prepare_menu);
    }
    MENU_ITEM(submenu, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM(submenu, MSG_SYSTEM, lcd_system_menu);
#ifdef SDSUPPORT
    if (card.cardOK)
    {
        if (card.isFileOpen())
        {
            if (card.sdprinting)
                MENU_ITEM(function, MSG_PAUSE_PRINT, lcd_sdcard_pause);
            else
                MENU_ITEM(function, MSG_RESUME_PRINT, lcd_sdcard_resume);
            MENU_ITEM(function, MSG_STOP_PRINT, lcd_sdcard_stop);
        }
        else{
            MENU_ITEM(submenu, MSG_CARD_MENU, lcd_sdcard_menu);
#if SDCARDDETECT < 1
            MENU_ITEM(gcode, MSG_CNG_SDCARD, PSTR("M21"));  // SD-card changed by user
#endif
        }
    }
    else{
        MENU_ITEM(submenu, MSG_NO_CARD, lcd_sdcard_menu);
#if SDCARDDETECT < 1
        MENU_ITEM(gcode, MSG_INIT_SDCARD, PSTR("M21")); // Manually initialize the SD-card via user interface
#endif
    }
#endif
    END_MENU();
}

#ifdef SDSUPPORT
static void lcd_autostart_sd()
{
    card.lastnr = 0;
    card.setroot();
    card.checkautostart(true);
}
#endif

void lcd_preheat()
{
    setTargetHotend0(*systemInfo.Extruders[0]->pFillamentHotEndTemp);
    fanSpeed = *systemInfo.Extruders[0]->pFillamentFanSpeed;

    // pick the highest bed temp...
    unsigned char maxBedTemp = *systemInfo.Extruders[0]->pFillamentHPBTemp;

#if MAX_EXTRUDERS > 1
    if (INSTALLED_EXTRUDERS > 1)
    {
        setTargetHotend1(*systemInfo.Extruders[1]->pFillamentHotEndTemp);
        fanSpeed1 = *systemInfo.Extruders[1]->pFillamentFanSpeed;
        maxBedTemp = max(maxBedTemp, *systemInfo.Extruders[1]->pFillamentHPBTemp);
    }
#endif
#if MAX_EXTRUDERS > 2
    if (INSTALLED_EXTRUDERS > 2)
    {
        setTargetHotend2(*systemInfo.Extruders[2]->pFillamentHotEndTemp);
        //fanSpeed2 = *systemInfo.Extruders[2]->pFillamentFanSpeed;
        maxBedTemp = max(maxBedTemp, *systemInfo.Extruders[2]->pFillamentHPBTemp);
    }
#endif

    setTargetBed(maxBedTemp);

    lcd_return_to_status();
    setWatch(); // heater sanity check timer
}

static void lcd_cooldown()
{
    setTargetHotend0(0);
    setTargetHotend1(0);
    setTargetHotend2(0);
    setTargetBed(0);
    lcd_return_to_status();
}

#ifdef BABYSTEPPING
static void lcd_babystep_x()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[X_AXIS]+=(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Babystepping X"),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}

static void lcd_babystep_y()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[Y_AXIS]+=(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Babystepping Y"),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}

static void lcd_babystep_z()
{
    if (encoderPosition != 0)
    {
        babystepsTodo[Z_AXIS]+=BABYSTEP_Z_MULTIPLICATOR*(int)encoderPosition;
        encoderPosition=0;
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Babystepping Z"),"");
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_tune_menu;
        encoderPosition = 0;
    }
}
#endif //BABYSTEPPING

static void lcd_tune_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
    MENU_ITEM_EDIT(int3, MSG_SPEED, &feedmultiply, 10, 999);
    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 5);
#if TEMP_SENSOR_1 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE1, &target_temperature[1], 0, HEATER_1_MAXTEMP - 5);
#endif
#if TEMP_SENSOR_2 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE2, &target_temperature[2], 0, HEATER_2_MAXTEMP - 5);
#endif
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15);
#endif
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);
    MENU_ITEM_EDIT(int3, MSG_FLOW, &extrudemultiply, 10, 999);

#ifdef BABYSTEPPING
#ifdef BABYSTEP_XY
    MENU_ITEM(submenu, "Babystep X", lcd_babystep_x);
    MENU_ITEM(submenu, "Babystep Y", lcd_babystep_y);
#endif //BABYSTEP_XY
    MENU_ITEM(submenu, "Babystep Z", lcd_babystep_z);
#endif
#ifdef FILAMENTCHANGEENABLE
    MENU_ITEM(gcode, MSG_FILAMENTCHANGE, PSTR("M600"));
#endif
    END_MENU();
}

static void lcd_prepare_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
#ifdef SDSUPPORT
#ifdef MENU_ADDAUTOSTART
    MENU_ITEM(function, MSG_AUTOSTART, lcd_autostart_sd);
#endif
#endif
    String s = filament_tostr_short(systemInfo.Extruders[0]->activeFillament);
    s.concat("|");
    if (INSTALLED_EXTRUDERS > 1)
    {
        s.concat(filament_tostr_short(systemInfo.Extruders[1]->activeFillament));
    }
    else
    {
        s.concat(" - ");
    }

    MENU_ITEM(function_str, MSG_PREHEAT, s.c_str(), lcd_preheat);
    MENU_ITEM(submenu, MSG_MOVE_AXIS, lcd_move_menu);
    MENU_ITEM(function, MSG_COOLDOWN, lcd_cooldown);
    MENU_ITEM(gcode, MSG_DISABLE_STEPPERS, PSTR("M84"));
    MENU_ITEM(gcode, MSG_AUTO_HOME, PSTR("G28"));
    //MENU_ITEM(gcode, MSG_SET_ORIGIN, PSTR("G92 X0 Y0 Z0"));
#if PS_ON_PIN > -1
    if (powersupply)
    {
        MENU_ITEM(gcode, MSG_SWITCH_PS_OFF, PSTR("M81"));
    }
    else{
        MENU_ITEM(gcode, MSG_SWITCH_PS_ON, PSTR("M80"));
    }
#endif
    END_MENU();
}

float move_menu_scale;
static void lcd_move_menu_axis();

static void lcd_back_to_menu_move_axis()
{
    lcd_quick_feedback();
    currentMenu = lcd_move_menu_axis;
    encoderPosition = 0;
}
static void lcd_move_x()
{
    if (encoderPosition != 0)
    {
        current_position[X_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[X_AXIS] < X_MIN_POS)
            current_position[X_AXIS] = X_MIN_POS;
        if (max_software_endstops && current_position[X_AXIS] > X_MAX_POS)
            current_position[X_AXIS] = X_MAX_POS;
        encoderPosition = 0;
#ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS]/60, active_extruder);
#else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[X_AXIS] / 60, active_extruder);
#endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("X"), float31_tostr(current_position[X_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_back_to_menu_move_axis();
    }
}
static void lcd_move_y()
{
    if (encoderPosition != 0)
    {
        current_position[Y_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[Y_AXIS] < Y_MIN_POS)
            current_position[Y_AXIS] = Y_MIN_POS;
        if (max_software_endstops && current_position[Y_AXIS] > Y_MAX_POS)
            current_position[Y_AXIS] = Y_MAX_POS;
        encoderPosition = 0;
#ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[Y_AXIS]/60, active_extruder);
#else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[Y_AXIS] / 60, active_extruder);
#endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Y"), float31_tostr(current_position[Y_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_back_to_menu_move_axis();
    }
}
static void lcd_move_z()
{
    if (encoderPosition != 0)
    {
        current_position[Z_AXIS] += float((int)encoderPosition) * move_menu_scale;
        if (min_software_endstops && current_position[Z_AXIS] < Z_MIN_POS)
            current_position[Z_AXIS] = Z_MIN_POS;
        if (max_software_endstops && current_position[Z_AXIS] > Z_MAX_POS)
            current_position[Z_AXIS] = Z_MAX_POS;
        encoderPosition = 0;
#ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[Z_AXIS]/60, active_extruder);
#else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[Z_AXIS] / 60, active_extruder);
#endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Z"), float31_tostr(current_position[Z_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_back_to_menu_move_axis();
    }
}
static void lcd_move_e()
{
    active_extruder = currentMenu_param1;
    if (encoderPosition != 0)
    {
        current_position[E_AXIS] += float((int)encoderPosition) * move_menu_scale;
        encoderPosition = 0;
#ifdef DELTA
        calculate_delta(current_position);
        plan_buffer_line(delta[X_AXIS], delta[Y_AXIS], delta[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS]/60, active_extruder);
#else
        plan_buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], manual_feedrate[E_AXIS] / 60, active_extruder);
#endif
        lcdDrawUpdate = 1;
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Extruder"), float31_tostr(current_position[E_AXIS]));
    }
    if (LCD_CLICKED)
    {
        lcd_back_to_menu_move_axis();
    }
}

static void lcd_move_menu_axis()
{
    START_MENU();
    MENU_ITEM(back, MSG_MOVE_AXIS, lcd_move_menu);
    MENU_ITEM(submenu, "Move X", lcd_move_x);
    MENU_ITEM(submenu, "Move Y", lcd_move_y);
    if (move_menu_scale < 10.0)
    {
        MENU_ITEM(submenu, "Move Z", lcd_move_z);
        MENU_ITEM(submenu, "Extruder 1", lcd_move_e, 0);
        if (INSTALLED_EXTRUDERS > 1)
        {
            MENU_ITEM(submenu, "Extruder 2", lcd_move_e, 1);
        }
    }
    END_MENU();
}

static void lcd_move_menu_10mm()
{
    move_menu_scale = 10.0;
    lcd_move_menu_axis();
}
static void lcd_move_menu_1mm()
{
    move_menu_scale = 1.0;
    lcd_move_menu_axis();
}
static void lcd_move_menu_01mm()
{
    move_menu_scale = 0.1;
    lcd_move_menu_axis();
}

static void lcd_move_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_PREPARE, lcd_prepare_menu);
    MENU_ITEM(submenu, "Move 10mm", lcd_move_menu_10mm);
    MENU_ITEM(submenu, "Move 1mm", lcd_move_menu_1mm);
    MENU_ITEM(submenu, "Move 0.1mm", lcd_move_menu_01mm);
    //TODO:X,Y,Z,E
    END_MENU();
}

static void lcd_control_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
    MENU_ITEM(submenu, MSG_TEMPERATURE, lcd_control_temperature_menu);
    MENU_ITEM(submenu, MSG_MOTION, lcd_control_motion_menu);
    //#ifdef DOGLCD
    //    MENU_ITEM_EDIT(int3, MSG_CONTRAST, &lcd_contrast, 0, 63);
    //MENU_ITEM(submenu, MSG_CONTRAST, lcd_set_contrast);
    //#endif
#ifdef FWRETRACT
    MENU_ITEM(submenu, MSG_RETRACT, lcd_control_retract_menu);
#endif
#ifdef EEPROM_SETTINGS
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
    MENU_ITEM(function, MSG_LOAD_EPROM, Config_RetrieveSettings);
#endif
    MENU_ITEM(function, MSG_RESTORE_FAILSAFE, Config_ResetDefault);
    END_MENU();
}

static void lcd_control_temperature_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM_EDIT(int3, MSG_NOZZLE, &target_temperature[0], 0, HEATER_0_MAXTEMP - 15);
#if TEMP_SENSOR_1 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE1, &target_temperature[1], 0, HEATER_1_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_2 != 0
    MENU_ITEM_EDIT(int3, MSG_NOZZLE2, &target_temperature[2], 0, HEATER_2_MAXTEMP - 15);
#endif
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(int3, MSG_BED, &target_temperature_bed, 0, BED_MAXTEMP - 15);
#endif
    MENU_ITEM_EDIT(int3, MSG_FAN_SPEED, &fanSpeed, 0, 255);
#ifdef AUTOTEMP
    MENU_ITEM_EDIT(bool, MSG_AUTOTEMP, &autotemp_enabled);
    MENU_ITEM_EDIT(float3, MSG_MIN, &autotemp_min, 0, HEATER_0_MAXTEMP - 15);
    MENU_ITEM_EDIT(float3, MSG_MAX, &autotemp_max, 0, HEATER_0_MAXTEMP - 15);
    MENU_ITEM_EDIT(float32, MSG_FACTOR, &autotemp_factor, 0.0, 1.0);
#endif
    MENU_ITEM(submenu, MSG_PREHEAT_SETTINGS, lcd_control_temperature_filament_settings_menu);
    END_MENU();
}

static void lcd_control_temperature_filament_settings_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_TEMPERATURE, lcd_control_temperature_menu);

    // display all the available filaments to edit values for all extruder types
    for (int extypeId = 0; extypeId < EXTRUDER_TYPE_COUNT; extypeId++)
    {
        MENU_ITEM(function_str, "", extruder_tostr(extypeId), do_nothing);
        for (int fillId = 0; fillId < FILAMENT_COUNT; fillId++)
        {
            MENU_ITEM(submenu_str, "  ", filament_tostr(fillId), lcd_control_temperature_filament_settings_values_menu, extypeId, fillId);
        }
    }

    END_MENU();
}

static void lcd_control_temperature_filament_settings_values_menu()
{
    int extruderTypeId = currentMenu_param1;
    int filamentId = currentMenu_param2;

    START_MENU();
    MENU_ITEM(back, MSG_TEMPERATURE, lcd_control_temperature_filament_settings_menu);

    MENU_ITEM_EDIT(int3, MSG_NOZZLE, (int*)&filamentConfig[extruderTypeId][filamentId][ID_HOTEND_TEMP], 0, HEATER_MAXTEMP(extruderTypeId) - 15);
#if TEMP_SENSOR_BED != 0
    MENU_ITEM_EDIT(uchar, MSG_BED, &filamentConfig[extruderTypeId][filamentId][ID_HPB_TEMP], 0, BED_MAXTEMP - 15);
#endif
    MENU_ITEM_EDIT(uchar, MSG_FAN_SPEED, &filamentConfig[extruderTypeId][filamentId][ID_FAN_SPEED], 0, 255);

#ifdef EEPROM_SETTINGS
    MENU_ITEM(function, MSG_STORE_EPROM, Config_StoreSettings);
#endif
    END_MENU();
}

static void lcd_control_motion_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM_EDIT(float32, MSG_ZPROBE_ZOFFSET, &zprobe_zoffset, 0.5, 50);
    MENU_ITEM_EDIT(float5, MSG_ACC, &acceleration, 500, 99000);
    MENU_ITEM_EDIT(float3, MSG_VXY_JERK, &max_xy_jerk, 1, 990);
    MENU_ITEM_EDIT(float52, MSG_VZ_JERK, &max_z_jerk, 0.1, 990);
    MENU_ITEM_EDIT(float3, MSG_VE_JERK, &max_e_jerk, 1, 990);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_X, &max_feedrate[X_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Y, &max_feedrate[Y_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_Z, &max_feedrate[Z_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMAX MSG_E, &max_feedrate[E_AXIS], 1, 999);
    MENU_ITEM_EDIT(float3, MSG_VMIN, &minimumfeedrate, 0, 999);
    MENU_ITEM_EDIT(float3, MSG_VTRAV_MIN, &mintravelfeedrate, 0, 999);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_X, reset_acceleration_rates, &max_acceleration_units_per_sq_second[X_AXIS], 100, 99000);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Y, reset_acceleration_rates, &max_acceleration_units_per_sq_second[Y_AXIS], 100, 99000);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_Z, reset_acceleration_rates, &max_acceleration_units_per_sq_second[Z_AXIS], 100, 99000);
    MENU_ITEM_EDIT_CALLBACK(long5, MSG_AMAX MSG_E, reset_acceleration_rates, &max_acceleration_units_per_sq_second[E_AXIS], 100, 99000);
    MENU_ITEM_EDIT(float5, MSG_A_RETRACT, &retract_acceleration, 100, 99000);
    MENU_ITEM_EDIT(float52, MSG_XSTEPS, &axis_steps_per_unit[X_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float52, MSG_YSTEPS, &axis_steps_per_unit[Y_AXIS], 5, 9999);
    MENU_ITEM_EDIT(float51, MSG_ZSTEPS, &axis_steps_per_unit[Z_AXIS], 5, 9999);
#ifdef ABORT_ON_ENDSTOP_HIT_FEATURE_ENABLED
    MENU_ITEM_EDIT(bool, "Endstop abort", &abort_on_endstop_hit);
#endif
    END_MENU();
}

#ifdef DOGLCD
static void lcd_set_contrast()
{
    if (encoderPosition != 0)
    {
        lcd_contrast -= encoderPosition;
        if (lcd_contrast < 0) lcd_contrast = 0;
        else if (lcd_contrast > 63) lcd_contrast = 63;
        encoderPosition = 0;
        lcdDrawUpdate = 1;
        u8g.setContrast(lcd_contrast);
    }
    if (lcdDrawUpdate)
    {
        lcd_implementation_drawedit(PSTR("Contrast"), int2_tostr(lcd_contrast));
    }
    if (LCD_CLICKED)
    {
        lcd_quick_feedback();
        currentMenu = lcd_control_menu;
        encoderPosition = 0;
    }
}
#endif

#ifdef FWRETRACT
static void lcd_control_retract_menu()
{
    START_MENU();
    MENU_ITEM(back, MSG_CONTROL, lcd_control_menu);
    MENU_ITEM_EDIT(bool, MSG_AUTORETRACT, &autoretract_enabled);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT, &retract_length, 0, 100);
    MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACTF, &retract_feedrate, 1, 999);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_ZLIFT, &retract_zlift, 0, 999);
    MENU_ITEM_EDIT(float52, MSG_CONTROL_RETRACT_RECOVER, &retract_recover_length, 0, 100);
    MENU_ITEM_EDIT(float3, MSG_CONTROL_RETRACT_RECOVERF, &retract_recover_feedrate, 1, 999);
    END_MENU();
}
#endif

#if SDCARDDETECT == -1
static void lcd_sd_refresh()
{
    card.initsd();
    currentMenuViewOffset = 0;
}
#endif
static void lcd_sd_updir()
{
    card.updir();
    currentMenuViewOffset = 0;
}
static void print_current_filename()
{
    char* filename = currentMenu_filename;
    
    char cmd[30];
    char* c;
    sprintf_P(cmd, PSTR("M23 %s"), filename);
    for (c = &cmd[4]; *c; c++)
        *c = tolower(*c);
    enquecommand(cmd);
    enquecommand_P(PSTR("M24"));
    lcd_return_to_status();
}

void send_command_T0()
{
    enquecommand_P(PSTR("T0 F0"));
}
void lcd_file_config_pre_print()
{
    START_MENU();
    MENU_ITEM(back, "Browse", lcd_sdcard_menu);
    
    // provide an option to change the primary extruder before printing
    if (INSTALLED_EXTRUDERS > 1)
    {
        MENU_ITEM_EDIT_CALLBACK(uchar, MSG_PRIMARY_EXTRUDER, send_command_T0, &systemInfo.primaryExtruder, 0, systemInfo.installedExtruders-1);
    }
    MENU_ITEM(function, "Print!", print_current_filename);
    END_MENU();
}
void lcd_sdcard_menu()
{
    if (lcdDrawUpdate == 0 && LCD_CLICKED == 0)
        return;	// nothing to do (so don't thrash the SD card)
    uint16_t fileCnt = card.getnrfilenames();
    START_MENU();
    MENU_ITEM(back, MSG_MAIN, lcd_main_menu);
    card.getWorkDirName();
    if (card.filename[0] == '/')
    {
#if SDCARDDETECT == -1
        MENU_ITEM(function, LCD_STR_REFRESH MSG_REFRESH, lcd_sd_refresh);
#endif
    }
    else
    {
        MENU_ITEM(function, "..", lcd_sd_updir);
    }

    for (uint16_t i = 0; i < fileCnt; i++)
    {
#ifndef SDCARD_RATHERRECENTFIRST
        card.getfilename(i);
#else
        card.getfilename(fileCnt - 1 - i);
#endif
        if (card.filenameIsDir)
        {
            MENU_ITEM(sddirectory, MSG_CARD_MENU, card.filename, card.longFilename);
        }
        else
        {
            MENU_ITEM(sdfile, MSG_CARD_MENU, card.filename, card.longFilename);
        }
    }
    END_MENU();
}


#ifdef REPRAPWORLD_KEYPAD
static void reprapworld_keypad_move_z_up() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_z();
}
static void reprapworld_keypad_move_z_down() {
    encoderPosition = -1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_z();
}
static void reprapworld_keypad_move_x_left() {
    encoderPosition = -1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_x();
}
static void reprapworld_keypad_move_x_right() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_x();
}
static void reprapworld_keypad_move_y_down() {
    encoderPosition = 1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_y();
}
static void reprapworld_keypad_move_y_up() {
    encoderPosition = -1;
    move_menu_scale = REPRAPWORLD_KEYPAD_MOVE_STEP;
    lcd_move_y();
}
static void reprapworld_keypad_move_home() {
    enquecommand_P((PSTR("G28")));      // move all axis home
}
#endif



/** End of menus **/

static void lcd_quick_feedback()
{
    lcdDrawUpdate = 2;
    blocking_enc = millis() + 500;
    lcd_implementation_quick_feedback();
}

/** Menu action functions **/
static void menu_action_back(menuFunc_t data)
{
    currentMenu = data;
    encoderPosition = 0;
}
static void menu_action_submenu(menuFunc_t data, int param1, int param2)
{
    currentMenu_param1 = param1;
    currentMenu_param2 = param2;
    currentMenu = data;
    encoderPosition = 0;
}
static void menu_action_submenu_str(const char*, menuFunc_t data, int param1, int param2)
{
    menu_action_submenu(data, param1, param2);
}
static void menu_action_gcode(const char* pgcode)
{
    enquecommand_P(pgcode);
}
static void menu_action_function(menuFunc_t data)
{
    (*data)();
}
static void menu_action_function_str(const char*, menuFunc_t data)
{
    (*data)();
}
static void menu_action_sdfile(char* filename, char* longFilename)
{
    currentMenu_filename = filename;
    currentMenu = lcd_file_config_pre_print;
    encoderPosition = 0;
}
static void menu_action_sddirectory(const char* filename, char* longFilename)
{
    card.chdir(filename);
    encoderPosition = 0;
}

static void menu_action_start_edit_bool(menuFunc_t callbackFunc, bool* ptr)
{
    *ptr = !(*ptr);
}

#endif//ULTIPANEL

/** LCD API **/
void lcd_init()
{
    lcd_implementation_init();

#ifdef NEWPANEL
    pinMode(BTN_EN1, INPUT);
    pinMode(BTN_EN2, INPUT);
    WRITE(BTN_EN1, HIGH);
    WRITE(BTN_EN2, HIGH);
#if BTN_ENC > 0
    pinMode(BTN_ENC, INPUT);
    WRITE(BTN_ENC, HIGH);
#endif
#ifdef REPRAPWORLD_KEYPAD
    pinMode(SHIFT_CLK,OUTPUT);
    pinMode(SHIFT_LD,OUTPUT);
    pinMode(SHIFT_OUT,INPUT);
    WRITE(SHIFT_OUT,HIGH);
    WRITE(SHIFT_LD,HIGH);
#endif
#else  // Not NEWPANEL
#ifdef SR_LCD_2W_NL // Non latching 2 wire shiftregister
    pinMode (SR_DATA_PIN, OUTPUT);
    pinMode (SR_CLK_PIN, OUTPUT);
#elif defined(SHIFT_CLK) 
    pinMode(SHIFT_CLK,OUTPUT);
    pinMode(SHIFT_LD,OUTPUT);
    pinMode(SHIFT_EN,OUTPUT);
    pinMode(SHIFT_OUT,INPUT);
    WRITE(SHIFT_OUT,HIGH);
    WRITE(SHIFT_LD,HIGH);
    WRITE(SHIFT_EN,LOW);
#else
#ifdef ULTIPANEL
#error ULTIPANEL requires an encoder
#endif
#endif // SR_LCD_2W_NL
#endif//!NEWPANEL

#if defined (SDSUPPORT) && defined(SDCARDDETECT) && (SDCARDDETECT > 0)
    pinMode(SDCARDDETECT, INPUT);
    WRITE(SDCARDDETECT, HIGH);
    lcd_oldcardstatus = IS_SD_INSERTED;
#endif//(SDCARDDETECT > 0)
#ifdef LCD_HAS_SLOW_BUTTONS
    slow_buttons = 0;
#endif
    lcd_buttons_update();
#ifdef ULTIPANEL
    encoderDiff = 0;
#endif
}

void lcd_update()
{
    static unsigned long timeoutToStatus = 0;

#ifdef LCD_HAS_SLOW_BUTTONS
    slow_buttons = lcd_implementation_read_slow_buttons(); // buttons which take too long to read in interrupt context
#endif

    lcd_buttons_update();

#if (SDCARDDETECT > 0)
    if ((IS_SD_INSERTED != lcd_oldcardstatus))
    {
        lcdDrawUpdate = 2;
        lcd_oldcardstatus = IS_SD_INSERTED;
        lcd_implementation_init(); // to maybe revive the lcd if static electricty killed it.

        if (lcd_oldcardstatus)
        {
            card.initsd();
            LCD_MESSAGEPGM(MSG_SD_INSERTED);
        }
        else
        {
            card.release();
            LCD_MESSAGEPGM(MSG_SD_REMOVED);
        }
    }
#endif//CARDINSERTED

    if (lcd_next_update_millis < millis())
    {
#ifdef ULTIPANEL
#ifdef REPRAPWORLD_KEYPAD
        if (REPRAPWORLD_KEYPAD_MOVE_Z_UP) {
            reprapworld_keypad_move_z_up();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_Z_DOWN) {
            reprapworld_keypad_move_z_down();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_X_LEFT) {
            reprapworld_keypad_move_x_left();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_X_RIGHT) {
            reprapworld_keypad_move_x_right();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_Y_DOWN) {
            reprapworld_keypad_move_y_down();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_Y_UP) {
            reprapworld_keypad_move_y_up();
        }
        if (REPRAPWORLD_KEYPAD_MOVE_HOME) {
            reprapworld_keypad_move_home();
        }
#endif
        if (abs(encoderDiff) >= ENCODER_PULSES_PER_STEP)
        {
            lcdDrawUpdate = 1;
            encoderPosition += encoderDiff / ENCODER_PULSES_PER_STEP;
            encoderDiff = 0;
            timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
        }
        if (LCD_CLICKED)
            timeoutToStatus = millis() + LCD_TIMEOUT_TO_STATUS;
#endif//ULTIPANEL

#ifdef DOGLCD        // Changes due to different driver architecture of the DOGM display
        blink++;     // Variable for fan animation and alive dot
        u8g.firstPage();
        do
        {
            u8g.setFont(u8g_font_6x10_marlin);
            u8g.setPrintPos(125, 0);
            if (blink % 2)
                u8g.setColorIndex(1);
            else
                u8g.setColorIndex(0); // Set color for the alive dot
            u8g.drawPixel(127, 63); // draw alive dot
            u8g.setColorIndex(1); // black on white

            // draw the current menu
            (*currentMenu)();

            // if we are in inline edit mode draw the edit menu function (i.e. overlay)
            if (inlineEditMenu != NULL)
                (*inlineEditMenu)();

            if (!lcdDrawUpdate)
                break; // Terminate display update, when nothing new to draw. This must be done before the last dogm.next()
        } while (u8g.nextPage());
#else
        (*currentMenu)();
#endif

#ifdef LCD_HAS_STATUS_INDICATORS
        lcd_implementation_update_indicators();
#endif

#ifdef ULTIPANEL
        if (timeoutToStatus < millis() && currentMenu != lcd_status_screen)
        {
            lcd_return_to_status();
            lcdDrawUpdate = 2;
        }
#endif//ULTIPANEL
        if (lcdDrawUpdate == 2)
            lcd_implementation_clear();
        if (lcdDrawUpdate)
            lcdDrawUpdate--;
        lcd_next_update_millis = millis() + 100;
    }
}

void lcd_setstatus(const char* message)
{
    if (lcd_status_message_level > 0)
        return;
    strncpy(lcd_status_message, message, LCD_WIDTH);
    lcdDrawUpdate = 2;
}
void lcd_setstatuspgm(const char* message)
{
    if (lcd_status_message_level > 0)
        return;
    strncpy_P(lcd_status_message, message, LCD_WIDTH);
    lcdDrawUpdate = 2;
}
void lcd_setalertstatuspgm(const char* message)
{
    lcd_setstatuspgm(message);
    lcd_status_message_level = 1;
#ifdef ULTIPANEL
    lcd_return_to_status();
#endif//ULTIPANEL
}
void lcd_reset_alert_level()
{
    lcd_status_message_level = 0;
}

#ifdef DOGLCD
void lcd_setcontrast(uint8_t value)
{
    lcd_contrast = value & 63;
    u8g.setContrast(lcd_contrast);
}
#endif

#ifdef ULTIPANEL
/* Warning: This function is called from interrupt context */
void lcd_buttons_update()
{
#ifdef NEWPANEL
    uint8_t newbutton = 0;
    if (READ(BTN_EN1) == 0)  newbutton |= EN_A;
    if (READ(BTN_EN2) == 0)  newbutton |= EN_B;
#if BTN_ENC > 0
    if ((blocking_enc < millis()) && (READ(BTN_ENC) == 0))
        newbutton |= EN_C;
#endif
    buttons = newbutton;
#ifdef LCD_HAS_SLOW_BUTTONS
    buttons |= slow_buttons;
#endif
#ifdef REPRAPWORLD_KEYPAD
    // for the reprapworld_keypad
    uint8_t newbutton_reprapworld_keypad=0;
    WRITE(SHIFT_LD,LOW);
    WRITE(SHIFT_LD,HIGH);
    for(int8_t i=0;i<8;i++) {
        newbutton_reprapworld_keypad = newbutton_reprapworld_keypad>>1;
        if(READ(SHIFT_OUT))
            newbutton_reprapworld_keypad|=(1<<7);
        WRITE(SHIFT_CLK,HIGH);
        WRITE(SHIFT_CLK,LOW);
    }
    buttons_reprapworld_keypad=~newbutton_reprapworld_keypad; //invert it, because a pressed switch produces a logical 0
#endif
#else   //read it from the shift register
    uint8_t newbutton=0;
    WRITE(SHIFT_LD,LOW);
    WRITE(SHIFT_LD,HIGH);
    unsigned char tmp_buttons=0;
    for(int8_t i=0;i<8;i++)
    {
        newbutton = newbutton>>1;
        if(READ(SHIFT_OUT))
            newbutton|=(1<<7);
        WRITE(SHIFT_CLK,HIGH);
        WRITE(SHIFT_CLK,LOW);
    }
    buttons=~newbutton; //invert it, because a pressed switch produces a logical 0
#endif//!NEWPANEL

    //manage encoder rotation
    uint8_t enc = 0;
    if (buttons&EN_A)
        enc |= (1 << 0);
    if (buttons&EN_B)
        enc |= (1 << 1);
    if (enc != lastEncoderBits)
    {
        switch (enc)
        {
        case encrot0:
            if (lastEncoderBits == encrot3)
                encoderDiff++;
            else if (lastEncoderBits == encrot1)
                encoderDiff--;
            break;
        case encrot1:
            if (lastEncoderBits == encrot0)
                encoderDiff++;
            else if (lastEncoderBits == encrot2)
                encoderDiff--;
            break;
        case encrot2:
            if (lastEncoderBits == encrot1)
                encoderDiff++;
            else if (lastEncoderBits == encrot3)
                encoderDiff--;
            break;
        case encrot3:
            if (lastEncoderBits == encrot2)
                encoderDiff++;
            else if (lastEncoderBits == encrot0)
                encoderDiff--;
            break;
        }
    }
    lastEncoderBits = enc;
}

void lcd_buzz(long duration, uint16_t freq)
{
#ifdef LCD_USE_I2C_BUZZER
    lcd.buzz(duration,freq);
#endif
}

bool lcd_clicked()
{
    return LCD_CLICKED;
}
#endif//ULTIPANEL

/********************************/
/** Float conversion utilities **/
/********************************/
//  convert float to string with +123.4 format
char conv[8];
const char *bool_tostr(const bool &x)
{
    return x ? MSG_ON : MSG_OFF;
}

const char *uchar_tostr(const unsigned char &x)
{
    return int3_tostr((int)x);
}

const char *int2_tostr(const uint8_t &x)
{
    //sprintf(conv,"%5.1f",x);
    conv[0] = (x / 10) % 10 + '0';
    conv[1] = (x) % 10 + '0';
    conv[2] = 0;
    return conv;
}

const char *int31_tostr(const int &x)
{
    conv[0] = (x >= 0) ? '+' : '-';
    conv[1] = (x / 1000) % 10 + '0';
    conv[2] = (x / 100) % 10 + '0';
    conv[3] = (x / 10) % 10 + '0';
    conv[4] = '.';
    conv[5] = (x) % 10 + '0';
    conv[6] = 0;
    return conv;
}

const char *int3_tostr(const int &x)
{
    int i = 0;
    if (x >= 100)
    {
        conv[i] = (x / 100) % 10 + '0';
        i++;
    }
    
    if (x >= 10)
    {
        conv[i] = (x / 10) % 10 + '0';
        i++;
    }
    
    conv[i] = (x) % 10 + '0';
    i++;
    conv[i] = 0;
    return conv;
}

const char *int3left_tostr(const int &x)
{
    if (x >= 100)
    {
        conv[0] = (x / 100) % 10 + '0';
        conv[1] = (x / 10) % 10 + '0';
        conv[2] = (x) % 10 + '0';
        conv[3] = 0;
    }
    else if (x >= 10)
    {
        conv[0] = (x / 10) % 10 + '0';
        conv[1] = (x) % 10 + '0';
        conv[2] = 0;
    }
    else
    {
        conv[0] = (x) % 10 + '0';
        conv[1] = 0;
    }
    return conv;
}

const char *int4_itostr(const int &x)
{
    if (x >= 1000)
        conv[0] = (x / 1000) % 10 + '0';
    else
        conv[0] = ' ';
    if (x >= 100)
        conv[1] = (x / 100) % 10 + '0';
    else
        conv[1] = ' ';
    if (x >= 10)
        conv[2] = (x / 10) % 10 + '0';
    else
        conv[2] = ' ';
    conv[3] = (x) % 10 + '0';
    conv[4] = 0;
    return conv;
}

const char *long5_tostr(const unsigned long &x) { return float5_tostr(x); }

const char *float3_tostr(const float &x) { return int3_tostr((int)x); }

//  convert float to string with 123.4 format
const char *float31ns_tostr(const float &x)
{
    int xx = x * 10;
    //conv[0]=(xx>=0)?'+':'-';
    xx = abs(xx);
    conv[0] = (xx / 1000) % 10 + '0';
    conv[1] = (xx / 100) % 10 + '0';
    conv[2] = (xx / 10) % 10 + '0';
    conv[3] = '.';
    conv[4] = (xx) % 10 + '0';
    conv[5] = 0;
    return conv;
}

const char *float31_tostr(const float &x)
{
    int xx = x * 10;
    conv[0] = (xx >= 0) ? '+' : '-';
    xx = abs(xx);
    conv[1] = (xx / 1000) % 10 + '0';
    conv[2] = (xx / 100) % 10 + '0';
    conv[3] = (xx / 10) % 10 + '0';
    conv[4] = '.';
    conv[5] = (xx) % 10 + '0';
    conv[6] = 0;
    return conv;
}

const char *float32_tostr(const float &x)
{
    long xx = x * 100;
    if (xx >= 0)
        conv[0] = (xx / 10000) % 10 + '0';
    else
        conv[0] = '-';
    xx = abs(xx);
    conv[1] = (xx / 1000) % 10 + '0';
    conv[2] = (xx / 100) % 10 + '0';
    conv[3] = '.';
    conv[4] = (xx / 10) % 10 + '0';
    conv[5] = (xx) % 10 + '0';
    conv[6] = 0;
    return conv;
}

//  convert float to string with 12345 format
const char *float5_tostr(const float &x)
{
    long xx = abs(x);
    if (xx >= 10000)
        conv[0] = (xx / 10000) % 10 + '0';
    else
        conv[0] = ' ';
    if (xx >= 1000)
        conv[1] = (xx / 1000) % 10 + '0';
    else
        conv[1] = ' ';
    if (xx >= 100)
        conv[2] = (xx / 100) % 10 + '0';
    else
        conv[2] = ' ';
    if (xx >= 10)
        conv[3] = (xx / 10) % 10 + '0';
    else
        conv[3] = ' ';
    conv[4] = (xx) % 10 + '0';
    conv[5] = 0;
    return conv;
}

//  convert float to string with +1234.5 format
const char *float51_tostr(const float &x)
{
    long xx = x * 10;
    conv[0] = (xx >= 0) ? '+' : '-';
    xx = abs(xx);
    conv[1] = (xx / 10000) % 10 + '0';
    conv[2] = (xx / 1000) % 10 + '0';
    conv[3] = (xx / 100) % 10 + '0';
    conv[4] = (xx / 10) % 10 + '0';
    conv[5] = '.';
    conv[6] = (xx) % 10 + '0';
    conv[7] = 0;
    return conv;
}

//  convert float to string with +123.45 format
const char *float52_tostr(const float &x)
{
    long xx = x * 100;
    conv[0] = (xx >= 0) ? '+' : '-';
    xx = abs(xx);
    conv[1] = (xx / 10000) % 10 + '0';
    conv[2] = (xx / 1000) % 10 + '0';
    conv[3] = (xx / 100) % 10 + '0';
    conv[4] = '.';
    conv[5] = (xx / 10) % 10 + '0';
    conv[6] = (xx) % 10 + '0';
    conv[7] = 0;
    return conv;
}

const char *filament_tostr(const int &x)
{
    switch (x)
    {
    case FILAMENT_ABS: return MSG_FILAMENT_ABS;
    case FILAMENT_PLA: return MSG_FILAMENT_PLA;
    case FILAMENT_HIPS: return MSG_FILAMENT_HIPS;
    case FILAMENT_FLEXIBLE: return MSG_FILAMENT_FLEXIBLE;
    case FILAMENT_LAYWOO_D3: return MSG_FILAMENT_LAYWOO_D3;
    case FILAMENT_LAYBRICK: return MSG_FILAMENT_LAYBRICK;
    case FILAMENT_T_GLASE: return MSG_FILAMENT_T_GLASE;
    case FILAMENT_NYLON_618: return MSG_FILAMENT_NYLON_618;
    case FILAMENT_NYLON_645: return MSG_FILAMENT_NYLON_645;
    case FILAMENT_NYLON_BRIDGE: return MSG_FILAMENT_NYLON_BRIDGE;
    case FILAMENT_PVA: return MSG_FILAMENT_PVA;
    }
    return "INVALID";
}

const char *filament_tostr_short(const int &x)
{
    switch (x)
    {
    case FILAMENT_ABS: return MSG_FILAMENT_ABS_S;
    case FILAMENT_PLA: return MSG_FILAMENT_PLA_S;
    case FILAMENT_HIPS: return MSG_FILAMENT_HIPS_S;
    case FILAMENT_FLEXIBLE: return MSG_FILAMENT_FLEXIBLE_S;
    case FILAMENT_LAYWOO_D3: return MSG_FILAMENT_LAYWOO_D3_S;
    case FILAMENT_LAYBRICK: return MSG_FILAMENT_LAYBRICK_S;
    case FILAMENT_T_GLASE: return MSG_FILAMENT_T_GLASE_S;
    case FILAMENT_NYLON_618: return MSG_FILAMENT_NYLON_618_S;
    case FILAMENT_NYLON_645: return MSG_FILAMENT_NYLON_645_S;
    case FILAMENT_NYLON_BRIDGE: return MSG_FILAMENT_NYLON_BRIDGE_S;
    case FILAMENT_PVA: return MSG_FILAMENT_PVA_S;
    }
    return "INVALID";
}

const char *extruder_tostr(const int &x)
{
    switch (x)
    {
    case EXTRUDER_TYPE_BUDASCHNOZZLE: return MSG_EXTRUDER_BUDASCHNOZZLE;
    case EXTRUDER_TYPE_HEXAGON: return MSG_EXTRUDER_HEXAGON;
    case EXTRUDER_TYPE_FLEXYSTRUDER: return MSG_EXTRUDER_FLEXYSTRUDER;
    }
    return MSG_EXTRUDER_NOTINSTALLED;
}

// Callback for after editing PID i value
// grab the pid i value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_i()
{
#ifdef PIDTEMP
    WORK_Ki(extruderId) = scalePID_i(raw_Ki);
    updatePID();
#endif
}

// Callback for after editing PID d value
// grab the pid d value out of the temp variable; scale it; then update the PID driver
void copy_and_scalePID_d()
{
#ifdef PIDTEMP
    WORK_Kd(extruderId) = scalePID_d(raw_Kd);
    updatePID();
#endif
}

void refresh_system_settings()
{
    systemInfo.Update();
}



#endif //ULTRA_LCD
