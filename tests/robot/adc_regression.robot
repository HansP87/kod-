*** Settings ***
Resource    resources/common.resource
Suite Setup    Prepare Session
Suite Teardown    Close Uart

*** Test Cases ***
Packet Flags Stay Clear
    ${packet}=    Wait For Auto Trigger Packet
    Packet Field Should Equal    ${packet}    flags    0

Vdda And Temperature Stay In Narrow Range
    ${packet}=    Wait For Auto Trigger Packet
    Sample Field Should Be Within Range    ${packet}    0    vdda_mv    3250    3350
    Sample Field Should Be Within Range    ${packet}    1    vdda_mv    3250    3350
    Sample Field Should Be Within Range    ${packet}    0    temp_c    15    40
    Sample Field Should Be Within Range    ${packet}    1    temp_c    15    40

Internal Reference Channel Stays Stable
    ${packet}=    Wait For Auto Trigger Packet
    Sample Channel Should Be Within Range    ${packet}    0    adc2    3    1180    1260
    Sample Channel Should Be Within Range    ${packet}    1    adc2    3    1180    1260

External ADC Channels Stay Near Expected Idle Range
    ${packet}=    Wait For Auto Trigger Packet
    Sample Channel Should Be Within Range    ${packet}    0    adc1    0    0    500
    Sample Channel Should Be Within Range    ${packet}    0    adc1    1    0    500
    Sample Channel Should Be Within Range    ${packet}    0    adc1    2    0    500
    Sample Channel Should Be Within Range    ${packet}    0    adc1    3    0    500
    Sample Channel Should Be Within Range    ${packet}    1    adc1    0    0    500
    Sample Channel Should Be Within Range    ${packet}    1    adc1    1    0    500
    Sample Channel Should Be Within Range    ${packet}    1    adc1    2    0    500
    Sample Channel Should Be Within Range    ${packet}    1    adc1    3    0    500
