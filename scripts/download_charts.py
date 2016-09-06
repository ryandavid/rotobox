#!/usr/bin/python

import datetime
import json
import os
import re
import Rotobox
import shutil
import subprocess
import sys

# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

CHART_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "charts")
SHAPEFILES_DIRECTORY = os.path.join(CHART_DIRECTORY, "shapefiles")
CHART_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "charts")

# Ensure the chart directory exists.
if(os.path.exists(CHART_DIRECTORY) is False):
    os.makedirs(CHART_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(CHART_PROCESSED_DIRECTORY) is False):
    os.makedirs(CHART_PROCESSED_DIRECTORY)

print "Chart Directory:\t{0}".format(CHART_DIRECTORY)
print "Chart Output:\t\t{0}".format(CHART_PROCESSED_DIRECTORY)

# Make sure our charts are up-to-date.
charts = Rotobox.FAA_Charts(CHART_DIRECTORY)
chartsDownloaded = charts.update()

croppedCharts = {}

# Iterate over every type (ie, sectional) of chart specified in the configuration
for chartType in chartsDownloaded:

    # Iterate over every requested chart name (ie, 'San Francisco') within this type
    for chart in chartsDownloaded[chartType]:
        chartBasename = os.path.splitext(os.path.basename(chart))[0]
        chartBasenameStripped = re.sub("(_[0-9]+)", "", chartBasename)
        shapeFilepath = os.path.join(SHAPEFILES_DIRECTORY, chartBasenameStripped + ".shp")
        croppedFilename = os.path.join(CHART_DIRECTORY, chartBasename + "_cropped.tif")

        if(os.path.exists(croppedFilename) is False):
            # Make sure we have the shapefile for cropping off the legend
            if(os.path.exists(shapeFilepath) is False):
                print ' => Skipping {0} since there is no matching shapefile'.format(chartBasename)
                continue

            # Tile the maps!
            print " => Cropping legend and reprojecting chart {0}".format(chartBasename)
            command = ["gdalwarp",
                       "-dstnodata", "0",
                       "-q",
                       "-cutline",
                       shapeFilepath,
                       "-crop_to_cutline",
                       "-multi",
                       "-t_srs", "EPSG:3857",
                       "-wo", "OPTIMIZE_SIZE=YES",
                       "-co", "compress=lzw",
                       chart,
                       croppedFilename]
            subprocess.call(command)

            print " => Converting to RGB"
            command = ["pct2rgb.py", croppedFilename, croppedFilename]
            subprocess.call(command)

        else:
            print " => Legend is already cropped on chart {0}".format(chartBasename)

        if(chartType not in croppedCharts):
            croppedCharts[chartType] = []
        croppedCharts[chartType].append(croppedFilename)

for chartType in croppedCharts:
    print " => Building VRT"
    outputFile = os.path.join(CHART_DIRECTORY, chartType + "_merged.vrt")
    command = ["gdalbuildvrt",
               "-srcnodata", "255 255 255",
               "-vrtnodata", "255 255 255",
               "-hidenodata",
               outputFile] + croppedCharts[chartType]
    subprocess.call(command)

    print " => Tiling {0} charts (This will take a while...)".format(chartType)
    command = ["gdal2tiles.py",
               "-w", "none",
               outputFile,
               CHART_PROCESSED_DIRECTORY]
    subprocess.call(command)

    # Clean up
    os.remove(outputFile)

print "\nAll done!\n"
