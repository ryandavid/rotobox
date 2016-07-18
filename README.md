# rotobox

## Compiling on OS X
````
# Install dependencies via MacPorts
sudo port install cmake libusb pkgconfig

# Before attempting to compile, need to export the pkgconfig path
# NO LONGER REQUIRED!
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/

# Need to add /opt/local/lib to dylib
nano ~/.bash_profile
export DYLD_FALLBACK_LIBRARY_PATH=$DYLD_FALLBACK_LIBRARY_PATH:/opt/local/lib
````

Build it!
````
make install
````