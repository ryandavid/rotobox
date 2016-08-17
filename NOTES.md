# rotobox
## Compiling on Ubuntu
Should be identical to the steps outlined in BRINGUP.md, ignoring the ones that are specific to rotobox HW configuration.

## Compiling on OS X
````
# Install dependencies via MacPorts
sudo port install cmake libusb pkgconfig sqlite3 spatialite

# Need to add /opt/local/lib to dylib
nano ~/.profile
export DYLD_FALLBACK_LIBRARY_PATH=$DYLD_FALLBACK_LIBRARY_PATH:/opt/local/lib
````

Build it!
````
make install
````

A reverse tunnel is no longer required if you are running rotobox on a host other than where GPSD is running. Provide the IP address to the rotobox HW using the 'a' flag
````
./rotobox -a <IP_OF_ROTOBOX>
````

Before this, you would have to set up a reverse tunnel from the rotobox.
````
# On rotobox:
ssh -NR 2947:localhost:2947 <username>@<mac_ip>
# Fill in your username and IP for your Mac.  Now you should be able to open cgps on you Mac and see
# live data from the GPS on rotobox
````

## SQLite Database
The SQLite database uses spatialite extensions for representing airspaces, airport locations, etc.  There are a set of tools under the scripts folder for creating/maintaining this DB.

````
./scripts/download_airports.py
````

You can also use [QGIS](http://www.qgis.org/en/site/) to view the contents of the resultant DB.  In the browser panel, select 'SpatiaLite', 'New Connection...' and then browse to 'rotobox.sqlite'

# ![qgis](screenshots/qgis.png)


## Charts
The leaflet map expects map tiles under 'wwwroot/charts' directory.  The following script downloads the necessary charts, crops them (only sectionals currently!) and then creates tiles.  
````
# Update the chart config to specify which charts you want
./charts/charts_config.json

# Run the update script
./scripts/download_airports.py
````
