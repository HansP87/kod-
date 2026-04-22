*** Settings ***
Library    ../../libraries/DevBoardLibrary.py

*** Keywords ***
Prepare Session
    Build Firmware
    Flash Firmware
    ${port}=    Auto Detect Serial Port
    Open Uart    ${port}    250000

Wait For Stream Packet
    ${packet}=    Read Tx Packet    5
    RETURN    ${packet}