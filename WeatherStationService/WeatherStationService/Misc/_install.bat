cls
REM make directory the one in which the InstallUtil.exe file is stored
e:
cd "E:\Documents\VBNet\WeatherStationService\WeatherStationService\Misc"
InstallUtil.exe  "..\bin\Debug\WeatherStationService.exe"
sc description WeatherStationService "Solar Weather Station webpage and log file service updates"
sc config WeatherStationService start=auto
time /t
pause