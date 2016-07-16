#!/usr/bin/python

from clint.textui import progress
import datetime
import json
import os
import re
import requests
import shutil
import subprocess
import sys
import xml.etree.ElementTree as etree
import zipfile

class FAA_Airports():
    URL_CYCLES_LIST = "https://enasr.faa.gov/eNASR/nasr/ValueList/Cycle"
    URL_AIXM = "https://nfdc.faa.gov/webContent/56DaySub/{0}/aixm5.1.zip"
    CHART_TYPES = ["sectional", "terminalArea", "world", "helicopter"]

    def __init__(self):
        self.cycles = {}

    # Download with progress bar
    # http://stackoverflow.com/a/20943461
    def download_aixm(self, cycle, target_path):
        if(cycle in self.cycles):
            url = self.URL_AIXM.format(self.cycles[cycle])
        else:
            url = self.URL_AIXM.format(self.cycles["current"])

        r = requests.get(url, stream=True)
        with open(target_path, 'wb') as f:
            total_length = int(r.headers.get('content-length'))
            for chunk in progress.bar(r.iter_content(chunk_size=1024),
                                      expected_size=(total_length/1024) + 1): 
                if chunk:
                    f.write(chunk)
                    f.flush()


    def update_cycles(self):
        page = requests.get(self.URL_CYCLES_LIST)
        obj = page.json()

        for cycle in obj["Cycle"]:
            m = re.search("([0-9]{4}-[0-9]{2}-[0-9]{2})", cycle["choice"])
            if(m is not None):
                self.cycles[cycle["name"].lower()] = m.group(0)

        return self.cycles

    def update_all(self, target_dir):
        self.update_cycles()

        basename = "aixm_" + self.cycles["current"]
        downloadFullpath = os.path.join(target_dir, basename + ".zip")
        print "Downloading AIXM Data".format(self.cycles["current"])
        if(os.path.exists(downloadFullpath) is False):
            print " => Fetching Current Subscription ({0})".format(self.cycles["current"])
            self.download_aixm("current", downloadFullpath)
        else:
            print " => Already Current Subscription ({0})".format(self.cycles["current"])

        desiredFiles = [
            "AIXM_5.1/XML-Subscriber-Files/APT_AIXM.zip",
            "AIXM_5.1/XML-Subscriber-Files/AWOS_AIXM.zip",
            "AIXM_5.1/XML-Subscriber-Files/AWY_AIXM.zip",
            "AIXM_5.1/XML-Subscriber-Files/NAV_AIXM.zip"
        ]

        main_zf = zipfile.ZipFile(downloadFullpath)

        extractedFilepaths = []
        for file in desiredFiles:
            print " => Extracting file {0}".format(file)
            outpath = main_zf.extract(file, target_dir)
            sub_zf = zipfile.ZipFile(outpath)
            # HACKY HACK HACK
            childname = os.path.basename(outpath).replace("zip", "xml")
            filename = sub_zf.extract(childname, target_dir)

            os.remove(outpath)
            extractedFilepaths.append(filename)

        # Clean up after ourselves
        shutil.rmtree(os.path.join(target_dir, "AIXM_5.1"))

        return extractedFilepaths

    def get_current_cycle(self):
        return self.cycles["current"]


def make_tag(ns, uri):
    global tags
    return "{" + tags[ns] + "}" + uri

def force_bool(value):
    forcedValue = False

    if(isinstance(value, int) == True):
        if(value > 0):
            forcedValue = True
    elif(isinstance(value, str) == True):
        if(value.lower() in ["t", "true", "1", "y", "yes"]):
            forcedValue = True
            
    return forcedValue

def helper_designator(element):
    return {"designator": element.text}

def helper_featureLifetime(element):
    return {"featureLifetime": ""}

def helper_name(element):
    return {"name": element.text}

def helper_locationIndicatorICAO(element):
    return {"locationIndicatorICAO": element.text}

def helper_type(element):
    return {"type": element.text}

def helper_extension(element):
    results = {}
    for item in element.iter():
        if(item.tag == make_tag("aixm", "extension")):
            continue
        if((item.tag in AIRPORT_TAG_PARSERS) and (AIRPORT_TAG_PARSERS[item.tag] is not None)):
            results.update(AIRPORT_TAG_PARSERS[item.tag](item))
            item.clear()
    return results

def helper_aeronauticalSectionalChart(element):
    return {"aeronauticalSectionalChart": element.text}

def helper_lightingSchedule(element):
    return {"lightingSchedule": element.text}

def helper_beaconLightingSchedule(element):
    return {"beaconLightingSchedule": element.text}

def helper_markerLensColor(element):
    return {"markerLensColor": element.text}

def helper_trafficControlTowerOnAirport(element):
    value = element.text.lower()
    return {"trafficControlTowerOnAirport": force_bool(element.text)}

def helper_segmentedCircleMarkerOnAirport(element):
    return {"segmentedCircleMarkerOnAirport": force_bool(element.text)}

def helper_airportAttendanceSchedule(element):
    return {"airportAttendanceSchedule": element.text}

def helper_privateUse(element):
    return {"privateUse": force_bool(element.text)}

def helper_controlType(element):
    return {"controlType": element.text} 

def helper_fieldElevation(element):
    units = element.get("uom")
    if((units is not None) and (units.lower() != "ft")):
        print "ERROR: Invalid units for field elevation ({0})".format(units)
    return {"fieldElevation": float(element.text)} 

def helper_magneticVariation(element):
    return {"magneticVariation": element.text}

def helper_windDirectionIndicator(element):
    return {"windDirectionIndicator": force_bool(element.text)} 

def helper_servedCity(element):
    # TODO: Stop being so lazy and actually drill down to the child node
    allText = "".join(element.itertext()).strip()
    return {"servedCity": allText} 

