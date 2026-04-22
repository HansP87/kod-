*** Settings ***
Resource    resources/common.resource
Suite Setup    Prepare Session
Suite Teardown    Close Uart

*** Test Cases ***
Packet Flags Stay Clear
    ${packet}=    Wait For Stream Packet
    Packet Field Should Equal    ${packet}    flags    0

Vdda And Temperature Stay In Narrow Range
    ${packet}=    Wait For Stream Packet
    Filtered Field Should Be Within Range    ${packet}    0    vdda_mv    3250    3350
    Filtered Field Should Be Within Range    ${packet}    0    temp_c    15    40

Internal Reference Channel Stays Stable
    ${packet}=    Wait For Stream Packet
    Filtered Channel Should Be Within Range    ${packet}    0    adc2    3    1180    1260

External ADC Channels Stay Near Expected Idle Range
    ${packet}=    Wait For Stream Packet
    Filtered Channel Should Be Within Range    ${packet}    0    adc1    0    0    500
    Filtered Channel Should Be Within Range    ${packet}    0    adc1    1    0    500
    Filtered Channel Should Be Within Range    ${packet}    0    adc1    2    0    500
    Filtered Channel Should Be Within Range    ${packet}    0    adc1    3    0    500

High Rate Cobs Stream Captures Filtered Channels
    ${frames}=    Capture High Rate Stream    1000    20
    High Rate Flags Should Equal    ${frames}    0
    ${adc1_rank1_stdev}=    High Rate Channel Should Be Stable    ${frames}    adc1    0    50
    Log    ADC1 rank 1 stddev=${adc1_rank1_stdev}
    ${plot_path}=    Plot High Rate Filtered Channels    ${frames}    high_rate_filtered_channels.png
    Log    Plot saved to ${plot_path}
    ${jitter_plot_path}=    Plot High Rate Age Jitter    ${frames}    high_rate_age_jitter.png
    Log    Jitter plot saved to ${jitter_plot_path}
    ${fft_plot_path}=    Plot High Rate Channel Fft    ${frames}    high_rate_channel_fft.png
    Log    FFT plot saved to ${fft_plot_path}
