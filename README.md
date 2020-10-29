# terra

This is all work in progress stuff.  I'm still learning the platform.
Hopefully, it helps get somebody started.

# setup instructions

Note: this assumes a linux build environment.
See https://github.com/electro-smith/DaisyExamples for instructions.


In your work directory, type in the following commands:

git clone --recursive https://github.com/electro-smith/DaisyExamples
cd DaisyExamples
./rebuild_all.sh
cd ..

git clone --recursive https://github.com/tcpoint/terra.git
cd terra
cd verb
make
# set your daisy to program mode and connect it to usb.
make program-dfu
# rock it.
