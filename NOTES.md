# rotobox

## Compiling on OS X
````
# Install dependencies via MacPorts
sudo port install cmake libusb pkgconfig sqlite3 spatialite

# Need to add /opt/local/lib to dylib
nano ~/.profile
export DYLD_FALLBACK_LIBRARY_PATH=$DYLD_FALLBACK_LIBRARY_PATH:/opt/local/lib

# If you want to use rotobox's GPS but develop on OS X, you can set up a reverse tunnel for the port
# that GPSD uses
# On rotobox:
ssh -NR 2947:localhost:2947 <username>@<mac_ip>
# Fill in your username and IP for your Mac.  Now you should be able to open cgps on you Mac and see
# live data from the GPS on rotobox
````

Build it!
````
make install
````