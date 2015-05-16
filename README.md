# Narwhal
A Marlin (2015Q1) fork for LulzBot TAZ 4+ printers

After buying my dual extruder for my LulzBot TAZ4 I realized that I'd need to re-flash the firmware everytime I change exturders. Really annoying. So I've decided to upgrade the firmware and add a few features of my own. Overall I think that LulzBot is awesome and have great products and it's about time they have their own dedicated firmware as well.

All the base features of Marlin ar kept intact, what I plan to do is slowly erode non-TAZ printers #defines to make the code more readible and add features to make the UI and navigation better.

Please note that this is unsupported work! so if I happen to introduce a bug that gets your extruder over heating please be kind and remember this disclaimer.

Lastly, I would love if other developers want to contribute and improve the firmware. 

Added features:
  - "System" MENU (this is a major feature and the main reason to fork the firmware). This gives the ability to describe the "current setup" of the system:
    - Select the extruder settings for each extruder #: type, nozzle size, loaded fillament, E steps per mm (not yet impl).
    - Ability to edit the fillament temp configuration inline.
    - Ability to leave the 2nd extruder as "Not Installed" - this will automatically change the system configuration between 1 and 2 extruders setup.
    - Change PIDC per Extuder type.
    - Supported types: Budaschnozzle, Hexagon, Flexystruder (currently same as Buda)
  - ALL fillament settings stored in the EPROM! :) - Now I don't need to figure out every time which x-temp + bed-temp I need to set for my fillament, all the various types (from LulzBot.com) are defined within the firmware. yay. see Temprature menu.
  - Inline editing of values -  I figured that there is no need to open a new MENU just to edit a single value!
  - Preheat settings now automatically takes into account the actual installed fillaments and heats accordingly. i.e. removed the "Preheat PLA, ABS and HIPS" since most often I'm loading different fillaments in the extuders.
  - Rearanged some menus to improve accessibility to more common features faster
  - Many small code simplifications and comments here and ther to keep track of the crazy #ifdef world of the code.
  
Planned features:
  - Long filenames 'ticker' (when highlighted the filename would slowly shift left to reveal the entire name). I found it that many times I'd name my files with a bunch of descriptions like (Fillament, nozzle dia, resolution etc) this makes a long enough filename for the LCD to handle, in addition many times I'd have more than 1 filename that starts exactly the same but ends just with 1 config change (e.g. resolution) this makes it extra hard to figure out which is which.
  - Ability to override settigns from the gcode file, i.e. trust the system over the file.
  - Save named extruder settings. I plan to buy more extruders (Flexy etc.) so it would be great to save the properties of say extruder "A", "B", and "C". Then physically mark the extruders with labels so everytime I change an extruder I can load the default configs without editing the values again. (this is mostly for the type and E_Steps but I'm sure more configs will come in)
  - Change the info screen to be more friendly and include the exturder type first letter, fan % per extruder.
  - Ability to predict print time and expose it in the info screen. prediction should depend on speed % ratio.
  - multiple info screens when printing including:
    - Planned extrusion path for current layer - i.e. animation of the print head location.
    - material used up until now
  - Ability to save a log file to the SD card - every now and then I try to remember how long did it take to print a file or how much material it used or when I printed it etc. would be nice to have a log saved for every print with some details so I can review them later.
  - Change "Pause" print to "Pasue and retract" - Pause leavs the head heating the object and defecting it. I want to pause and retract up (maybe away also) to avoid damaging my print.
  - Ability to manually move the printhead to a "start" location on the model. this would be useful when I want to start with a given physical object or previous print and before I heat up the exturder move it to the location it will start printing from. this could be in x, y & z and will allow multiple colors or additions to other pre-printed objects - this would be cool! :)
  - After a user selects a file to print, provide some extra configurations / info:
    - Ability to override file temp settings
    - when the file specifies 1 extruder and the system has 2 installed, ability to select which one would extrude. 
    - Ability to select out of 9 "center of mass" positions to print around. I found myself continuously printing at various corders to avoid re-shmearing ABS "glue" on the platform and avoid cooldown/heatup timely cycles. so would be nice to be able to specify: print on top-left cornet or "middle" or "left" etc.
    - Provide alerts (warnings)
      - File temp doesn't equal installed fillament temp
      - # of extureders in file is more than installed extruders
  - Menus:
    - Show a "down" and "up" arrows on the left of the MENU screens if there are more MENU items below or above.
    - ? Start from line #2 for each menu. line #1 is always "back", when I navigate forward I always want to do an action and not go back... so on navigation forward I think that starting from line #2 makes more sense. + if the menu is longer than 1 screen I want the first line to be at position -1 in order to show more options in the first menu load.

  
