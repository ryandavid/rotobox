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

ROTOBOX_ROOT = os.path.dirname(SCRIPT_DIR)
AIRPORT_DB = os.path.join(ROTOBOX_ROOT, "rotobox.sqlite")
CHART_DIRECTORY = os.path.join(ROTOBOX_ROOT, "charts")
SHAPEFILES_DIRECTORY = os.path.join(CHART_DIRECTORY, "shapefiles")
CHART_PROCESSED_DIRECTORY = os.path.join(ROTOBOX_ROOT, "wwwroot", "charts")

# Ensure the chart directory exists.
if(os.path.exists(CHART_DIRECTORY) is False):
    os.makedirs(CHART_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(CHART_PROCESSED_DIRECTORY) is False):
    os.makedirs(CHART_PROCESSED_DIRECTORY)

print "Chart Directory:\t{0}".format(CHART_DIRECTORY)
print "Chart Output:\t\t{0}".format(CHART_PROCESSED_DIRECTORY)

db = Rotobox.Database(AIRPORT_DB)

charts = Rotobox.FAA_Charts(CHART_DIRECTORY)
chartList = charts.get_latest_chart_info()

# Update the DB with the latest chart info.
for chartType in chartList:
    for chart in chartList[chartType]:
        row = {
            "chart_type": chartType,
            "chart_name": chart,
            "current_date": chartList[chartType][chart]["current_edition"]["date"],
            "current_number": chartList[chartType][chart]["current_edition"]["number"],
            "current_url": chartList[chartType][chart]["current_edition"]["url"],
            "next_date": chartList[chartType][chart]["next_edition"]["date"],
            "next_number": chartList[chartType][chart]["next_edition"]["number"],
            "next_url": chartList[chartType][chart]["next_edition"]["url"],
        }

        db.upsert_into_db_table_charts(row)
db.commit()

chartsToDownload = db.get_chart_list_to_be_downloaded()
localCharts = charts.fetch_charts(chartsToDownload)
croppedCharts = {}

# Iterate over every type (ie, sectional) of chart specified in the configuration
for type in localCharts:
    for chart in localCharts[type]:

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

        if(type not in croppedCharts):
            croppedCharts[type] = []
        croppedCharts[type].append(croppedFilename)


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
