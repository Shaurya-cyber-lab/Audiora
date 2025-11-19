@echo off
REM Test script to display the playlist interface

echo Testing Audiora Playlist Display...
echo.
echo Press 3 to Display Playlist, then 9 to Exit
echo.
echo 3 > input.txt
echo 9 >> input.txt

audiora.exe < input.txt
