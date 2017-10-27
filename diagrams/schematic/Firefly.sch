EESchema Schematic File Version 2
LIBS:power
LIBS:device
LIBS:transistors
LIBS:conn
LIBS:linear
LIBS:regul
LIBS:74xx
LIBS:cmos4000
LIBS:adc-dac
LIBS:memory
LIBS:xilinx
LIBS:microcontrollers
LIBS:dsp
LIBS:microchip
LIBS:analog_switches
LIBS:motorola
LIBS:texas
LIBS:intel
LIBS:audio
LIBS:interface
LIBS:digital-audio
LIBS:philips
LIBS:cypress
LIBS:siliconi
LIBS:opto
LIBS:atmel
LIBS:contrib
LIBS:valves
LIBS:FireflyComponents
LIBS:switches
LIBS:Firefly-cache
EELAYER 25 0
EELAYER END
$Descr A4 11693 8268
encoding utf-8
Sheet 1 1
Title "Preliminary Firefly Schematic"
Date "2017-10-24"
Rev "0.1"
Comp ""
Comment1 ""
Comment2 "if you are familiar with circuit design and/or kicad."
Comment3 "many changes. Please feel free to update this document"
Comment4 "This is a preliminary document which will likely undergo"
$EndDescr
$Comp
L Display OLED1
U 1 1 59EEC845
P 3350 4100
F 0 "OLED1" H 3350 4100 60  0000 C CNN
F 1 "Display" H 3700 3500 60  0000 C CNN
F 2 "" H 3350 4000 60  0001 C CNN
F 3 "" H 3350 4000 60  0001 C CNN
	1    3350 4100
	1    0    0    -1  
$EndComp
$Comp
L R 10kΩ
U 1 1 59EEC8B2
P 6400 5100
F 0 "10kΩ" V 6480 5100 50  0000 C CNN
F 1 "R1" V 6400 5100 50  0000 C CNN
F 2 "" V 6330 5100 50  0001 C CNN
F 3 "" H 6400 5100 50  0001 C CNN
	1    6400 5100
	1    0    0    -1  
$EndComp
$Comp
L SW_Push SW1
U 1 1 59EECD23
P 5700 4850
F 0 "SW1" H 5750 4950 50  0000 L CNN
F 1 "SW_Push" H 5700 4790 50  0000 C CNN
F 2 "" H 5700 5050 50  0001 C CNN
F 3 "" H 5700 5050 50  0001 C CNN
	1    5700 4850
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59EECD81
P 8300 2150
F 0 "#PWR?" H 8300 1900 50  0001 C CNN
F 1 "GND" H 8300 2000 50  0000 C CNN
F 2 "" H 8300 2150 50  0001 C CNN
F 3 "" H 8300 2150 50  0001 C CNN
	1    8300 2150
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59EECDA4
P 6400 5450
F 0 "#PWR?" H 6400 5200 50  0001 C CNN
F 1 "GND" H 6400 5300 50  0000 C CNN
F 2 "" H 6400 5450 50  0001 C CNN
F 3 "" H 6400 5450 50  0001 C CNN
	1    6400 5450
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59EECDC7
P 5050 4500
F 0 "#PWR?" H 5050 4250 50  0001 C CNN
F 1 "GND" H 5050 4350 50  0000 C CNN
F 2 "" H 5050 4500 50  0001 C CNN
F 3 "" H 5050 4500 50  0001 C CNN
	1    5050 4500
	1    0    0    -1  
$EndComp
$Comp
L GND #PWR?
U 1 1 59EECDEA
P 2500 3050
F 0 "#PWR?" H 2500 2800 50  0001 C CNN
F 1 "GND" H 2500 2900 50  0000 C CNN
F 2 "" H 2500 3050 50  0001 C CNN
F 3 "" H 2500 3050 50  0001 C CNN
	1    2500 3050
	1    0    0    -1  
