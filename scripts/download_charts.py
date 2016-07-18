#!/usr/bin/python

from clint.textui import progress
import datetime
import json
from lxml import html
import os
import requests
import shutil
import subprocess
import sys
import zipfile

class FAA_Charts():
    URL_VFR_RASTER_CHARTS = "https://www.faa.gov/air_traffic/flight_info/aeronav/digital_products/vfr/"
    CHART_TYPES = ["sectional", "terminalArea", "world", "helicopter"]

    def __init__(self):
        self.chart_list = {}

    def update_all_vfr_charts(self):
        self.chart_list = {}
        page = requests.get(self.URL_VFR_RASTER_CHARTS)
        tree = html.fromstring(page.content)

        for chart in self.CHART_TYPES:
            print "Updating {0} charts.".format(chart)
            self.chart_list[chart] = self.scrape_html_table(tree, chart)

        return self.chart_list

    def scrape_html_table(self, page, chart_type):
        charts = {}
        # There are two tables within each section:
        # [0] - GEO-TIFF files
        # [1] - PDF's
        sectionalTables = page.xpath('//div[@id="' + chart_type + '"]//table[@class="striped"]//tbody')

        for elem in sectionalTables[0].iter("tr"):
            cells = elem.getchildren()
            cell_chart_name = cells[0]
            cell_current_edition = cells[1]
            cell_next_edition = cells[2]

            chart_name = cell_chart_name.text_content().encode("ascii", errors="ignore")

            current_edition = cell_current_edition.text_content().encode("ascii", errors="ignore")
            current_edition = current_edition.split("  ")
            current_edition_number = current_edition[0]
            current_edition_date = current_edition[1]
            current_edition_link = None
            if (("zip" in current_edition_date.lower())):
                chop_pos = current_edition_date.find(" (")
                current_edition_date = current_edition_date[0:chop_pos]

            for element, attribute, link, pos in cell_current_edition.iterlinks():
                current_edition_link = link.encode("ascii", errors="ignore")
                break

            current_edition_date = datetime.datetime.strptime(current_edition_date, "%b %d %Y").isoformat()

            next_edition = cell_next_edition.text_content().encode("ascii", errors="ignore")
            if(("discontinued" in next_edition.lower()) | ("tbd" in next_edition.lower())):
                next_edition_number = None
                next_edition_date = None
                next_edition_link = None
            else:
                next_edition = next_edition.split("  ")
                next_edition_number = next_edition[0]
                next_edition_date = next_edition[1]
                next_edition_link = None
                if ("zip" in next_edition_date.lower()):
                    chop_pos = next_edition_date.find(" (")
                    next_edition_date = next_edition_date[0:chop_pos]
                    for element, attribute, link, pos in cell_next_edition.iterlinks():
                        next_edition_link = link.encode("ascii", errors="ignore")
                        break

                next_edition_date = datetime.datetime.strptime(next_edition_date, "%b %d %Y").isoformat()
            

            charts[chart_name] = {
                "current_edition": {
                    "number": current_edition_number,
                    "date": current_edition_date,
                    "url": current_edition_link
                },
                "next_edition": {
                    "number": next_edition_number,
                    "date": next_edition_date,
                    "url": next_edition_link
                }
            }
            
        return charts


# Download with progress bar
# http://stackoverflow.com/a/20943461
def download_chart(url, target_path):
    r = requests.get(url, stream=True)
    with open(target_path, 'wb') as f:
        total_length = int(r.headers.get('content-length'))
        for chunk in progress.bar(r.iter_content(chunk_size=1024), expected_size=(total_length/1024) + 1): 
            if chunk:
                f.write(chunk)
                f.flush()

# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

CHART_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "charts")
CHART_CONFIG = os.path.join(CHART_DIRECTORY, "chart_config.json")
CHART_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "charts")

print "Chart Directory:\t{0}".format(CHART_DIRECTORY)
print "Chart Configuration:\t{0}".format(CHART_CONFIG)
print "Chart Output:\t\t{0}".format(CHART_PROCESSED_DIRECTORY)

# Ensure the chart directory exists.
if(os.path.exists(CHART_DIRECTORY) is False):
    os.makedirs(CHART_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(CHART_PROCESSED_DIRECTORY) is False):
    os.makedirs(CHART_PROCESSED_DIRECTORY)

# Snag a list of all the latest chart versions
charts = FAA_Charts()
chartList = charts.update_all_vfr_charts()

# Read the user's chart configuration file. Default to something empty.
if(os.path.exists(CHART_CONFIG)):
    with open(CHART_CONFIG, "r") as fHandle:
        config = json.load(fHandle)
