#!/bin/sh

echo "Start init ALC5650"

amixer -c0 cset "name='DAC L2 Mux'" 1
amixer -c0 cset "name='DAC R2 Mux'" 1
amixer -c0 cset "name='A DAC2 L Mux'" 1
amixer -c0 cset "name='A DAC2 R Mux'" 1
amixer -c0 cset "name='Mono DAC MIXL DAC L2 Switch'" 1
amixer -c0 cset "name='Mono DAC MIXR DAC R2 Switch'" 1
amixer -c0 cset "name='DAC2 Playback Switch'" 1
amixer -c0 cset "name='SPOL MIX SPKVOL L Switch'" 1
amixer -c0 cset "name='SPOR MIX SPKVOL R Switch'" 1
amixer -c0 cset "name='SPKVOL L Switch'" 1
amixer -c0 cset "name='SPKVOL R Switch'" 1
amixer -c0 cset "name='Speaker Channel Switch'" 1
amixer -c0 cset "name='Left Spk Switch'" 1
amixer -c0 cset "name='Right Spk Switch'" 1


amixer -c0 cset "name='DAC1 L Mux'" 0
amixer -c0 cset "name='DAC1 R Mux'" 0
amixer -c0 cset "name='DAC1 MIXL DAC1 Switch'" 1
amixer -c0 cset "name='DAC1 MIXR DAC1 Switch'" 1
amixer -c0 cset "name='Stereo DAC MIXL DAC L1 Switch'" 1
amixer -c0 cset "name='Stereo DAC MIXR DAC R1 Switch'" 1
amixer -c0 cset "name='HPOVOL MIXL DAC1 Switch'" 1
amixer -c0 cset "name='HPOVOL MIXR DAC1 Switch'" 1
amixer -c0 cset "name='Headphone Channel Switch'" 1
amixer -c0 cset "name='HPO MIX HPVOL Switch'" 1
amixer -c0 cset "name='HPOVOL L Switch'" 1
amixer -c0 cset "name='HPOVOL R Switch'" 1



amixer -c0 cset "name='DAC1 Playback Volume'" 87
amixer -c0 cset "name='DAC2 Playback Volume'" 87


amixer -c0 cset "name='SPK MIXL DAC L1 Switch'" 1
amixer -c0 cset "name='SPK MIXR DAC R1 Switch'" 1
amixer -c0 cset "name='SPK MIXL DAC L2 Switch'" 0
amixer -c0 cset "name='SPK MIXR DAC R2 Switch'" 0
amixer -c0 cset "name='Speaker Playback Volume'" 31

amixer -c0 cset "name='HPOVOL MIXL DAC1 Switch'" 1
amixer -c0 cset "name='HPOVOL MIXR DAC1 Switch'" 1
amixer -c0 cset "name='HPOVOL MIXL DAC2 Switch'" 0
amixer -c0 cset "name='HPOVOL MIXR DAC2 Switch'" 0
amixer -c0 cset "name='Headphone Playback Volume'" 31


amixer -c0 cset "name='RECMIXL BST1 Switch'" 1
amixer -c0 cset "name='RECMIXR BST1 Switch'" 1
amixer -c0 cset "name='Stereo1 ADC1 Mux'" 1
amixer -c0 cset "name='Sto1 ADC MIXL ADC1 Switch'" 0
amixer -c0 cset "name='Sto1 ADC MIXR ADC1 Switch'" 0
amixer -c0 cset "name='Sto1 ADC MIXL ADC1 Switch'" 1
amixer -c0 cset "name='Sto1 ADC MIXR ADC1 Switch'" 1
amixer -c0 cset "name='RT5650 IF1 ADC1 Swap Mux'" 0
amixer -c0 cset "name='ADC Capture Switch'" 1
#amixer -c0 cset "name='IF2 ADC Mux'" IF_ADC1
amixer -c0 cset "name='IN1 Boost'" 2
amixer -c0 cset "name='IN Capture Volume'" 31

#amixer -c 0 cset name='PGA1.0 1 Master Playback Switch' on
#amixer -c 0 cset name='PGA2.0 2 Master Capture Switch' on
#amixer -c 0 cset name='PGA3.0 3 Master Playback Switch' on
#amixer -c 0 cset name='PGA4.0 4 Master Capture Switch' on

exit 0
