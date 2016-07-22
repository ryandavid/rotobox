#!/usr/bin/python

from clint.textui import progress
import datetime
import json
from lxml import html
import os
import re
import requests
import shutil
import subprocess
import sys
import zipfile

class FAA_Charts():
    URL_VFR_RASTER_CHARTS = "https://www.faa.gov/air_traffic/flight_info/aeronav/digital_products/vfr/"
    CHART_TYPES = {
        "sectional": {
            "filename_suffix": "SEC"
        },
        "terminalArea": {
            "filename_suffix": "TAC"
        },
        "world": {
            "filename_suffix": "WOR"
        },
        "helicopter": {
            "filename_suffix": "HEL"
        },
    }
    CHART_CONFIG_FILENAME = "chart_config.json"
    REFRESH_DAYS = 7

    def __init__(self, chart_directory):
        self.chart_directory = chart_directory
        self.chart_config_file = os.path.join(self.chart_directory, self.CHART_CONFIG_FILENAME)
        self.config = {"offline_charts" : []}

        # Read the user's chart configuration file. Defaults to something empty.
        if(os.path.exists(self.chart_config_file)):
            with open(self.chart_config_file, "r") as fHandle:
                self.config = json.load(fHandle)
        else:
            print "WARN: No chart configuration found!"
        

    def flush_configuration_file(self):
        with open(self.chart_config_file, "w") as fHandle:
            json.dump(config, fHandle, indent=4, sort_keys=True)

    def update_vfr_chart_list(self):
        self.config["available_charts"] = {}

        page = requests.get(self.URL_VFR_RASTER_CHARTS)
        tree = html.fromstring(page.content)

        for chart in self.CHART_TYPES:
            print " => Updating {0} charts.".format(chart)
            self.config["available_charts"][chart] = self.scrape_html_table(tree, chart)

        self.config["updated"] = datetime.datetime.now().isoformat()
        self.flush_configuration_file()

        return self.config["available_charts"]

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

    def update(self, force_download=False):
        localCharts = {}
        # Logic to determine if we need to re-download the list of current charts
        # TODO: Be smarter when we know a subscription cycle is going to expire
        if(force_download is True):
            self.update_vfr_chart_list()
        elif("updated" in self.config):
            lastUpdated = datetime.datetime.strptime(self.config["updated"], "%Y-%m-%dT%H:%M:%S.%f")
            if((datetime.datetime.now() - lastUpdated) > datetime.timedelta(self.REFRESH_DAYS)):
                self.update_vfr_chart_list()
        elif("available_charts" not in self.config):
            self.update_vfr_chart_list()

        # Iterate through the desired charts.  Make sure they are real as compared to our index.
        for chartType in self.config["offline_charts"]:
            if(chartType in self.CHART_TYPES):
                for chart in self.config["offline_charts"][chartType]:
                    if(chart not in self.config["available_charts"][chartType]):
                        print "ERROR: Unknown {0} chart '{1}'... skipping!".format(chartType, chart)
                    else:
                        availableChartInfo = self.config["available_charts"][chartType][chart]
                        currentNumber = availableChartInfo["current_edition"]["number"]

                        expectedFilename = "{0}_{1}_{2}.tif".format(
                            self.chart_filename_escape(chart),
                            self.CHART_TYPES[chartType]["filename_suffix"],
                            currentNumber)
                        expectedFilepath = os.path.join(self.chart_directory, expectedFilename)

                        if((os.path.exists(expectedFilepath) is False) or (force_download is True)):
                            print " => Downloading/Extracting {0} chart!".format(chart)
                            url = availableChartInfo["current_edition"]["url"]
                            self.download_chart(url, expectedFilepath)
                        else:
                            print " => Current {0} chart is already downloaded!".format(chart)

                        # Finally, append the charts we have locally to a dict.
                        if(chartType not in localCharts):
                            localCharts[chartType] = []
                        localCharts[chartType].append(expectedFilepath)

        return localCharts


    # TODO: Cover more naughty characters here
    def chart_filename_escape(self, chartName):
        return chartName.replace(" ", "_")

    # Download with progress bar
    # http://stackoverflow.com/a/20943461
    def download_chart(self, url, target_path):
        success = False
        temp_path = os.path.join(self.chart_directory, "temp.zip")

        r = requests.get(url, stream=True)
        with open(temp_path, 'wb') as f:
            total_length = int(r.headers.get('content-length'))
            for chunk in progress.bar(r.iter_content(chunk_size=1024),
                                      expected_size=(total_length/1024) + 1): 
                if chunk:
                    f.write(chunk)
                    f.flush()
        
        with zipfile.ZipFile(temp_path) as zf:
            for name in zf.namelist():
                if(".tif" in name):
                    filename = zf.extract(name)
                    shutil.move(filename, target_path)
                    os.remove(temp_path)
                    success = True
                    break
        return success

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
charts = FAA_Charts(CHART_DIRECTORY)
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
        vrtFilename = os.path.join(CHART_DIRECTORY, chartBasename + "_cropped.vrt")

        if(os.path.exists(croppedFilename) is False):
            # Tile the maps!
            print " => Cropping legend from chart {0}".format(chartBasename)
            command = ["gdalwarp",
                       "-dstnodata", "0",
                       "-q",
                       "-cutline",
                       shapeFilepath,
                       "-crop_to_cutline",
                       "-multi",
                       chart,
                       croppedFilename]
            subprocess.call(command)
        else:
            print " => Legend is already cropped on chart {0}".format(chartBasename)

        print " => Generating VRT"
        command = ["gdal_translate",
                   "-q",
                   "-of", "vrt",
                   "-expand", "rgba",
                   "-co", "COMPRESS=LZW",
                   "-co", "TILED=YES",
                   croppedFilename,
                   vrtFilename]
        subprocess.call(command)

        if(chartType not in croppedCharts):
            croppedCharts[chartType] = []
        croppedCharts[chartType].append(vrtFilename)

for chartType in croppedCharts:
    print " => Creating merged GeoTIFF"
    outputFile = os.path.join(CHART_DIRECTORY, chartType + "_merged.tif")
    command = ["gdal_merge.py", "-o", outputFile, "-n", "0"] + croppedCharts[chartType]
    print command
    #subprocess.call(command)
sys.exit(0)


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

print "\nAll done!\n"