else:
    print "ERROR: No chart configuration found!"
    config = {
        "offline_charts" : []
    }

# We want to write out available charts to this file
config["available_charts"] = chartList
config["updated"] = datetime.datetime.now().isoformat()
with open(CHART_CONFIG, "w") as fHandle:
    json.dump(config, fHandle, indent=4, sort_keys=True)

# Iterate over every type (ie, sectional) of chart specified in the configuration
for chartType in config["offline_charts"]:
    if(chartType not in chartList):
        print "ERROR: Unknown chart type '{0}'".format(chartType)
        continue

    # Iterate over every requested chart name (ie, 'San Francisco') within this type
    for chart in config["offline_charts"][chartType]:
        if(chart not in chartList[chartType]):
            print "ERROR: Unknown {0} chart '{1}'".format(chartType, chart)
        else:
            print "Checking VFR {0} for {1}".format(chartType, chart)
            print " => Current Edition is #{0} ({1})".format(
                chartList[chartType][chart]["current_edition"]["number"],
                chartList[chartType][chart]["current_edition"]["date"])

            # TODO(rdavid): Do something smart with the next edition as well
            if(chartList[chartType][chart]["next_edition"]["url"] is not None):
                print " => Next Edition is #{0} ({1})".format(
                    chartList[chartType][chart]["next_edition"]["number"],
                    chartList[chartType][chart]["next_edition"]["date"])

            if(chartType == "sectional"):
                basename_suffix = "_SEC_"
                shapefile_suffix = "_SEC.shp"
            elif(chartType == "terminalArea"):
                basename_suffix = "_TAC_"
                shapefile_suffix = "_TAC.shp"
            elif(chartType == "world"):
                basename_suffix = "_WAC_"
                shapefile_suffix = "_WAC.shp"
            elif(chartType == "helicopter"):
                basename_suffix = "_HEL_"
                shapefile_suffix = "_HEL.shp"

            shapefilepath = os.path.join(CHART_DIRECTORY, "shapefiles", chart.replace(" ", "_") + shapefile_suffix)
            basename = chart.replace(" ", "_") + basename_suffix
            basename += chartList[chartType][chart]["current_edition"]["number"]
            expectedFiles = [os.path.join(CHART_DIRECTORY, basename + ".htm"),
                             os.path.join(CHART_DIRECTORY, basename + ".tfw"),
                             os.path.join(CHART_DIRECTORY, basename + ".tif")]

            # Check that all the files we expect are present.
            allFilesExist = True
            for file in expectedFiles:
                if(os.path.exists(file) is False):
                    allFilesExist = False
                    break
            allFilesExist=False
            # If all the files don't exist, time to grab the chart.
            if(allFilesExist == False):
                url = chartList[chartType][chart]["current_edition"]["url"]
                downloadFullpath = os.path.join(CHART_DIRECTORY, os.path.basename(url))
                print " => Fetching {0}".format(url)
                #download_chart(url, downloadFullpath)
                #
                ## Unzip individually instead of one fell swoop so we can touch up the filenames.
                #with zipfile.ZipFile(downloadFullpath) as zf:
                #    for item in zf.infolist():
                #        filename = zf.extract(item, CHART_DIRECTORY)
                #        new_filename = filename.replace(" ", "_")
                #        shutil.move(filename, new_filename)
                #        print " => Unpacking {0}".format(new_filename)
                #
                ## Finally clean up after ourselves.
                #os.remove(downloadFullpath)

                # Tile the maps!
                mapFilename = os.path.join(CHART_DIRECTORY, basename + ".tif")
                croppedFilename = os.path.join(CHART_DIRECTORY, basename + "cropped.tif")
                print " => Cropping legend from chart"
                command = ["gdalwarp",
                           "-dstnodata", "0",
                           "-q",
                           "-cutline",
                           shapefilepath,
                           "-crop_to_cutline",
                           mapFilename,
                           croppedFilename]
                subprocess.call(command)

                vrtFilename = os.path.join(CHART_DIRECTORY, basename + ".vrt")
                print " => Converting {0}".format(basename)
                command = ["gdal_translate",
                           "-q",
                           "-of", "vrt",
                           "-expand", "rgba",
                           croppedFilename,
                           vrtFilename]
                subprocess.call(command)

                print " => Tiling {0} (This will take a while...)".format(basename)
                command = ["gdal2tiles.py",
                           "-w", "none",
                           "-q",
                           vrtFilename,
                           CHART_PROCESSED_DIRECTORY]
                subprocess.call(command)

                # Clean up
                os.remove(croppedFilename)
                os.remove(vrtFilename)

            else:
                print " => {0} is up to date locally!".format(chart)

print "\nAll done!\n"
