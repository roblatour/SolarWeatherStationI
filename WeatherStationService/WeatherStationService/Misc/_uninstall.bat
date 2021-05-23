cls
REM make directory the one in which the InstallUtil.exe file is stored
e:
cd "E:\Documents\VBNet\WeatherStationService\WeatherStationService\Misc"
net stop WeatherStationService
InstallUtil.exe  /u "..\bin\Release\WeatherStationService.exe"
time /t
pause