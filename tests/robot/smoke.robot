*** Settings ***
Resource    resources/common.resource
Suite Setup    Prepare Session
Suite Teardown    Close Uart

*** Test Cases ***
Boot Starts Continuous Streaming
    ${packet}=    Wait For Stream Packet
    Packet Field Should Be Within Range    ${packet}    flags    0    0

Streaming Produces A Packet
    ${packet}=    Wait For Stream Packet
    Packet Field Should Be Within Range    ${packet}    age_us    0    1000000
    Filtered Field Should Be Within Range    ${packet}    0    vdda_mv    3000    3400
    Filtered Field Should Be Within Range    ${packet}    0    temp_c    0    80
