# Description
Basic delay.

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1 | Delay Time | Time for delay |
| Ctrl 2 | Feedback | Feedback for delay |
| Ctrl 3 | Mix | Dry / Wet Mix |
| SW 1 | Passthru | Illuminated when set to passthru |
| Audio In | Audio input | |
| Audio Out | Mix Out | |

# Description
Compressor that used the daisysp compressor module
TODO:  figure out sidechain
TODO:  get the right ranges on the controls

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Ctrl 1| Attack | attack |
| Ctrl 2| Release | release |
| Ctrl 3| ratio | compression ratio |
| Ctrl 4| threshold | threshold |
| Ctrl 5| gain makeup | make up for gain lost |
| SW 1 | Auto Gain |  |

# Description
Double chorus - my interpretation of the Double chorus in the Mr Black blog

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Knob 1 | delay | |
| Knob 2 | speed | |
| Knob 3 | depth | |
| Knob 4 | mix | |
| Knob 5 | feedback | |
| Knob 6 | makeup | adds a little gain (workaround for some gain loss) |

# Description
Flanger - port from the petal code example

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Knob 1 | delay | |
| Knob 2 | feedback | |
| Kob 3 | mix | |
| Knob 4 | lfo | lfo speed |
| Knob 5 | lfo depth | |

# Description
Three delay bundled in one. Set their delay times independently. Feedback is controlled globally.
The three delays mixed with the dry signal is available at all outputs.
Set the dry/wet amount of this final mix with the encoder.

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Ctrls 1 - 3 - 5| Delay Time | Time for delays 1 through 3 |
| Ctrls 2 - 4 - 5 | Feedback | Feedback for delays 1 through 3 |
| Stomp 1 | Passthru | Illuminated when set to passthru |
| Audio In | Audio input | |
| Audio Out | Mix Out | |

# Description
verb - reverb - port from petal

# Control

| Control | Description | Comment |
| --- | --- | --- |
| Knob 1 | Reverb Time | Small room to near-infinite |
| Knob 2 | Reverb Damping | Internal Cutoff filter from about 500Hz to 20kHz |
| Knob 3 | Send Amount | Controls amount of dry signal sent to reverb |
