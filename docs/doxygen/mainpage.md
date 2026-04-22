# devADC-dk Firmware

@mainpage devADC-dk Firmware Reference

## Overview

This documentation covers the custom application code used by the `devADC-dk` firmware for the STM32H7B3I-DK platform.

The documented code focuses on:

- application mode control
- command/configuration protocol handling
- ADC sampling and DSP pipeline processing
- packet ownership and transmission
- monitor and task coordination services

## Data Path

The runtime pipeline is:

1. DMA-backed ADC frames are captured on ADC1 and ADC2.
2. The sampling pipeline converts raw values to engineering units.
3. A CIC decimator and post-filtering stages prepare filtered outputs.
4. `tx_packet_service` publishes packets for normal streaming or high-rate capture.
5. `transmit_service` serializes the selected packet into the wire protocol.

## Architecture Diagrams

### Sampling And Transport Flow

\dot
digraph sampling_flow {
	graph [rankdir=LR, bgcolor="transparent"];
	node [shape=box, style="rounded,filled", fillcolor="#eef5ff", color="#335c81", fontname="Helvetica"];
	edge [color="#4f6d7a", arrowsize=0.8];

	adc1 [label="ADC1 DMA\nranked analog channels"];
	adc2 [label="ADC2 DMA\ninternal + analog channels"];
	dsp [label="sampling_pipeline_service\nraw conversion\nCIC decimation\n100/200/300 Hz notch filters"];
	pkt [label="tx_packet_service\npacket ownership\npublish latest frame"];
	tx [label="transmit_service\nnormal binary stream\nhigh-rate capture stream"];
	uart [label="USART1\nCOBS-framed machine protocol", fillcolor="#fdf3e7", color="#b26b00"];

	adc1 -> dsp;
	adc2 -> dsp;
	dsp -> pkt;
	pkt -> tx;
	tx -> uart;
}
\enddot

### Task And Control Interaction

\dot
digraph task_flow {
	graph [rankdir=TB, bgcolor="transparent"];
	node [shape=box, style="rounded,filled", fillcolor="#eefaf0", color="#3d6b35", fontname="Helvetica"];
	edge [color="#577590", arrowsize=0.8];

	irq [label="HAL callbacks / IRQs", fillcolor="#fff4e6", color="#a65e2e"];
	dsp_task [label="dsp_task\nwaits on DSP_FLAG_TICK"];
	cmd_task [label="command_task\nASCII config + binary capture requests"];
	mon_task [label="monitor_task\nboot banner + button markers"];
	tx_task [label="transmit_task\nwaits on TX_FLAG_SEND"];

	irq -> dsp_task [label="LPTIM2 tick"];
	irq -> cmd_task [label="USART1 RX bytes"];
	irq -> mon_task [label="button press state"];
	dsp_task -> tx_task [label="normal stream request"];
	dsp_task -> tx_task [label="high-rate frame ready"];
	cmd_task -> tx_task [label="start high-rate capture"];
	mon_task -> tx_task [label="banner / trigger send"];
}
\enddot

## Operating Modes

- `APP_MODE_STREAMING`: normal binary stream output is enabled.
- `APP_MODE_CONFIG`: streaming is paused and command/configuration responses are served over UART.

## Generated Content

The HTML output is configured to include the custom headers and source files under `Core/Inc` and `Core/Src`, while excluding generated build artefacts and third-party middleware trees.
