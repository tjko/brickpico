# BrickPico: Command Reference
BrickPico uses "SCPI like" command set. Command syntax should be mostly SCPI and IEE488.2 compliant,
to make it easier to control and configure Fanpico units.


## Command Set
BrickPico supports following commands:

* [*IDN?](#idn)
* [*RST](#rst)
* [CONFigure?](#configure)
* [CONFigure:SAVe](#configuresave)
* [CONFigure:Read?](#configureread)
* [CONFigure:DELete](#configuredelete)
* [CONFigure:OUTPUTx:NAME](#configureoutputxname)
* [CONFigure:OUTPUTx:NAME?](#configureoutputxname-1)
* [CONFigure:OUTPUTx:DEFault](#configureoutputxdefault)
* [CONFigure:OUTPUTx:DEFault?](#configureoutputxdefault-1)
* [CONFigure:OUTPUTx:MINpwm](#configureoutputxminpwm)
* [CONFigure:OUTPUTx:MINpwm?](#configureoutputxminpwm-1)
* [CONFigure:OUTPUTx:MAXpwm](#configureoutputxmaxpwm)
* [CONFigure:OUTPUTx:MAXpwm?](#configureoutputxmaxpwm-1)
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
* [SYStem:ECHO?](#systemecho)
* [SYStem:OUTputs?](#systemoutputs)
* [SYStem:LED](#systemled)
* [SYStem:LED?](#systemled-1)
* [SYStem:NAME](#systemname)
* [SYStem:NAME?](#systemname-1)
* [SYStem:SERIAL](#systemserial)
* [SYStem:SERIAL?](#systemserial-1)
* [SYStem:SPI](#systemspi)
* [SYStem:SPI?](#systemspi-1)
* [SYStem:TIME](#systemtime)
* [SYStem:TIME?](#systemtime-1)
* [SYStem:UPTIme?](#systemuptime)
* [SYStem:UPGRADE](#systemupgrade)
* [SYStem:VERsion?](#systemversion)
* [SYStem:WIFI?](#systemwifi)
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

#### CONFigure:SAVe
Save current configuration into flash memory.

Example:
```
CONF:SAVE
```

#### CONFigure:Read?
Display current configuration in JSON format.

Example:
```
CONF:READ?
```

#### CONFigure:DELete
Delete current configuration saved into flash. After unit has been reset it will be using default configuration.

Example:
```
CONF:DEL
*RST
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

#### CONFigure:OUTPUTx:DEFault
Set default PWM duty cycle (%) for given output port.
This is value set when unit boots up and normally defaults
to 0% (off).

Default: 0 %

Example: Set default PWM duty cycle to 5% for OUTPUT1
```
CONF:OUTPUT1:DEF 5
```

#### CONFigure:OUTPUTx:DEFault?
Query current default PWM duty cycle for a output port.

Example:
```
CONF:OUTPUT1:DEF?
5
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



### MEASure Commands
These commands are for reading (measuring) the current output values on ports.

#### MEASure:Read?
This command returns current output settings on all OUTPUT ports.
(This is same as: Read?)

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

Layout is specified as a comma delimited string descibing what to
display on each row (8 rows available if using 128x64 OLEd module, 10 rows available with 128x128 pixel modules).

Syntax: <R1>,<R2>,...<R8>

Where tow specifications can be one of the following:

Type|Description|Notes
----|-----------|-----
Mn|MBFan input n|n=1..4
Sn|Sensor input n|n=1..3
Vn|Virtual Sensor input n|n=1..8
-|Horizontal Line|
Ltext|Line with "text"|Max lenght 9 characters.


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


#### SYStem:VSENSORS?
Display number of virtual (temperature) sensors available.

Example:
```
SYS:VSENSORS?
8
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
due to old firmware on the AP (upgrading to latest firmare typically helps).
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
Display currently configured WiFi connection mdoe?

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

These commands are for setting/adjusting output port PWM signal values.

#### WRIte:OUTPUTx

Set/update PWM duty cycle for a outpuyt port.

Example: Set OUTPUT1 to 50% duty cycle
```
WRITE:OUTPUT1 50.0
```

