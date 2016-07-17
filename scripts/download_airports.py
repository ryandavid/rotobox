#!/usr/bin/python

from clint.textui import progress
import datetime
import json
import os
import re
import requests
import shutil
import sqlite3
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
    return {"activated": "".join(element.itertext()).strip()}

def helper_name(element):
    return {"name": element.text}

def helper_locationIndicatorICAO(element):
    return {"icao_name": element.text}

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
    return {"sectional_chart": element.text}

def helper_lightingSchedule(element):
    return {"lighting_schedule": element.text}

def helper_beaconLightingSchedule(element):
    return {"beacon_lighting_schedule": element.text}

def helper_markerLensColor(element):
    return {"marker_lens_color": element.text}

def helper_trafficControlTowerOnAirport(element):
    value = element.text.lower()
    return {"traffic_control_tower_on_airport": force_bool(element.text)}

def helper_segmentedCircleMarkerOnAirport(element):
    return {"segmented_circle_marker_on_airport": force_bool(element.text)}

def helper_airportAttendanceSchedule(element):
    return {"attendance_schedule": element.text}

def helper_privateUse(element):
    return {"private_use": force_bool(element.text)}

def helper_controlType(element):
    return {"control_type": element.text} 

def helper_fieldElevation(element):
    units = element.get("uom")
    if((units is not None) and (units.lower() != "ft")):
        print "ERROR: Invalid units for field elevation ({0})".format(units)
    return {"field_elevation": float(element.text)} 

def helper_magneticVariation(element):
    return {"magnetic_variation": element.text}

def helper_windDirectionIndicator(element):
    return {"wind_direction_indicator": force_bool(element.text)} 

def helper_servedCity(element):
    # TODO: Stop being so lazy and actually drill down to the child node
    allText = "".join(element.itertext()).strip()
    return {"served_city": allText} 

def helper_ARP(element):
    # TODO: Stop being so lazy and actually drill down to the child node
    allText = "".join(element.itertext()).strip()
    coord = allText.split(" ")
    return {"longitude": float(coord[0]), "latitude": float(coord[1])}

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
            content = {"remarks": allText}

    return content

def parse_airport(element):
    airport = {
        "faa_id": element.get(make_tag("gml", "id")),
        "remarks": "",
    }

    # Don't use iter since we just want the direct children
    for item in element[0][0]:
        if((item.tag in AIRPORT_TAG_PARSERS) and (AIRPORT_TAG_PARSERS[item.tag] is not None)):
            result = AIRPORT_TAG_PARSERS[item.tag](item)
            if(result is not None):
                # Special case to append remarks
                if(result.keys()[0] == "remarks"):
                    airport["remarks"] += result["remarks"] + "  "
                else:
                    airport.update(result)

            item.clear()
    airport["remarks"] = airport["remarks"].strip()
    return airport

def parse_runway(element):
    runway = {
        "faa_id": element.get(make_tag("gml", "id")),
    }

    for item in element[0][0]:
        if((item.tag in RUNWAY_TAG_PARSERS) and (RUNWAY_TAG_PARSERS[item.tag] is not None)):
            result = RUNWAY_TAG_PARSERS[item.tag](item)
            runway.update(result)

        item.clear()
    return runway


# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

AIRPORT_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "airports")
AIRPORT_CONFIG = os.path.join(AIRPORT_DIRECTORY, "airport_config.json")
AIRPORT_DB = os.path.join(AIRPORT_DIRECTORY, "airport_db.sqlite")
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


#files = airports.update_all(AIRPORT_DIRECTORY)
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
    make_tag("aixm", "featureLifetime"): helper_featureLifetime,
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

def helper_associatedAirportHeliport(element):
    associatedString = element.get(make_tag("xlink", "href"))
    startPos = associatedString.find("id='") + len("id='")
    endPos = associatedString.find("'", startPos)
    return {"airport_faa_id": associatedString[startPos:endPos]}

def helper_runwayDesignatior(element):
    return {"designator": element.text}

def helper_runwayLength(element):
    units = element.get("uom")
    if((units is not None) and (units.lower() != "ft")):
        print "ERROR: Invalid units for runway length ({0})".format(units)
    return {"length": float(element.text)}

def helper_runwayWidth(element):
    units = element.get("uom")
    if((units is not None) and (units.lower() != "ft")):
        print "ERROR: Invalid units for runway length ({0})".format(units)
    return {"width": float(element.text)}

