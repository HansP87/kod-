*** Settings ***
Resource    resources/common.resource
Suite Setup    Prepare Session
Suite Teardown    Close Uart

*** Test Cases ***
Boot Announces Mcu Serial
    ${serial}=    Wait For Startup Banner    5
    Should Match Regexp    ${serial}    ^[0-9A-F]{24}$

Auto Trigger Produces A Packet
    Wait For Line Matching    ^TEST:AUTO_TRIGGER$    5
    ${packet}=    Read Tx Packet    5
    Packet Field Should Be Within Range    ${packet}    age_us    0    1000000
    Packet Field Should Be Within Range    ${packet}    ready_us    0    4294967295
    Sample Field Should Be Within Range    ${packet}    0    vdda_mv    3000    3400
    Sample Field Should Be Within Range    ${packet}    0    temp_c    0    80
    Sample Field Should Be Within Range    ${packet}    1    vdda_mv    3000    3400
    Sample Field Should Be Within Range    ${packet}    1    temp_c    0    80