def helper_ARP(element):
    # TODO: Stop being so lazy and actually drill down to the child node
    allText = "".join(element.itertext()).strip()
    coord = allText.split(" ")
    return {"ARP": [float(coord[0]), float(coord[1])]}

def helper_annotation(element):
    content = None
    for item in element.iter(make_tag("aixm", "LinguisticNote")):
        itemType = item.get(make_tag("gml", "id"))

        # TODO: Be smarter about the annotation types:
        # "REMARK", "WINDDIRECTION_NOTE", "SEAPLANE_BASE_NOTE", "GLIDERPORT_NOTE", "ULTRALIGHT_NOTE"
        if("REMARK_NAME" in itemType):
            itemType

        else:
            # TODO: Stop being so lazy and actually drill down to the child node
            allText = "".join(item.itertext()).strip()
            content = {"remark": allText}

    return content

def parse_airport(element):
    airport = {
        "id": element.get(make_tag("gml", "id")),
        "remark": "",
    }

    # Don't use iter since we just want the direct children
    for item in element[0][0]:
        if((item.tag in AIRPORT_TAG_PARSERS) and (AIRPORT_TAG_PARSERS[item.tag] is not None)):
            result = AIRPORT_TAG_PARSERS[item.tag](item)
            if(result is not None):
                # Special case to append remarks
                if(result.keys()[0] == "remark"):
                    airport["remark"] += result["remark"] + "  "
                else:
                    airport.update(result)

            item.clear()
    airport["remark"] = airport["remark"].strip()
    return airport


# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

AIRPORT_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "airports")
AIRPORT_CONFIG = os.path.join(AIRPORT_DIRECTORY, "airport_config.json")
AIRPORT_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "airports")

print "Airport Directory:\t{0}".format(AIRPORT_DIRECTORY)
print "Airport Configuration:\t{0}".format(AIRPORT_CONFIG)
print "Airport Output:\t\t{0}".format(AIRPORT_PROCESSED_DIRECTORY)

# Ensure the chart directory exists.
if(os.path.exists(AIRPORT_DIRECTORY) is False):
    os.makedirs(AIRPORT_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(AIRPORT_PROCESSED_DIRECTORY) is False):
    os.makedirs(AIRPORT_PROCESSED_DIRECTORY)

# MAGIC
airports = FAA_Airports()

# Read the user's chart configuration file. Default to something empty.
if(os.path.exists(AIRPORT_CONFIG)):
    with open(AIRPORT_CONFIG, "r") as fHandle:
        config = json.load(fHandle)
else:
    print "ERROR: No airport configuration found!"
    config = {}

# We want to write out available charts to this file
config["updated"] = datetime.datetime.now().isoformat()
with open(AIRPORT_CONFIG, "w") as fHandle:
    json.dump(config, fHandle, indent=4, sort_keys=True)


files = airports.update_all(AIRPORT_DIRECTORY)
aptXML = os.path.join(AIRPORT_DIRECTORY, "APT_AIXM.xml")


tags = {
    "faa": "http://www.faa.gov/aixm5.1",
    "apt": "http://www.faa.gov/aixm5.1/apt",
    "gml": "http://www.opengis.net/gml/3.2",
    "xsi": "http://www.w3.org/2001/XMLSchema-instance",
    "aixm": "http://www.aixm.aero/schema/5.1",
    "gmd": "http://www.isotc211.org/2005/gmd",
    "gco": "http://www.isotc211.org/2005/gco",
    "xlink": "http://www.w3.org/1999/xlink"
}



AIRPORT_TAG_PARSERS = {
    make_tag("gml", "featureLifetime"): helper_featureLifetime,
    make_tag("aixm", "designator"): helper_designator,
    make_tag("aixm", "name"): helper_name,
    make_tag("aixm", "locationIndicatorICAO"): helper_locationIndicatorICAO,
    make_tag("aixm", "type"): helper_type,
    make_tag("aixm", "privateUse"): helper_privateUse,
    make_tag("aixm", "controlType"): helper_controlType,
    make_tag("aixm", "fieldElevation"): helper_fieldElevation,
    make_tag("aixm", "magneticVariation"): helper_magneticVariation,
    make_tag("aixm", "windDirectionIndicator"): helper_windDirectionIndicator,
    make_tag("aixm", "servedCity"): helper_servedCity,
    make_tag("aixm", "ARP"): helper_ARP,
    make_tag("aixm", "annotation"): helper_annotation,
    make_tag("aixm", "extension"): helper_extension,
    make_tag("apt", "aeronauticalSectionalChart"): helper_aeronauticalSectionalChart,
    make_tag("apt", "lightingSchedule"): helper_lightingSchedule,
    make_tag("apt", "beaconLightingSchedule"): helper_beaconLightingSchedule,
    make_tag("apt", "markerLensColor"): helper_markerLensColor,
    make_tag("apt", "trafficControlTowerOnAirport"): helper_trafficControlTowerOnAirport,
    make_tag("apt", "segmentedCircleMarkerOnAirport"): helper_segmentedCircleMarkerOnAirport,
    make_tag("apt", "airportAttendanceSchedule"): helper_airportAttendanceSchedule
}


inhibitClearing = False
TAG_AIRPORT_HELIPORT = make_tag("aixm", "AirportHeliport")
for event, elem in etree.iterparse(aptXML, events=("start", "end")):
    if(elem.tag == TAG_AIRPORT_HELIPORT):
        if event == "start":
            inhibitClearing = True
        else:
            inhibitClearing = False
            airport = parse_airport(elem)
            print " => Parsed airport {0}".format(airport["name"])

        
    if(inhibitClearing == False):
        elem.clear()