def helper_runwaySurface(element):
    surfaceCharacteristics = element[0]
    compositionTag = surfaceCharacteristics.find(make_tag("aixm", "composition"))
    if(compositionTag is not None):
        composition = compositionTag.text
    else:
        composition = None

    preparationTag = surfaceCharacteristics.find(make_tag("aixm", "preparation"))
    if(preparationTag is not None):
        preparation = preparationTag.text
    else:
        preparation = None

    conditionTag = surfaceCharacteristics.find(make_tag("aixm", "surfaceCondition"))
    if(conditionTag is not None):
        condition = conditionTag.text
    else:
        condition = None

    return {"composition": composition, "preparation": preparation, "condition": condition}


RUNWAY_TAG_PARSERS = {
    make_tag("aixm", "associatedAirportHeliport"): helper_associatedAirportHeliport,
    make_tag("aixm", "designator"): helper_runwayDesignatior,
    make_tag("aixm", "lengthStrip"): helper_runwayLength,
    make_tag("aixm", "widthStrip"): helper_runwayWidth,
    make_tag("aixm", "surfaceProperties"): helper_runwaySurface,
}

def helper_tdtoDesignatior(element):
    return {"designator": element.text}

def helper_tdloAnnoation(element):
    content = None

    note = element[0]
    propertyNameTag = note.find(make_tag("aixm", "propertyName"))
    if(propertyNameTag is not None):
        propertyName = propertyNameTag.text.lower()
        propertyText = "".join(note.find(make_tag("aixm", "translatedNote")).itertext()).strip()

        if("righthandtrafficpattern" in propertyName):
            rhTraffic = force_bool(propertyText)
            content = {"right_traffic_pattern": rhTraffic}
        elif("endtruebearing" in propertyName):
            content = {"true_bearing": float(propertyText)}
        elif("ilstype" in propertyName):
            content = {"ils_type": propertyText}
        else:
            print "Unknown Touchdown Liftoff property '{0}'".format(propertyName)

    return content

def helper_tdloAssociatedAH(element):
    associatedString = element.get(make_tag("xlink", "href"))
    startPos = associatedString.find("id='") + len("id='")
    endPos = associatedString.find("'", startPos)
    return {"airport_faa_id": associatedString[startPos:endPos]}

def parse_touchDownLiftOff(element):
    tdlo = {}

    for item in element[0][0]:
        if((item.tag in TOUCHDOWN_LIFTOFF_PARSERS) and (TOUCHDOWN_LIFTOFF_PARSERS[item.tag] is not None)):
            result = TOUCHDOWN_LIFTOFF_PARSERS[item.tag](item)
            if(result is not None):
                tdlo.update(result)

        item.clear()
    return tdlo

TOUCHDOWN_LIFTOFF_PARSERS = {
    make_tag("aixm", "designator"): helper_tdtoDesignatior,
    make_tag("aixm", "annotation"): helper_tdloAnnoation,
    make_tag("aixm", "associatedAirportHeliport"): helper_tdloAssociatedAH
}

TABLES = {
    "airports" : {
        "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
        "faa_id": ["VARCHAR(32)", "UNIQUE"],
        "designator": ["VARCHAR(8)"],
        "name": ["VARCHAR(64)"],
        "activated": ["VARCHAR(64)"],  # TODO: Datetime
        "icao_name": ["VARCHAR(64)"],
        "type": ["VARCHAR(64)"],
        "private_use": ["BOOLEAN"],
        "control_type": ["VARCHAR(64)"],
        "field_elevation": ["FLOAT"],
        "magnetic_variation": ["FLOAT"],
        "wind_direction_indicator": ["BOOLEAN"],
        "served_city": ["VARCHAR(128)"],
        "latitude": ["FLOAT"],
        "longitude": ["FLOAT"],
        "remarks": ["TEXT"],
        "sectional_chart": ["VARCHAR(64)"],
        "lighting_schedule": ["VARCHAR(64)"],
        "beacon_lighting_schedule": ["VARCHAR(64)"],
        "marker_lens_color": ["VARCHAR(64)"],
        "traffic_control_tower_on_airport": ["BOOLEAN"],
        "segmented_circle_marker_on_airport": ["BOOLEAN"],
        "attendance_schedule": ["VARCHAR(64)"]
    }
    "runways" : {
        "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
        "faa_id": ["VARCHAR(32)", "UNIQUE"],
        "airport_faa_id": ["VARCHAR(32)"],
        "designator": ["VARCHAR(16)"],
        "length": ["FLOAT"],
        "width": ["FLOAT"],
        "preparation": ["VARCHAR(64)"],
        "composition": ["VARCHAR(64)"],
        "condition": ["VARCHAR(64)"],
        "right_traffic_pattern": ["BOOLEAN"],
        "true_bearing": ["FLOAT"],
        "ils_type": ["VARCHAR(64)"]
    }
}


