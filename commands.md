# BrickPico: Command Reference
BrickPico uses "SCPI like" command set. Command syntax should be mostly SCPI and IEE488.2 compliant,
to make it easier to control and configure Fanpico units.


## Command Set
BrickPico supports following commands:

* [*IDN?](#idn)
* [*RST](#rst)
* [CONFigure?](#configure)
* [CONFigure:DEFault:PWM](#configuredefaultpwm)
* [CONFigure:DEFault:STAte](#configuredefaultstate)
* [CONFigure:DELete](#configuredelete)
* [CONFigure:Read?](#configureread)
* [CONFigure:SAVe](#configuresave)
* [CONFigure:OUTPUTx:NAME](#configureoutputxname)
* [CONFigure:OUTPUTx:NAME?](#configureoutputxname-1)
* [CONFigure:OUTPUTx:MINpwm](#configureoutputxminpwm)
* [CONFigure:OUTPUTx:MINpwm?](#configureoutputxminpwm-1)
* [CONFigure:OUTPUTx:MAXpwm](#configureoutputxmaxpwm)
* [CONFigure:OUTPUTx:MAXpwm?](#configureoutputxmaxpwm-1)
* [CONFigure:OUTPUTx:PWM](#configureoutputxpwm)
* [CONFigure:OUTPUTx:PWM?](#configureoutputxpwm-1)
* [CONFigure:OUTPUTx:STAte](#configureoutputxstate)
* [CONFigure:OUTPUTx:STAte?](#configureoutputxstate-1)
* [CONFigure:TIMERS?](#configuretimers)
* [CONFigure:TIMERS:ADD](#configuretimersadd)
* [CONFigure:TIMERS:DEL](#configuretimersdel)
* [MEASure:Read?](#measureread)
* [MEASure:OUTPUTx?](#measureoutputx)
* [MEASure:OUTPUTx:Read?](#measureoutputxread)
* [MEASure:OUTPUTx:PWM](#measureoutputxpwm)
* [Read?](#read)
* [SYStem:ERRor?](#systemerror)
* [SYStem:DEBug](#systemdebug)
* [SYStem:DEBug?](#systemdebug-1)
* [SYStem:LOG](#systemlog)
* [SYStem:LOG?](#systemlog-1)
* [SYStem:SYSLOG](#systemsyslog)
* [SYStem:SYSLOG?](#systemsyslog-1)
* [SYStem:DISPlay](#systemdisplay)
* [SYStem:DISPlay?](#systemdisplay)
* [SYStem:DISPlay:LAYOUTR](#systemdisplaylayoutr)
* [SYStem:DISPlay:LAYOUTR?](#systemdisplaylayoutr-1)
* [SYStem:DISPlay:LOGO](#systemdisplaylogo)
* [SYStem:DISPlay:LOGO?](#systemdisplaylogo-1)
* [SYStem:DISPlay:THEMe](#systemdisplaytheme)
* [SYStem:DISPlay:THEMe?](#systemdisplaytheme-1)
* [SYStem:ECHO](#systemecho)
* [SYStem:ECHO?](#systemecho-1)
* [SYStem:FLASH?](#systemflash)
* [SYStem:OUTputs?](#systemoutputs)
* [SYStem:LED](#systemled)
* [SYStem:LED?](#systemled-1)
* [SYStem:LFS?](#systemlfs)
* [SYStem:LFS:FORMAT](#systemlfsformat)
* [SYStem:MEM](#systemmem)
* [SYStem:MEM?](#systemmem-1)
* [SYStem:MQTT:SERVer](#systemmqttserver)
* [SYStem:MQTT:SERVer?](#systemmqttserver-1)
* [SYStem:MQTT:PORT](#systemmqttport)
* [SYStem:MQTT:PORT?](#systemmqttport-1)
* [SYStem:MQTT:USER](#systemmqttuser)
* [SYStem:MQTT:USER?](#systemmqttuser-1)
* [SYStem:MQTT:PASSword](#systemmqttpassword)
* [SYStem:MQTT:PASSword?](#systemmqttpassword-1)
* [SYStem:MQTT:SCPI](#systemmqttscpi)
* [SYStem:MQTT:SCPI?](#systemmqttscpi-1)
* [SYStem:MQTT:TLS](#systemmqtttls)
* [SYStem:MQTT:TLS?](#systemmqtttls-1)
* [SYStem:MQTT:HA:DISCovery](#systemmqtthadiscovery)
* [SYStem:MQTT:HA:DISCcovery?](#systemmqtthadiscovery-1)
* [SYStem:MQTT:INTerval:STATus](#systemmqttintervalstatus)
* [SYStem:MQTT:INTerval:STATus?](#systemmqttintervalstatus-1)
* [SYStem:MQTT:INTerval:TEMP](#systemmqttintervaltemp)
* [SYStem:MQTT:INTerval:TEMP?](#systemmqttintervaltemp-1)
* [SYStem:MQTT:INTerval:PWM](#systemmqttintervalpwm)
* [SYStem:MQTT:INTerval:PWM?](#systemmqttintervalpwm-1)
* [SYStem:MQTT:TOPIC:STATus](#systemmqtttopicstatus)
* [SYStem:MQTT:TOPIC:STATus?](#systemmqtttopicstatus-1)
* [SYStem:MQTT:TOPIC:COMMand](#systemmqtttopiccommand)
* [SYStem:MQTT:TOPIC:COMMand?](#systemmqtttopiccommand-1)
* [SYStem:MQTT:TOPIC:RESPonse](#systemmqtttopicresponse)
* [SYStem:MQTT:TOPIC:RESPonse?](#systemmqtttopicresponse-1)
* [SYStem:MQTT:TOPIC:ERRor](#systemmqtttopicerror)
* [SYStem:MQTT:TOPIC:ERRor?](#systemmqtttopicerror-1)
* [SYStem:MQTT:TOPIC:WARNing](#systemmqtttopicwarning)
* [SYStem:MQTT:TOPIC:WARNing?](#systemmqtttopicwarning-1)
* [SYStem:MQTT:TOPIC:TEMP](#systemmqtttopictemp)
* [SYStem:MQTT:TOPIC:TEMP?](#systemmqtttopictemp-1)
* [SYStem:MQTT:TOPIC:PWM](#systemmqtttopicpwm)
* [SYStem:MQTT:TOPIC:PWM?](#systemmqtttopicpwm-1)
* [SYStem:NAME](#systemname)
* [SYStem:NAME?](#systemname-1)
* [SYStem:PWMfreq](#systempwmfreq)
* [SYStem:PWMfreq?](#systempwmfreq-1)
* [SYStem:SERIAL](#systemserial)
* [SYStem:SERIAL?](#systemserial-1)
* [SYStem:SPI](#systemspi)
* [SYStem:SPI?](#systemspi-1)
* [SYStem:TELNET:SERVer](#systemtelnetserver)
* [SYStem:TELNET:SERVer?](#systemtelnetserver-1)
* [SYStem:TELNET:AUTH](#systemtelnetauth)
* [SYStem:TELNET:AUTH?](#systemtelnetauth-1)
* [SYStem:TELNET:PORT](#systemtelnetport)
* [SYStem:TELNET:PORT?](#systemtelnetport-1)
* [SYStem:TELNET:RAWmode](#systemtelnetrawmode)
* [SYStem:TELNET:RAWmode?](#systemtelnetrawmode-1)
* [SYStem:TELNET:USER](#systemtelnetuser)
* [SYStem:TELNET:USER?](#systemtelnetuser-1)
* [SYStem:TELNET:PASSword](#systemtelnetpassword)
* [SYStem:TELNET:PASSword?](#systemtelnetpassword-1)
* [SYStem:TIME](#systemtime)
* [SYStem:TIME?](#systemtime-1)
* [SYStem:TIMEZONE](#systemtimezone)
* [SYStem:TIMEZONE?](#systemtimezone-1)
* [SYStem:TLS:CERT](#systemtlscert)
* [SYStem:TLS:CERT?](#systemtlscert-1)
* [SYStem:TLS:PKEY](#systemtlspkey)
* [SYStem:TLS:PKEY?](#systemtlspkey-1)
* [SYStem:UPTIme?](#systemuptime)
* [SYStem:UPGRADE](#systemupgrade)
* [SYStem:VERsion?](#systemversion)
* [SYStem:WIFI?](#systemwifi)
* [SYStem:WIFI:AUTHmode](#systemwifiauthmode)
* [SYStem:WIFI:AUTHmode?](#systemwifiauthmode-1)
* [SYStem:WIFI:COUntry](#systemwificountry)
* [SYStem:WIFI:COUntry?](#systemwificountry-1)
* [SYStem:WIFI:HOSTname](#systemwifihostname)
* [SYStem:WIFI:HOSTname?](#systemwifihostname-1)
* [SYStem:WIFI:IPaddress](#systemwifiipaddress)
* [SYStem:WIFI:IPaddress?](#systemwifiipaddress-1)
* [SYStem:WIFI:MODE](#systemwifimode)
* [SYStem:WIFI:MODE?](#systemwifimode-1)
* [SYStem:WIFI:NETMASK](#systemwifinetmask)
* [SYStem:WIFI:NETMASK?](#systemwifinetmask-1)
* [SYStem:WIFI:GATEWAY](#systemwifigateway)
* [SYStem:WIFI:GATEWAY?](#systemwifigateway-1)
* [SYStem:WIFI:NTP](#systemwifintp)
* [SYStem:WIFI:NTP?](#systemwifintp-1)
* [SYStem:WIFI:SYSLOG](#systemwifisyslog)
* [SYStem:WIFI:SYSLOG?](#systemwifisyslog-1)
* [SYStem:WIFI:MAC?](#systemwifimac)
* [SYStem:WIFI:SSID](#systemwifissid)
* [SYStem:WIFI:SSID?](#systemwifissid-1)
* [SYStem:WIFI:STATus?](#systemwifistatus)
* [SYStem:WIFI:STATS?](#systemwifistats)
* [SYStem:WIFI:PASSword](#systemwifipassword)
* [SYStem:WIFI:PASSword?](#systemwifipassword-1)
* [WRIte:OUTPUTx](#writeoutputx)
* [WRIte:OUTPUTx:PWM](#writeoutputxpwm)
* [WRIte:OUTPUTx:STAte](#writeoutputxstate)


Additionally unit will respond to following standard SCPI commands to provide compatibility in case some program
unconditionally will send these:
* *CLS
* *ESE
* *ESE?
* *ESR?
* *OPC
* *OPC?
* *SRE
* *SRE?
* *STB?
* *TST?
* *WAI


### Common Commands

#### *IDN?
Identify device. This returns string that contains following fields:
```
<manufacturer>,<model number>,<serial number>,<firmware version>
```

Example:
```
*IDN?
TJKO Industries,BRICKPICO-16,e660c0d1c768a330,1.0
```

#### *RST
Reset unit. This triggers BrickPico to perform (warm) reboot.

```
*RST
```

### CONFigure:
Commands for configuring the device settings.

#### CONFigure?
Display current configuration in JSON format.
Same as CONFigure:Read?

Example:
```
CONF?
```

#### CONFigure:DEFault:PWM
Copy current output PWM (brightess) settings to power-on default PWM settings.
Alternative to this command is to set default PWM for each port using _CONF:OUTPUTx:PWM_ command.

Configuration must be saved after running this command.

Example:
```
CONF:DEFAULT:PWM
CONF:SAVE
```

#### CONFigure:DEFault:STAte
Copy current output states (ON/OFF) to power-on default states.
Alternative to this command is to set default power on state using _CONF:OUTPUTx:DEFAULT_ command.

Configuration must be saved after running this command.

Example:
```
CONF:DEFAULT:STATE
CONF:SAVE
```

#### CONFigure:DELete
Delete current configuration saved into flash. After unit has been reset it will be using default configuration.

Example:
```
CONF:DEL
*RST
```

#### CONFigure:Read?
Display current configuration in JSON format.

Example:
```
CONF:READ?
```

#### CONFigure:SAVe
Save current configuration into flash memory.

Example:
```
CONF:SAVE
```

### CONFigure:OUTPUTx Commands
OUTPUTx commands are used to configure specific output port.
Where x is a number for the output port.

For example:
```
CONFIGURE:OUTPUT8:NAME Interrior Lights
```

#### CONFigure:OUTPUTx:NAME
Set name for output port.

For example:
```
CONF:OUTPUT1:NAME Front Lights
```

#### CONFigure:OUTPUTx:NAME?
Query name of a output port.

For example:
```
CONF:OUTPUT1:NAME?
Front Lights
```

#### CONFigure:OUTPUTx:MINpwm
Set absolute minimum PWM duty cycle (%) for given output port.
This can be used to make sure that output never sees a lower
duty cycle (overriding the normal signal).

Default: 0 %

Example: Set minimum PWM duty cycle to 20% for OUTPUT1
```
CONF:OUTPUT1:MIN 20
```

#### CONFigure:OUTPUTx:MINpwm?
Query current minimum PWM duty cycle (%) configured on a output port.

Example:
```
CONF:OUTPUT1:MIN?
20
```

#### CONFigure:OUTPUTx:MAXpwm
Set absolute maximum PWM duty cycle (%) for given output port.
This can be used to make sure that output never sees higher duty cycle
than given value (overriding the normal output signal).

Default: 100 %

Example: Set maximum PWM duty cycle to 95% for OUTPUT1
```
CONF:OUTPUT1:MAX 95
```

#### CONFigure:OUTPUTx:MAXpwm?
Query current maximum PWM duty cycle (%) configured on a output port.

Example:
```
CONF:OUTPUT1:MAX?
95
```

#### CONFigure:OUTPUTx:PWM
Set default PWM duty cycle (%) for given output port.
This is value set when unit boots up and normally defaults
to 100% (full brightness).

Default: 100%

Example: Set default PWM duty cycle to 50% for OUTPUT1
```
CONF:OUTPUT1:PWM 50
```

#### CONFigure:OUTPUTx:PWM?
Query current default PWM duty cycle for an output port.

Example:
```
CONF:OUTPUT1:PWM?
50
```

#### CONFigure:OUTPUTx:STAte
Controls whether output is ON or OFF state when unit boots up.

Values: ON, OFF

Default: OFF

Example: Set Output 5 to be ON when system boots up.
```
CONF:OUTPUT5:STATE ON
```

#### CONFigure:OUTPUTx:STAte?
Query whether output is set to default to ON or OFF when unit boots up.

Example:
```
CONF:OUTPUT1:STATE?
OFF
```

#### CONFigure:TIMERS?
List currently configured timers (events).

Output format:
```
<#>: <minute> <hour> <weekdays> <action> <outputs> <comment>
```

Example:
```
CONF:TIMERS?
1: 00 18 1-5 ON 1-6 turn on lights during the week
1: 00 17 6-7 ON 1-6 turn on lights on the weekends
2: 30 05 0-6 OFF 1-16 turn off lights in the morning
```

#### CONFigure:TIMERS:ADD
Add new timer entry.

Timer entry format:
```
<minute> <hour> <weekdays> <action> <outputs> [<comment>]
```

Field|Valid Values|Description
-----|------|-----
minute|00..59|Minute
hour|00.23|Hour (24h clock)
weekdays|0..6|0=Sunday, 1=Monday, 2=Tuesday, ... 6=Saturday
action|ON, OFF|
outputs|1..n|Outputs that the event affects.

Specifying _weekdays_ and _outputs_:

Notation|Description
--------|-----------
*|Match all
1,3,5|List
1-5|Range


Example:
```
CONF:TIMERS:ADD 00 18 1-5 ON 1-6 Turn on during the week
CONF:TIMERS:ADD 00 17 6-7 ON 1-8 Turn on during weekends
CONF:TIMERS:ADD 00 06 * OFF * Turn everything off in the morning 
```

#### CONFigure:TIMERS:DEL
Remove timer event. Command takes the event number as parameter.
Currently configured timer events along with their numbers can be
viewed using: _CONF:TIMERS?_

Example (remove second timer event):
```
CONF:TIMERS:DEL 2
```


### MEASure Commands
These commands are for reading (measuring) the current output values on ports.

#### MEASure:Read?
This command returns current output settings on all OUTPUT ports.
(This is same as: Read?)

Response format:
```
output<n>,"<name>",<output duty cycle>,<output state>
```

Example:
```
MEAS:READ?
output1,"Output 1",100.0,OFF
output2,"Output 2",50.00,OFF
output3,"Output 3",50.00,ON
output4,"Output 4",25.00,ON
...
```

### MEASure:OUTPUTx Commands

#### MEASure:OUTPUTx?
Return current output PWM signal duty cycle (%) for a output.

Response format:
```
<duty_cycle>
```

Example:
```
MEAS:OUTPUT1?
100.00
```

#### MEASure:OUTPUTx:Read?
Return current output PWM signal duty cycle (%) for a output.

This is same as: MEASure:OUTPUTx?


#### MEASure:OUTPUTx:PWM?
Return current output PWM signal duty cycle (%) for a output.

This is same as: MEASure:OUTPUTx?



### Read Commands

#### Read?
This command returns all setting for all OUTPUT ports.
(This is same as: MEASure:Read?)

Response format:
```
output<n>,"<name>",<output duty cycle>
```

Example:
```
MEAS:READ?
output1,"Output 1",100.0
output2,"Output 2",50.00
output3,"Output 3",50.00
output4,"Output 4",0.00
...
```


### SYStem Commands


#### SYStem:ERRor?
Display status from last command.

Example:
```
SYS:ERR?
0,"No Error"
```

#### SYStem:DEBug
Set the system debug level. This controls the logging to the console.

Default: 0   (do not log any debug messages)

Example: Enable verbose debug output
```
SYS:DEBUG 2
```

#### SYStem:DEBug?
Display current system debug level.

Example:
```
SYS:DEBUG?
0
```

#### SYStem:LOG
Set the system logging level. This controls the level of logging to the console.

Default: WARNING

Log Levels:

Level|Name
-----|----
0|EMERG
1|ALERT
2|CRIT
3|ERR
4|WARNING
5|NOTICE
6|INFO
7|DEBUG

Example: Enable verbose debug output
```
SYS:LOG DEBUG
```

#### SYStem:LOG?
Display current system logging level.

Example:
```
SYS:LOG?
NOTICE
```

#### SYStem:SYSLOG
Set the syslog logging level. This controls the level of logging to a remote
syslog server.

Default: ERR

Log Levels:

Level|Name
-----|----
0|EMERG
1|ALERT
2|CRIT
3|ERR
4|WARNING
5|NOTICE
6|INFO
7|DEBUG

Example: Enable logging of NOTICE (and lower level) message:
```
SYS:LOG NOTICE
```

#### SYStem:SYSLOG?
Display current syslog logging level.

Example:
```
SYS:SYSLOG?
ERR
```

#### SYStem:DISPlay
Set display (module) parameters as a comma separated list.

This can be used to set display module type if cannot be automatically detected.
Additionally this can be used to set some display parameters like brightness.

Default: default

Currently supported values:

Value|Description|Notes
-----|-----------|-----
default|Use default settings (auto-detect).
128x64|OLED 128x64 module installed.|OLED
128x128|OLED 128x128 module installed.|OLED
132x64|OLED 132x64 installed (some 1.3" 128x64 modules need this setting!)|OLED
flip|Flip display (upside down)|OLED
invert|Invert display|OLED
brightness=n|Set display brightness (%) to n (where n=0..100) [default: 50]|OLED

Example: 1.3" (SH1106) module installed that doesn't get detected correctly
```
SYS:DISP 132x64
```

Example: Invert display and set brightnes to 30%
```
SYS:DISP default,invert,brightness=30
```

#### SYStem:DISPlay?
Display current display module setting.

Example:
```
SYS:DISP?
132x64,flip,invert,brightness=75
```

#### SYStem:DISPlay:LAYOUTR
Configure (OLED) Display layout for the right side of the screen.

Layout is specified as a comma delimited string describing what to
display on each row (8 rows available if using 128x64 OLEd module, 10 rows available with 128x128 pixel modules).

Syntax: <R1>,<R2>,...<R8>

Where tow specifications can be one of the following:

Type|Description|Notes
----|-----------|-----
Mn|MBFan input n|n=1..4
Sn|Sensor input n|n=1..3
Vn|Virtual Sensor input n|n=1..8
-|Horizontal Line|
Ltext|Line with "text"|Max length 9 characters.


Default: <not set>

When this setting is not set following defaults are used based
on the OLED module size:

Screen Size|Available Rows|Default Configuration
-----------|--------------|---------------------
128x64|8|M1,M2,M3,M4,-,S1,S2,S3
128x128|10|LMB Inputs,M1,M2,M3,M4,-,LSensors,S1,S2,S3

Example: configure custom theme (for 128x64 display):
```
SYS:DISP:LAYOUTR M1,M2,-,S1,S2,S3,V1,V2
```

#### SYStem:DISPlay:LAYOUTR?
Display currently configured (OLED) Display layout for the right side of the screen.

Example:
```
SYS:DISP:THEME?
M1,M2,-,S1,S2,S3,V1,V2
```


#### SYStem:DISPlay:LOGO
Configure (LCD) Display boot logo.

Currently available logos:

Name|Description
----|-----------
default|Default FanPico boot logo.
custom|Custom boot logo (only available if firmware has been compiled with custom logo included).

Example: configure custom logo
```
SYS:DISP:LOGO custom
```

#### SYStem:DISPlay:LOGO?
Display currently configured (LCD) Display boot logo.

Example:
```
SYS:DISP:LOGO?
default
```


#### SYStem:DISPlay:THEMe
Configure (LCD) Display theme to use.

Default: default

Example: configure custom theme
```
SYS:DISP:THEME custom
```

#### SYStem:DISPlay:THEMe?
Display currently configured (LCD) display theme name.

Example:
```
SYS:DISP:THEME?
default
```


#### SYStem:ECHO
Enable or disaple local echo on the console.
This can be useful if interactively programming BrickPico.

Value|Status
-----|------
0|Local Echo disabled.
1|Local Echo enabled.

Default: 0

Example: enable local echo
```
SYS:ECHO 1
```

Example: disable local echo
```
SYS:ECHO 0
```

#### SYStem:ECHO?
Display local echo status:

Example:
```
SYS:ECHO?
0
```


### SYStem:FLASH?
Returns information about Pico flash memory usage.

Example:
```
SYS:FLASH?
Flash memory size:                     2097152
Binary size:                           683520
LittleFS size:                         262144
Unused flash memory:                   1151488
```


#### SYStem:OUTPUTS?
Display number of OUTPUT output ports available.

Example:
```
SYS:OUTPUTS?
8
```


#### SYStem:LED
Set system indicator LED operating mode.

Supported modes:

mode|description
----|-----------
0   | LED blinking slowly  [default]
1   | LED on (continuously)
2   | LED off


Default: 0

Example to set LED to be on continuously:
```
SYS:LED 1
```

#### SYStem:LED?
Query current system LED operating mode.

Example:
```
SYS:LED?
0
```


#### SYStem:LFS?
Display information about the LittleFS filesystem in the flash memory.

Example:
```
SYS:LFS?
Filesystem size:                       262144
Filesystem used:                       24576
Filesystem free:                       237568
Number of files:                       3
Number of subdirectories:              0
```


#### SYStem:LFS:FORMAT
Format flash filesystem. This will erase current configuration (including any TLS certificates saved in flash).

Example (format filesystem and save current configuration):
```
SYS:LFS:FORMAT
CONF:SAVE
```



### SYStem:MEM
Test how much available (heap) memory system currently has.
This does simple test to try to determine what is the largest
block of heap memory that is currently available as well as
try allocating as many as possible small block of memory to determine
roughly the total available heap memory.

This command takes optional 'blocksize' parameter to specify the memory
block size to use in the tests. Default is 1024 bytes.

Example:
```
SYS:MEM 512
Largest available memory block:        114688 bytes
Total available memory:                111104 bytes (217 x 512bytes)
```

### SYStem:MEM?
Returns information about heap and stack size. As well as information
about current (heap) memory usage as returned by _mallinfo()_ system call.

Note,  _mallinfo()_ doesn't "see" all of the available heap memory, unless ```SYS:MEM``` command
has been run first.

Example:
```
SYS:MEM?
Core0 stack size:                      8192
Core1 stack size:                      4096
Heap size:                             136604
mallinfo:
Total non-mmapped bytes (arena):       136604
# of free chunks (ordblks):            2
# of free fastbin blocks (smblks):     0
# of mapped regions (hblks):           0
Bytes in mapped regions (hblkhd):      0
Max. total allocated space (usmblks):  0
Free bytes held in fastbins (fsmblks): 0
Total allocated space (uordblks):      21044
Total free space (fordblks):           115560
Topmost releasable block (keepcost):   114808
```


### SYStem:MQTT Commands
BrickPico has MQTT Client that can be configured to publish (send) periodic status
updates to a topic.
Additionally MQTT Client support subscribing to a "command" topic to listen for commands.
This allows remotely controlling BrickPico.

To enable MQTT at minimum server must be configured. To explicitly disbable MQTT set server
to empty string.


#### SYStem:MQTT:SERVer
Set MQTT server to connect to. This parameter expects a DNS name as argument.

Default: <empty>   (when this setting is empty string, MQTT is explicitly disabled)

Example (configure MQTT server name):
```
SYS:MQTT:SERVER io.adafruit.com
```

Example (disable MQTT):
```
SYS:MQTT:SERVER
```

#### SYStem:MQTT:SERVer?
Query currently set MQTT server name.

Example:
```
SYS:MQTT:SERVER?
io.adafruit.com
```


#### SYStem:MQTT:PORT
Set MQTT server (TCP) port. This setting is needed when MQTT server is not using standard port.
If this setting is not set (value is left to default "0"), then standard MQTT port is used.

- Secure (TLS) Port = 8883
- Insecure Port = 1883

Default: 0   (when this setting is 0 use default MQTT ports)

Example:
```
SYS:MQTT:PORT 9883
```


#### SYStem:MQTT:PORT?
Query currently set MQTT (TCP) port.

If return value is zero (0), then default MQTT port is being used.

Example:
```
SYS:MQTT:PORT?
0
```


#### SYStem:MQTT:USER
Set MQTT username to use when connecting to MQTT server.

Default: <empty>

Example:
```
SYS:MQTT:USER myusername
```


#### SYStem:MQTT:USER?
Query currently set MQTT username.

Example:
```
SYS:MQTT:USER?
myusername
```


#### SYStem:MQTT:PASS
Set MQTT password to use when connecting to MQTT server.

Default: <empty>

Example:
```
SYS:MQTT:PASS mymqttpassword
```


#### SYStem:MQTT:PASS?
Query currently set MQTT password.

Example:
```
SYS:MQTT:PASS?
mymqttpassword
```


#### SYStem:MQTT:SCPI
Configure if SCPI commands will be accepted via MQTT.
This is potentially "dangerous" feature so only enable if you understand
the potential risks allowing device to be remotely configured.

Default: OFF

Example:
```
SYS:MQTT:SCPI ON
```


#### SYStem:MQTT:SCPI?
Query whether SCPI commands are allowed via MQTT.


Example:
```
SYS:MQTT:SCPI?
OFF
```


#### SYStem:MQTT:TLS
Enable/disable use of secure connection mode (TLS/SSL) when connecting to MQTT server.
Default is TLS on to protect MQTT credentials (usename/password).

Default: ON

Example:
```
SYS:MQTT:TLS OFF
```


#### SYStem:MQTT:TLS?
Query whether TLS is enabled or disabled for MQTT.

Example:
```
SYS:MQTT:TLS?
ON
```


#### SYStem:MQTT:HA:DISCovery
Configure Home Assistant MQTT Discovery prefix. This must be set to enable
Home Assistant support/integration in FanPico.

If this is left to empty (string), then Home Assistant support is disabled.

Default: <empty>

Example (enable Home Assistant support using default prefix):
```
SYS:MQTT:HA:DISC homeassistant
```


#### SYStem:MQTT:HA:DISCovery?
Query currently set Home Assistant MQTT Discovery prefix.

Note, if this is empty, then Home Assistant support is disabled.

Example:
```
SYS:MQTT:HA:DISC?
homeassistant
```


#### SYStem:MQTT:INTerval:STATus
Configure how often unit will publish (send) status message to status topic.
Set this to 0 (seconds) to disable publishing status updates.
Recommended values are 60 (seconds) or higher.

Default: 600  (every 10 minutes)

Example:
```
SYS:MQTT:INT:STAT 3600
```


#### SYStem:MQTT:INTerval:STATus?
Query currently set frequency for publishing unit status information.

Example:
```
SYS:MQTT:INT:STAT?
3600
```


#### SYStem:MQTT:INTerval:TEMP
Configure how often unit will publish (send) status message to temp topic.
Set this to 0 (seconds) to disable publishing temperature updates.
Recommended values are 60 (seconds) or higher.

Default: 60  (every 60 seconds)

Example:
```
SYS:MQTT:INT:TEMP 180
```


#### SYStem:MQTT:INTerval:TEMP?
Query currently set frequency for publishing unit temperature information.

Example:
```
SYS:MQTT:INT:TEMP?
180
```


#### SYStem:MQTT:INTerval:PWM
Configure how often unit will publish (send) status message to PWM topic.
Set this to 0 (seconds) to disable publishing PWM updates.
Recommended values are 60 (seconds) or higher.

Default: 600  (every 10 minutes)

Example:
```
SYS:MQTT:INT:PWM 300
```


#### SYStem:MQTT:INTerval:PWM?
Query currently set frequency for publishing unit PWM information.

Example:
```
SYS:MQTT:INT:PWM?
300
```


#### SYStem:MQTT:TOPIC:STATus
Configure topic to publish unit status information periodically.
If this is left to empty (string), then no status information is published to MQTT server.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:STAT musername/feeds/brickpico1
```


#### SYStem:MQTT:TOPIC:STATus?
Query currently set topic for publishing unit status information to.

Example:
```
SYS:MQTT:TOPIC:STAT?
myusername/feeds/brickpico1
```


#### SYStem:MQTT:TOPIC:COMMand
Configure topic to subscribe to to wait for commands to control outputs.
If this is left to empty (string), then unit won't subcrible (and accept) any commands from MQTT.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:COMM musername/feeds/cmd
```


#### SYStem:MQTT:TOPIC:COMMand?
Query currently set topic for subscribing to wait for commands.

Example:
```
SYS:MQTT:TOPIC:COMM?
myusername/feeds/cmd
```


#### SYStem:MQTT:TOPIC:RESPonse
Configure topic to publish responses to commands received from the command topic.
If this is left to empty, then unit won't send response to any commands.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:RESP musername/feeds/response
```


#### SYStem:MQTT:TOPIC:RESPonse?
Query currently set topic for publishing responses to commands.

Example:
```
SYS:MQTT:TOPIC:RESP?
myusername/feeds/response
```


#### SYStem:MQTT:TOPIC:ERRor
Configure topic to subscribe to for receiving error messages from MQTT broker.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:ERR mysername/errors
```


#### SYStem:MQTT:TOPIC:ERRor?
Query currently set topic for subscribing to receive error messages from MQTT broker.

Example:
```
SYS:MQTT:TOPIC:ERR?
myusername/errors
```


#### SYStem:MQTT:TOPIC:WARNing
Configure topic to subscribe to for receiving warning messages from MQTT broker.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:WARN mysername/throttle
```


#### SYStem:MQTT:TOPIC:WARNing?
Query currently set topic for subscribing to receive warning messages from MQTT broker.

Example:
```
SYS:MQTT:TOPIC:WARN?
myusername/throttle
```


#### SYStem:MQTT:TOPIC:TEMP
Configure topic to publish unit temperature data to.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:TEMP mysername/feeds/brickpico_temp
```


#### SYStem:MQTT:TOPIC:TEMP?
Query currently set topic for publishing temperature data.

Example:
```
SYS:MQTT:TOPIC:TEMP?
myusername/feeds/brickpico_temp
```


#### SYStem:MQTT:TOPIC:PWM
Configure topic to publish output port PWM status to.
This is a template string where ```%d``` should be used to mark the output port number.

Default: <empty>

Example:
```
SYS:MQTT:TOPIC:PWM mysername/feeds/brickpico_output%d
```


#### SYStem:MQTT:TOPIC:PWM?
Query currently set topic for publishing PWM data.

Example:
```
SYS:MQTT:TOPIC:PWM?
myusername/feeds/brickpico_output%d
```



#### SYStem:NAME
Set name of the system. (Default: fanpico1)

Example:
```
SYS:NAME HomeServer
```


#### SYStem:NAME?
Get name of the system.

Example:
```
SYS:NAME?
HomeServer
```


#### SYStem:PWMfreq
Set PWM frequency for the outputs. Supported range 10Hz - 100kHz.
When changing the frequency, chang will take effect after unit has been rebooted.

Default: 1000  (1kHz)

Example, set PWM frequency to 1.5kHz:
```
SYS:PWM 1500
CONF:SAVE
*RST
```


#### SYStem:PWMfreq?
Get current PWM frequency for the outputs.

Example:
```
SYS:PWM?
1000
```


#### SYStem:SERIAL
Enable or disable TTL Serial Console. This is enabled by default if board has this connector.
Reason to disable this could be to use the second I2C bus that is sharing pins with the UART.

Example (disable serial console):
```
SYS:SERIAL 0
```

#### SYStem:SERIAL?
Return status of TTL Serial Console.

Status|Description
------|-----------
1|Enabled
0|Disabled

Example:
```
SYS:SERIAL?
1
```

#### SYStem:SPI
Enable or disable SPI bus (on connector J18). When SPI bus is enabled I22 (OLED display) and Serial TTL connectors cannot be used
as these share pins with the SPI bus. 
Reason to enable SPI bus would be to connect LCD panel on the J18 connector.

Example (enable SPI bus):
```
SYS:SPI 1
```

#### SYStem:SPI?
Return status of SPI bus.

Status|Description
------|-----------
1|Enabled
0|Disabled

Example:
```
SYS:SPI?
0
```

#### SYStem:TELNET:SERVer
Control whether Telnet server is enabled or not.
After making change configuration needs to be saved and unit reset.

Default: OFF

Example:
```
SYS:TELNET:SERV ON
```

#### SYStem:TELNET:SERVer?
Display whether Telnet server status.

Example:
```
SYS:TELNET:SERV?
OFF
```


#### SYStem:TELNET:AUTH
Toggle Telnet server authentication mode. When enabled then Telnet server will
prompt user for login/password. When off, no authentication is needed.

Default: ON

Example:
```
SYS:TELNET:AUTH OFF
```

#### SYStem:TELNET:AUTH?
Display whether Telnet server authentication is enabled or not.

Example:
```
SYS:TELNET:AUTH?
ON
```


#### SYStem:TELNET:PORT
Set TCP port where Telnet server will listen on.
If this setting is not set then default port will be used.

Default: 23 (default Telnet port)

Example:
```
SYS:TELNET:PORT 8000
```

#### SYStem:TELNET:PORT?
Display currently configured port for Telnet server.

(if port is set to 0, then default Telnet port will be used)

Example:
```
SYS:TELNET:PORT?
8000
```


#### SYStem:TELNET:RAWmode
Configure Telnet server mode. By default Telnet server uses Telnet protocol, but
setting this option causes Telnet protocol to be disabled. And server uses "raw TCP" mode.

Default: OFF

Example:
```
SYS:TELNET:RAW ON
```

#### SYStem:TELNET:RAWmode?
Display if "raw TCP" mode is enabled or not.

Example:
```
SYS:TELNET:RAW?
OFF
```

#### SYStem:TELNET:USER
Configure username that is allowed to login to this server using Telnet.

Default: <none>

Example:
```
SYS:TELNET:USER admin
```

#### SYStem:TELNET:USER?
Display currently configured telnet user (login) name.

Example:
```
SYS:TELNET:USER?
admin
```


#### SYStem:TELNET:PASSword
Configure password for the telnet user. Password is hashed using SHA-512 Crypt algorithm.

Default: <none>

Example:
```
SYS:TELNET:PASS mypassword
```

#### SYStem:TELNET:PASSword?
Display currently configured telnet user password hash.

Example:
```
SYS:TELNET:PASS?
$6$QvD5AkWSuydeH/EB$UsYA0cymsCRSse78fN4bMb5q0hM5B7YUNSFd3zJfMDbTG7DOH8iuMufVjsvqBOxR9YCJYSHno4CFeOhLtTGLx.
```



#### SYStem:TIME
Set system Real-Time Clock (RTC) time.

This command expects time in following format:
  YYYY-MM-DD HH:MM:SS

Example:
```
SYS:TIME 2022-09-19 18:55:42
```

#### SYStem:TIME?
Return current Real-Time Clock (RTC) time.
This is only available if using Pico W and it has successfully
gotten time from a NTP server or RTC has been initialized using
SYStem:TIME command.

Command returns nothing if RTC has not been initialized.

Example:
```
SYS:TIME?
2022-09-19 18:55:42
```

#### SYStem:TIMEZONE
Set POSIX timezone to use when getting time from a NTP server.
If DHCP server does not supply POSIX Timezone (DHCP Option 100), then this
command can be used to specify local timezone.

This command takes POSIX timezone string as argument (or if argument is blank,
then it clears existinh timezone setting).

Example (set Pacific Standard time as local timezone):
```
SYS:TIMEZONE PST8PDT7,M3.2.0/2,M11.1.0/02:00:00
```

Example (clear timezone setting):
```
SYS:TIMEZONE
```

#### SYStem:TIMEZONE?
Return current POSIX timezone setting.

Command returns nothing if no timezone has been set.

Example:
```
SYS:TIMEZONE?
PST8PDT7,M3.2.0/2,M11.1.0/02:00:00
```

#### SYStem:TLS:CERT
Upload or delete TLS certificate for the HTTP server.
Note, both certificate and private key must be installed before HTTPS server will
activate (when system is restarted next time).

When run without arguments this will prompt to paste TLS (X.509) certificate
in PEM format.  When run with "DELETE" argument currently installed certificate
will be deleted.

Example (upload/paste certificate):
```
SYS:TLS:CERT
Paste certificate in PEM format:

```

Example (delete existing certificate from flash memory):
```
SYS:TLS:CERT DELETE
```

#### SYStem:TLS:CERT?
Display currently installed certificate.

Example:
```
SYS:TLS:CERT?
```


#### SYStem:TLS:PKEY
Upload or delete (TLS Certificate) Private key for the HTTP server.
Note, both certificate and private key must be installed before HTTPS server will
activate (when system is restarted next time).

When run without arguments this will prompt to paste private key
in PEM format.  When run with "DELETE" argument currently installed private key
will be deleted.

Example (upload/paste private key):
```
SYS:TLS:PKEY
Paste private key in PEM format:

```

Example (delete existing private key from flash memory):
```
SYS:TLS:PKEY DELETE
```

#### SYStem:TLS:PKEY?
Display currently installed private key.

Example:
```
SYS:TLS:CERT?
```


#### SYStem:UPTIme?
Return time elapsed since unit was last rebooted.

Example:
```
SYS:UPTIME?
up 4 days, 22 hours, 27 minutes
```


#### SYStem:UPGRADE
Reboot unit to USB (BOOTSEL) mode for firmware upgrade.
This command triggers fanpico to reboot and enter USB "mode", where
new firmware can simply be copied to the USB drive that appears.
After file has been copied, fanpico will automatically reboot to new
firmware image.

Example:
```
SYS:VER?
```


#### SYStem:VERsion?
Display software version and copyright information.

Example:
```
SYS:VER?
```


#### SYStem:WIFI?
Check if the unit support WiFi networking.
This should be used to determine if any other "WIFI"
commands will be available.

Return values:

0 = No WiFi support (Pico).
1 = WiFi supported (Pico W).

Example:
```
SYS:WIFI?
1
```

#### SYStem:WIFI:AUTHmode
Set Wi-Fi Authentication mode.

Following modes are currently supported:


Mode|Description|Notes
----|-----------|-----
default|Use system default|Currently default is WPA2
WPA3_WPA2|Use WPA3/WPA2 (mixed) mode|
WPA3|Use WPA3 only|
WPA2|Use WPA2 only|
WPA2_WPA|Use WPA2/WPA (mixed) mode|
WPA|Use WPA only|
OPEN|Use "Open" mode|No authentication.


Example:
```
SYS:WIFI:AUTH WPA3_WPA2
```


#### SYStem:WIFI:AUTHmode?
Return currently configured Authentication mode for the WiFi interface.

Example:

```
SYS:WIFI:AUTH?
default
```

#### SYStem:WIFI:COUntry
Set Wi-Fi Country code. By default, the country setting for the wireless adapter is unset.
This means driver will use default world-wide safe setting, which can mean that some channels
are unavailable.

Country codes are two letter (ISO 3166) codes. For example, Finland = FI, Great Britain = GB,
United States of Americ = US, ...

Example:
```
SYS:WIFI:COUNTRY US
```


#### SYStem:WIFI:COUntry?
Return currently configured country code for the Wi-Fi interface.

Codes used are the ISO 3166 standard two letter codes ('XX' means unset/worldwide setting).

Example:

```
SYS:WIFI:COUNTRY?
US
```

#### SYStem:WIFI:HOSTname
Set system hostname. This will be used with DHCP and SYSLOG.
If hostname is not defined then system will default to generating
hostname as follows:
  BrickPico-xxxxxxxxxxxxxxxx

(where "xxxxxxxxxxxxxxxx" is the BrickPico serial number)

Example:
```
SYS:WIFI:HOSTNAME fanpico1
```


#### SYStem:WIFI:HOSTname?
Return currently configured system hostname.

Example:

```
SYS:WIFI:HOSTNAME?
fanpico1
```


#### SYStem:WIFI:IPaddress
Set staticlly configured IP address.

Set address to "0.0.0.0" to enable DHCP.

Example:

```
SYS:WIFI:IP 192.168.1.42
```

#### SYStem:WIFI:IPaddress?
Display currently configured (static) IP address.
If no static address is configured, DHCP will be used.

Set address to "0.0.0.0" to enable DHCP.

Example:

```
SYS:WIFI:IP?
0.0.0.0
```


#### SYStem:WIFI:MODE
Set WiFi connection mode. Normally this setting is not needed with modern APs.

However, if FanPico is failing to connect to WiFi network, this couldbe
due to old firmware on the AP (upgrading to latest firmware typically helps).
If firmware update did not help or there is no updated firmware available, setting
connection mode to synchronous can help (however this could cause FanPico to "hang" for up to 60 seconds
during boot up).


Mode|Description
------|-----------
0|Asynchronous connection mode (default)
1|Synchronous connection mode

Default: 0


Example:

```
SYS:WIFI:MODE 1
```

#### SYStem:WIFI:MODE?
Display currently configured WiFi connection mode?

Example:

```
SYS:WIFI:MODE?
0

```

#### SYStem:WIFI:NETMASK
Set statically configured netmask.


Example:

```
SYS:WIFI:NETMASK 255.255.255.0
```

#### SYStem:WIFI:NETMASK?
Display currently configured (static) netmask.

Example:

```
SYS:WIFI:NETMASK?
255.255.255.0
```


#### SYStem:WIFI:GATEWAY
Set statically configured default gateway (router).


Example:

```
SYS:WIFI:GATEWAY 192.168.1.1
```

#### SYStem:WIFI:GATEWAY?
Display currently configured default gateway (router).

Example:

```
SYS:WIFI:GATEWAY?
192.168.1.1
```


#### SYStem:WIFI:NTP
Configure IP for NTP server to use.

Set to "0.0.0.0" to use server provided by DHCP.

Example:

```
SYS:WIFI:NTP 192.168.1.10
```

#### SYStem:WIFI:NTP?
Display currently configure NTP server.

Note, "0.0.0.0" means to use DHCP.

Example:

```
SYS:WIFI:NTP?
192.168.1.10
```



#### SYStem:WIFI:SYSLOG
Configure IP for Syslog server to use.

Set to "0.0.0.0" to use server provided by DHCP.

Example:

```
SYS:WIFI:SYSLOG 192.168.1.20
```

#### SYStem:WIFI:SYSLOG?
Display currently configured Syslog server.

Note, "0.0.0.0" means to use DHCP.

Example:

```
SYS:WIFI:SYSLOG?
192.168.1.20
```


#### SYStem:WIFI:MAC?
Display WiFi adapter MAC (Ethernet) address.

Example:

```
SYS:WIFI:MAC?
28:cd:c1:01:02:03
```


#### SYStem:WIFI:SSID
Set Wi-Fi network SSID. FanPico will automatically try joining to this network.

Example
```
SYS:WIFI:SSID mynetwork
```


#### SYStem:WIFI:SSID?
Display currently configured Wi-Fi network SSID.

Example:

```
SYS:WIFI:SSID?
mynetwork
```

#### SYStem:WIFI:STATus?
Display WiFi Link status.

Return value: linkstatus,current_ip,current_netmask,current_gateway

Link Status:

Value|Description
-----|-----------
0    |Link is down.
1    |Connected to WiFi.
2    |Connected to WiFi, but no IP address.
3    |Connected to WiFi with and IP address.
-1   |Connection failed.
-2   |No matching SSID found(could be out of range, or down).
-3   |Authentication failed (wrong password?)

Example:

```
SYS:WIFI:STAT?
1,192.168.1.42,255.255.255.0,192.168.1.1
```

#### SYStem:WIFI:STATS?
Display TCP/IP stack (LwIP) statistics.

NOTE, this command only works on "Debug" builds of the firmware currently.

Example
```
SYS:WIFI:STATS?
```

#### SYStem:WIFI:PASSword
Set Wi-Fi (PSK) password/passrase.

Example
```
SYS:WIFI:PASS mynetworkpassword
```


#### SYStem:WIFI:PASSword?
Display currently configured Wi-Fi (PSK) password/passphrase.

Example:

```
SYS:WIFI:PASS?
mynetworkpassword
```


### WRIte Commands

These commands are for turning output ports ON/OFF and for adjusting output PWM duty cycle.


#### WRIte:OUTPUTx

Turn output on or off.

Example: Turn OUTPUT1 on.
```
WRITE:OUTPUT1 ON
```

Example: Turn OUTPUT1 off.
```
WRITE:OUTPUT1 OFF
```


#### WRIte:OUTPUTx:PWM

Set output PWM duty cycle. Valid values are from 0 to 100.

Example: Set OUTPUT2 to 25% duty cycle.
```
WRITE:OUTPUT2:PWM 25
```

#### WRIte:OUTPUTx:STAte

Turn output  on or off. This is same as WRIte:OUTPUTx command.

Example: Turn OUTPUT1 on.
```
WRITE:OUTPUT1:STATE ON
```

Example: Turn OUTPUT1 off.
```
WRITE:OUTPUT1:STATE OFF
```


