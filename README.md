# rotobox

## Compiling on OS X
````
# Install dependencies via MacPorts
sudo port install cmake libusb pkgconfig

# Before attempting to compile, need to export the pkgconfig path
# NO LONGER REQUIRED!
#export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/
````

Build it!
````
make install
````