def reset_tables(dbConn):
    c = dbConn.cursor()
    for table in TABLES:
        # Create the table if it already doesn't exist
        query = "CREATE TABLE IF NOT EXISTS {0}(".format(table)
        for column in TABLES[table]:
            query += "{0} {1}, ".format(column, " ".join(TABLES[table][column]))
        query = query[:-2] + ")"
        c.execute(query)

        # Make sure the tables are empty:
        query = "DELETE FROM {0}".format(table)
        c.execute(query)
    c.close()

def insert_into_db_table_airports(dbConn, airport):
    c = dbConn.cursor()
    columns = ""
    values = ""
    for item in airport:
        if(item not in TABLES["airports"]):
            "Unknown item '{0}' when inserting row into airports table".format(item)

    query = "INSERT INTO airports ({0}) VALUES ({1})".format(", ".join(airport.keys()),
                                                             ", ".join("?"*len(airport)))
    c.execute(query, airport.values())
    c.close()

def insert_into_db_table_runways(dbConn, runway):
    c = dbConn.cursor()
    columns = ""
    values = ""
    for item in runway:
        if(item not in TABLES["runways"]):
            "Unknown item '{0}' when inserting row into runways table".format(item)

    query = "INSERT INTO runways ({0}) VALUES ({1})".format(", ".join(runway.keys()),
                                                             ", ".join("?"*len(runway)))
    c.execute(query, runway.values())
    c.close()

def update_runway_db_with_tdlo_info(db, tdlo):
    c = dbConn.cursor()

    designator = tdlo["designator"]
    airport_faa_id = tdlo["airport_faa_id"]

    tdlo.pop("designator")
    tdlo.pop("airport_faa_id")

    queryNames = []
    queryValues = []
    for item in tdlo:
        if(item not in TABLES["runways"]):
            "Unknown item '{0}' when inserting row into runways table".format(item)
        else:
            queryNames.append("{0} = ?".format(item))
            queryValues.append(tdlo[item])

    if(len(queryNames) != 0):
        query = "UPDATE runways SET {0} WHERE designator = ? AND airport_faa_id = ?".format(
            ", ".join(queryNames))
        c.execute(query, queryValues + [designator, airport_faa_id])
    return

dbConn = sqlite3.connect(AIRPORT_DB)
reset_tables(dbConn)


inhibitClearing = False
TAG_AIRPORT_HELIPORT = make_tag("aixm", "AirportHeliport")
TAG_RUNWAY = make_tag("aixm", "Runway")
TAG_TOUCHDOWN_LIFTOFF = make_tag("aixm", "TouchDownLiftOff")
for event, elem in etree.iterparse(aptXML, events=("start", "end")):
    if(elem.tag == TAG_AIRPORT_HELIPORT):
        if(event == "start"):
            inhibitClearing = True
        else:
            inhibitClearing = False
            #airport = parse_airport(elem)
            #insert_into_db_table_airports(dbConn, airport)
            #print " => Parsed airport {0}".format(airport["name"])

    elif(elem.tag == TAG_RUNWAY):
        if(event == "start"):
            inhibitClearing = True
        else:
            inhibitClearing = False
            runway = parse_runway(elem)
            insert_into_db_table_runways(dbConn, runway)
            print " => Inserted runway '{0}' as '{1}'".format(runway["designator"],
                                                              runway["faa_id"])

    elif(elem.tag == TAG_TOUCHDOWN_LIFTOFF):
        if(event == "start"):
            inhibitClearing = True
        else:
            inhibitClearing = False
            tdlo = parse_touchDownLiftOff(elem)
            update_runway_db_with_tdlo_info(dbConn, tdlo)
            print " => Updated runway with touchdown liftoff info"
        
    if(inhibitClearing == False):
        elem.clear()

dbConn.commit()
dbConn.close()
sys.exit(0)