$EndComp
$Comp
L nRF24L01+ Radio1
U 1 1 59EEE67C
P 7500 1700
F 0 "Radio1" H 7450 2100 60  0000 C CNN
F 1 "nRF24L01+" H 7800 1300 60  0000 C CNN
F 2 "" H 7850 1900 60  0001 C CNN
F 3 "" H 7850 1900 60  0001 C CNN
	1    7500 1700
	1    0    0    -1  
$EndComp
$Comp
L ArduinoNano PC1
U 1 1 59EEC47E
P 5700 3600
F 0 "PC1" H 5700 4500 60  0000 C CNN
F 1 "Arduino Nano" H 5700 2700 60  0000 C CNN
F 2 "" H 5700 2700 60  0001 C CNN
F 3 "" H 5700 2700 60  0001 C CNN
	1    5700 3600
	1    0    0    -1  
$EndComp
Wire Wire Line
	4700 2900 4700 3700
Wire Wire Line
	4700 3700 5150 3700
Wire Wire Line
	3400 2800 4800 2800
Wire Wire Line
	4800 2800 4800 3800
Wire Wire Line
	4800 3800 5150 3800
Wire Wire Line
	3300 2700 4900 2700
Wire Wire Line
	4900 2700 4900 4850
Wire Wire Line
	4900 4000 5150 4000
Wire Wire Line
	4900 4850 5500 4850
Connection ~ 4900 4000
Wire Wire Line
	6400 3900 6250 3900
Wire Wire Line
	6400 3900 6400 4950
Wire Wire Line
	6400 4850 5900 4850
Connection ~ 6400 4850
Wire Wire Line
	3200 3400 3200 2700
Wire Wire Line
	3200 2700 2500 2700
Wire Wire Line
	2500 2700 2500 3050
Wire Wire Line
	3300 3400 3300 2700
Wire Wire Line
	3400 2800 3400 3400
Wire Wire Line
	4700 2900 3500 2900
Wire Wire Line
	3500 2900 3500 3400
Wire Wire Line
	7100 2200 7100 2400
Wire Wire Line
	7100 2400 5000 2400
Wire Wire Line
	5000 2400 5000 2900
Wire Wire Line
	5000 2900 5150 2900
Wire Wire Line
	7000 2200 7000 3100
Wire Wire Line
	7000 3100 6250 3100
Wire Wire Line
	7200 2200 7200 3000
Wire Wire Line
	7200 3000 6250 3000
Wire Wire Line
	6250 2900 7300 2900
Wire Wire Line
	7300 2900 7300 2200
Wire Wire Line
	5150 3000 5100 3000
Wire Wire Line
	5100 3000 5100 2500
Wire Wire Line
	5100 2500 8600 2500
Wire Wire Line
	8600 2500 8600 1800
Wire Wire Line
	8600 1800 8200 1800
Wire Wire Line
	8300 2150 8300 1900
Wire Wire Line
	8300 1900 8200 1900
Wire Wire Line
	5150 4200 5050 4200
Wire Wire Line
	5050 4200 5050 4500
$Comp
L Battery BT1
U 1 1 59EEF90B
P 4300 5200
F 0 "BT1" H 4400 5300 50  0000 L CNN
F 1 "Battery" H 4400 5200 50  0000 L CNN
F 2 "" V 4300 5260 50  0001 C CNN
F 3 "" V 4300 5260 50  0001 C CNN
	1    4300 5200
	1    0    0    -1  
$EndComp
Wire Wire Line
	5150 4300 4300 4300
Wire Wire Line
	4300 4300 4300 5000
$Comp
L GND #PWR?
U 1 1 59EEF9D1
P 4300 5650
F 0 "#PWR?" H 4300 5400 50  0001 C CNN
F 1 "GND" H 4300 5500 50  0000 C CNN
F 2 "" H 4300 5650 50  0001 C CNN
F 3 "" H 4300 5650 50  0001 C CNN
	1    4300 5650
	1    0    0    -1  
$EndComp
Wire Wire Line
	4300 5400 4300 5650
Text Label 3800 5200 0    60   ~ 0
CR2032
Text Label 3900 5300 0    60   ~ 0
(2x)
Wire Wire Line
	6400 5250 6400 5450
$EndSCHEMATC
