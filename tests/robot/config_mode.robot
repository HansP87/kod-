*** Settings ***
Resource    resources/common.resource
Suite Setup    Prepare Session
Suite Teardown    Close Uart

*** Test Cases ***
Entering Config Mode Stops Periodic Streaming
    Enter Config Mode
    Wait For No Line Matching    ^(TEST:|SEQ:)    3

Mcu Serial Command Returns Unique Id
    Enter Config Mode
    Send Config Command    mcu_serial
    ${response}=    Wait For Config Response    MCU_SERIAL    5
    Response Parameter Should Match    ${response}    0    ^[0-9A-F]{24}$

Reset Command Emits Startup Serial Banner
    Enter Config Mode
    Send Config Command    mcu_serial
    ${serial_response}=    Wait For Config Response    MCU_SERIAL    5
    ${expected_serial}=    Set Variable    ${serial_response}[parameters][0]
    Send Config Command    reset
    ${reset_response}=    Wait For Config Response    RESET    5
    Response Parameter Should Equal    ${reset_response}    0    REBOOTING
    ${startup_serial}=    Wait For Startup Banner    5
    Values Should Be Equal    ${startup_serial}    ${expected_serial}
    Wait For Line Matching    ^TEST:AUTO_TRIGGER$    5

Exit Config Mode Restores Streaming
    Enter Config Mode
    Send Config Command    exit_config
    ${response}=    Wait For Config Response    EXIT_CONFIG    5
    Response Parameter Should Equal    ${response}    0    STREAMING
    Wait For Line Matching    ^TEST:AUTO_TRIGGER$    5
    Read Tx Packet    5

Bad Crc Returns Error Response
    Enter Config Mode
    Send Config Command With Crc    mcu_serial    00
    ${response}=    Wait For Config Response    ERROR    5
    Response Parameter Should Equal    ${response}    0    BADCRC

Unknown Command Returns Error Response
    Enter Config Mode
    Send Config Command    unsupported_command
    ${response}=    Wait For Config Response    ERROR    5
    Response Parameter Should Equal    ${response}    0    UNKNOWN_COMMAND

Malformed Frame Returns Error Response
    Enter Config Mode
    Send Command    @
    ${response}=    Wait For Config Response    ERROR    5
    Response Parameter Should Equal    ${response}    0    MALFORMED

Bad Sign Returns Error Response
    Enter Config Mode
    Send Config Command With Sign    !    mcu_serial
    ${response}=    Wait For Config Response    ERROR    5
    Response Parameter Should Equal    ${response}    0    BADSIGN