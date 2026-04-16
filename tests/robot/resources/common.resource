*** Settings ***
Library    ../../libraries/DevBoardLibrary.py

*** Keywords ***
Prepare Session
    Build Firmware
    Flash Firmware
    ${port}=    Auto Detect Serial Port
    Open Uart    ${port}    115200

Wait For Auto Trigger Packet
    Wait For Line Matching    ^TEST:AUTO_TRIGGER$    5
    ${packet}=    Read Tx Packet    5
    RETURN    ${packet}