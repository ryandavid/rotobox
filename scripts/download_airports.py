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

# Definition of tables to store in sqlite
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
    },
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
    },
    "radio" : {
        "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
        "faa_id": ["VARCHAR(32)", "UNIQUE"],
        "airport_faa_id": ["VARCHAR(32)"],
        "radio_comm_id": ["VARCHAR(32)"],
        "channel_name": ["VARCHAR(32)"],
        "tx_frequency": ["FLOAT"],
        "rx_frequency": ["FLOAT"]
    }
}

class FAA_GenericParser(object):
    XML_TAGS = {
        "faa": "http://www.faa.gov/aixm5.1",
        "apt": "http://www.faa.gov/aixm5.1/apt",
        "gml": "http://www.opengis.net/gml/3.2",
        "xsi": "http://www.w3.org/2001/XMLSchema-instance",
        "aixm": "http://www.aixm.aero/schema/5.1",
        "gmd": "http://www.isotc211.org/2005/gmd",
        "gco": "http://www.isotc211.org/2005/gco",
        "xlink": "http://www.w3.org/1999/xlink"
    }

    SUPPORTED_TAG = None

    def __init__(self):
        self.print_parser_name()
        self.create_tag_table()

    def create_tag_table(self):
        return

    def make_tag(self, ns, uri):
        return "{" + self.XML_TAGS[ns] + "}" + uri

    def force_bool(self, value):
        forcedValue = False

        if(isinstance(value, int) == True):
            if(value > 0):
                forcedValue = True
        elif(isinstance(value, str) == True):
            if(value.lower() in ["t", "true", "1", "y", "yes"]):
                forcedValue = True
                
        return forcedValue

    def print_parser_name(self):
        print " => Setting up {0}".format(self.__class__.__name__)

class FAA_AirportParser(FAA_GenericParser):
    def __init__(self):
        self.SUPPORTED_TAG = self.make_tag("aixm", "AirportHeliport")
        super(FAA_AirportParser, self).__init__()

    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "featureLifetime"): self.helper_featureLifetime,
            self.make_tag("aixm", "designator"): self.helper_designator,
            self.make_tag("aixm", "name"): self.helper_name,
            self.make_tag("aixm", "locationIndicatorICAO"): self.helper_locationIndicatorICAO,
            self.make_tag("aixm", "type"): self.helper_type,
            self.make_tag("aixm", "privateUse"): self.helper_privateUse,
            self.make_tag("aixm", "controlType"): self.helper_controlType,
            self.make_tag("aixm", "fieldElevation"): self.helper_fieldElevation,
            self.make_tag("aixm", "magneticVariation"): self.helper_magneticVariation,
            self.make_tag("aixm", "windDirectionIndicator"): self.helper_windDirectionIndicator,
            self.make_tag("aixm", "servedCity"): self.helper_servedCity,
            self.make_tag("aixm", "ARP"): self.helper_ARP,
            self.make_tag("aixm", "annotation"): self.helper_annotation,
            self.make_tag("aixm", "extension"): self.helper_extension,
            self.make_tag("apt", "aeronauticalSectionalChart"): self.helper_aeronauticalSectionalChart,
            self.make_tag("apt", "lightingSchedule"): self.helper_lightingSchedule,
            self.make_tag("apt", "beaconLightingSchedule"): self.helper_beaconLightingSchedule,
            self.make_tag("apt", "markerLensColor"): self.helper_markerLensColor,
            self.make_tag("apt", "trafficControlTowerOnAirport"): self.helper_trafficControlTowerOnAirport,
            self.make_tag("apt", "segmentedCircleMarkerOnAirport"): self.helper_segmentedCircleMarkerOnAirport,
            self.make_tag("apt", "airportAttendanceSchedule"): self.helper_airportAttendanceSchedule
        }

    def helper_designator(self, element):
        return {"designator": element.text}

    def helper_featureLifetime(self, element):
        return {"activated": "".join(element.itertext()).strip()}

    def helper_name(self, element):
        return {"name": element.text}

    def helper_locationIndicatorICAO(self, element):
        return {"icao_name": element.text}

    def helper_type(self, element):
        return {"type": element.text}

    def helper_extension(self, element):
        results = {}
        for item in element.iter():
            if(item.tag == self.make_tag("aixm", "extension")):
                continue
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                results.update(self.PARSERS[item.tag](item))
                item.clear()
        return results

    def helper_aeronauticalSectionalChart(self, element):
        return {"sectional_chart": element.text}

    def helper_lightingSchedule(self, element):
        return {"lighting_schedule": element.text}

    def helper_beaconLightingSchedule(self, element):
        return {"beacon_lighting_schedule": element.text}

    def helper_markerLensColor(self, element):
        return {"marker_lens_color": element.text}

    def helper_trafficControlTowerOnAirport(self, element):
        value = element.text.lower()
        return {"traffic_control_tower_on_airport": self.force_bool(element.text)}

    def helper_segmentedCircleMarkerOnAirport(self, element):
        return {"segmented_circle_marker_on_airport": self.force_bool(element.text)}

    def helper_airportAttendanceSchedule(self, element):
        return {"attendance_schedule": element.text}

    def helper_privateUse(self, element):
        return {"private_use": self.force_bool(element.text)}

    def helper_controlType(self, element):
        return {"control_type": element.text} 

    def helper_fieldElevation(self, element):
        units = element.get("uom")
        if((units is not None) and (units.lower() != "ft")):
            print "ERROR: Invalid units for field elevation ({0})".format(units)
        return {"field_elevation": float(element.text)} 

    def helper_magneticVariation(self, element):
        return {"magnetic_variation": element.text}

    def helper_windDirectionIndicator(self, element):
        return {"wind_direction_indicator": self.force_bool(element.text)} 

    def helper_servedCity(self, element):
        # TODO: Stop being so lazy and actually drill down to the child node
        allText = "".join(element.itertext()).strip()
        return {"served_city": allText} 

    def helper_ARP(self, element):
        # TODO: Stop being so lazy and actually drill down to the child node
        allText = "".join(element.itertext()).strip()
        coord = allText.split(" ")
        return {"longitude": float(coord[0]), "latitude": float(coord[1])}

    def helper_annotation(self, element):
        content = None
        for item in element.iter(self.make_tag("aixm", "LinguisticNote")):
            itemType = item.get(self.make_tag("gml", "id"))

            # TODO: Be smarter about the annotation types:
            # "REMARK", "WINDDIRECTION_NOTE", "SEAPLANE_BASE_NOTE", "GLIDERPORT_NOTE", "ULTRALIGHT_NOTE"
            if("REMARK_NAME" in itemType):
                itemType

            else:
                # TODO: Stop being so lazy and actually drill down to the child node
                allText = "".join(item.itertext()).strip()
                content = {"remarks": allText}

        return content

    def parse(self, element):
        airport = {
            "faa_id": element.get(self.make_tag("gml", "id")),
            "remarks": "",
        }

        # Don't use iter since we just want the direct children
        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                if(result is not None):
                    # Special case to append remarks
                    if(result.keys()[0] == "remarks"):
                        airport["remarks"] += result["remarks"] + "  "
                    else:
                        airport.update(result)

                item.clear()
        airport["remarks"] = airport["remarks"].strip()
        return airport

class FAA_RunwayParser(FAA_GenericParser):
    def __init__(self):
        self.SUPPORTED_TAG = self.make_tag("aixm", "Runway")
        super(FAA_RunwayParser, self).__init__()

    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "associatedAirportHeliport"): self.helper_associatedAirportHeliport,
            self.make_tag("aixm", "designator"): self.helper_runwayDesignatior,
            self.make_tag("aixm", "lengthStrip"): self.helper_runwayLength,
            self.make_tag("aixm", "widthStrip"): self.helper_runwayWidth,
            self.make_tag("aixm", "surfaceProperties"): self.helper_runwaySurface,
        }

    def helper_associatedAirportHeliport(self, element):
        associatedString = element.get(self.make_tag("xlink", "href"))
        startPos = associatedString.find("id='") + len("id='")
        endPos = associatedString.find("'", startPos)
        return {"airport_faa_id": associatedString[startPos:endPos]}

    def helper_runwayDesignatior(self, element):
        return {"designator": element.text}

    def helper_runwayLength(self, element):
        units = element.get("uom")
        if((units is not None) and (units.lower() != "ft")):
            print "ERROR: Invalid units for runway length ({0})".format(units)
        return {"length": float(element.text)}

    def helper_runwayWidth(self, element):
        units = element.get("uom")
        if((units is not None) and (units.lower() != "ft")):
            print "ERROR: Invalid units for runway length ({0})".format(units)
        return {"width": float(element.text)}

    def helper_runwaySurface(self, element):
        surfaceCharacteristics = element[0]
        compositionTag = surfaceCharacteristics.find(self.make_tag("aixm", "composition"))
        if(compositionTag is not None):
            composition = compositionTag.text
        else:
            composition = None

        preparationTag = surfaceCharacteristics.find(self.make_tag("aixm", "preparation"))
        if(preparationTag is not None):
            preparation = preparationTag.text
        else:
            preparation = None

        conditionTag = surfaceCharacteristics.find(self.make_tag("aixm", "surfaceCondition"))
        if(conditionTag is not None):
            condition = conditionTag.text
        else:
            condition = None

        return {"composition": composition, "preparation": preparation, "condition": condition}

    def parse(self, element):
        runway = {
            "faa_id": element.get(self.make_tag("gml", "id")),
        }

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                runway.update(result)

            item.clear()
        return runway

class FAA_RadioCommunicationServiceParser(FAA_GenericParser):
    def __init__(self):
        self.SUPPORTED_TAG = self.make_tag("aixm", "RadioCommunicationChannel")
        super(FAA_RadioCommunicationServiceParser, self).__init__()

    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "interpretation"): self.helper_radioInterpretation,
            self.make_tag("aixm", "frequencyTransmission"): self.helper_frequency_transmission,
            self.make_tag("aixm", "frequencyReception"): self.helper_frequency_reception
        }

    def helper_radioInterpretation(self, element):
        return {"channel_name": element.text}

    def helper_frequency_transmission(self, element):
        units = element.get("MHZ")
        if((units is not None) and (units.lower() != "ft")):
            print "ERROR: Invalid units transmission frequency ({0})".format(units)
        return {"tx_frequency": float(element.text)}

    def helper_frequency_reception(self, element):
        units = element.get("MHZ")
        if((units is not None) and (units.lower() != "ft")):
            print "ERROR: Invalid units reception frequency ({0})".format(units)
        return {"rx_frequency": float(element.text)}

    def parse(self, element):
        radio = {
            "radio_comm_id": element.get(self.make_tag("gml", "id"))
        }

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                radio.update(result)
        return radio

class FAA_TouchDownLiftOffParser(FAA_GenericParser):
    def __init__(self):
        self.SUPPORTED_TAG = self.make_tag("aixm", "TouchDownLiftOff")
        super(FAA_TouchDownLiftOffParser, self).__init__()

    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "designator"): self.helper_tdtoDesignatior,
            self.make_tag("aixm", "annotation"): self.helper_tdloAnnoation,
            self.make_tag("aixm", "associatedAirportHeliport"): self.helper_tdloAssociatedAH
        }

    def helper_tdtoDesignatior(self, element):
        return {"designator": element.text}

    def helper_tdloAnnoation(self, element):
        content = None

        note = element[0]
        propertyNameTag = note.find(self.make_tag("aixm", "propertyName"))
        if(propertyNameTag is not None):
            propertyName = propertyNameTag.text.lower()
            propertyText = "".join(note.find(self.make_tag("aixm", "translatedNote")).itertext()).strip()

            if("righthandtrafficpattern" in propertyName):
                rhTraffic = self.force_bool(propertyText)
                content = {"right_traffic_pattern": rhTraffic}
            elif("endtruebearing" in propertyName):
                content = {"true_bearing": float(propertyText)}
            elif("ilstype" in propertyName):
                content = {"ils_type": propertyText}
            else:
                print "Unknown Touchdown Liftoff property '{0}'".format(propertyName)

        return content

    def helper_tdloAssociatedAH(self, element):
        associatedString = element.get(self.make_tag("xlink", "href"))
        startPos = associatedString.find("id='") + len("id='")
        endPos = associatedString.find("'", startPos)
        return {"airport_faa_id": associatedString[startPos:endPos]}

    def parse(self, element):
        tdlo = {}

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                if(result is not None):
                    tdlo.update(result)

            item.clear()
        return tdlo

class FAA_AirTrafficControlServiceParser(FAA_GenericParser):
    def __init__(self):
        self.SUPPORTED_TAG = self.make_tag("aixm", "AirTrafficControlService")
        super(FAA_AirTrafficControlServiceParser, self).__init__()

    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "radioCommunication"): self.helper_radioCommunication,
            self.make_tag("aixm", "clientAirport"): self.helper_clientAirport
        }

    def helper_radioCommunication(self, element):
        radioCommChannel = element.get(self.make_tag("xlink", "href"))
        # NOTE the wierd formatting for 'id'
        startPos = radioCommChannel.find("id ='") + len("id ='")
        endPos = radioCommChannel.find("'", startPos)
        return {"radio_comm_id": radioCommChannel[startPos:endPos]}

    def helper_clientAirport(self, element):
        clientAirport = element.get(self.make_tag("xlink", "href"))
        # NOTE the wierd formatting for 'id'
        startPos = clientAirport.find("id ='") + len("id ='")
        endPos = clientAirport.find("'", startPos)
        return {"airport_faa_id": clientAirport[startPos:endPos]}

    def parse(self, element):
        atc = {
            "faa_id": element.get(self.make_tag("gml", "id"))
        }

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                atc.update(result)
        return atc

class FAA_NASR_Data():
    URL_CYCLES_LIST = "https://enasr.faa.gov/eNASR/nasr/ValueList/Cycle"
    URL_AIXM = "https://nfdc.faa.gov/webContent/56DaySub/{0}/aixm5.1.zip"
    CHART_TYPES = ["sectional", "terminalArea", "world", "helicopter"]

    def __init__(self):
        self.cycles = {}
        self.filepath_apt_xml = None
        self.filepath_awos_xml = None
        self.filepath_awy_aixm = None
        self.filepath_nav_aixm = None

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
        print "Downloading NASR AIXM Data".format(self.cycles["current"])
        if(os.path.exists(downloadFullpath) is False):
            print " => Fetching current subscription ({0})".format(self.cycles["current"])
            self.download_aixm("current", downloadFullpath)
        else:
            print " => Already have current subscription ({0})".format(self.cycles["current"])

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

        for filepath in extractedFilepaths:
            if("APT_AIXM" in filepath):
                self.filepath_apt_xml = filepath
            elif("AWOS_AIXM" in filepath):
                self.filepath_awos_xml = filepath
            elif("AWY_AIXM" in filepath):
                self.filepath_awy_xml = filepath
            elif("NAV_AIXM" in filepath):
                self.filepath_nav_xml = filepath

        return extractedFilepaths

    def get_current_cycle(self):
        return self.cycles["current"]

    def get_filepath_apt(self):
        return self.filepath_apt_xml

    def get_filepath_awos(self):
        return self.filepath_awos_xml

    def get_filepath_awy(self):
        return self.filepath_awy_xml

    def get_filepath_nav(self):
        return self.filepath_nav_xml

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

def insert_into_db_table_radio(dbConn, radio):
    c = dbConn.cursor()
    columns = ""
    values = ""
    for item in radio:
        if(item not in TABLES["radio"]):
            "Unknown item '{0}' when inserting row into radio table".format(item)

    query = "INSERT INTO radio ({0}) VALUES ({1})".format(", ".join(radio.keys()),
                                                             ", ".join("?"*len(radio)))
    c.execute(query, radio.values())
    c.close()

def update_radio_db_with_frequency(dbConn, radio):
    c = dbConn.cursor()

    radio_comm_id = radio["radio_comm_id"]
    radio.pop("radio_comm_id")

    queryNames = []
    queryValues = []
    for item in radio:
        if(item not in TABLES["radio"]):
            "Unknown item '{0}' when updating row in radio table".format(item)
        else:
            queryNames.append("{0} = ?".format(item))
            queryValues.append(radio[item])

    queryValues.append(radio_comm_id)

    if(len(queryNames) != 0):
        query = "UPDATE radio SET {0} WHERE radio_comm_id = ?".format(", ".join(queryNames))
        c.execute(query, queryValues)
    return


# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

AIRPORT_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "airports")
AIRPORT_CONFIG = os.path.join(AIRPORT_DIRECTORY, "airport_config.json")
AIRPORT_DB = os.path.join(AIRPORT_DIRECTORY, "airport_db.sqlite")
AIRPORT_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "airports")

# Ensure the chart directory exists.
if(os.path.exists(AIRPORT_DIRECTORY) is False):
    os.makedirs(AIRPORT_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(AIRPORT_PROCESSED_DIRECTORY) is False):
    os.makedirs(AIRPORT_PROCESSED_DIRECTORY)

print "Airport Directory:\t{0}".format(AIRPORT_DIRECTORY)
print "Airport Configuration:\t{0}".format(AIRPORT_CONFIG)
print "Airport Output:\t\t{0}".format(AIRPORT_PROCESSED_DIRECTORY)

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

# DO WORK
nasr = FAA_NASR_Data()
files = nasr.update_all(AIRPORT_DIRECTORY)

dbConn = sqlite3.connect(AIRPORT_DB)
reset_tables(dbConn)

print "Parsing '{0}'".format(nasr.get_filepath_apt())
airportParser = FAA_AirportParser()
runwayParser = FAA_RunwayParser()
radioParser = FAA_RadioCommunicationServiceParser()
touchdownParser = FAA_TouchDownLiftOffParser()
atcParser = FAA_AirTrafficControlServiceParser()

parsers = {
    airportParser.SUPPORTED_TAG: {
        "parser": airportParser.parse,
        "db_action": insert_into_db_table_airports
    },
    runwayParser.SUPPORTED_TAG: {
        "parser": runwayParser.parse,
        "db_action": insert_into_db_table_runways
    },
    radioParser.SUPPORTED_TAG: {
        "parser": radioParser.parse,
        "db_action": update_radio_db_with_frequency
    },
    touchdownParser.SUPPORTED_TAG: {
        "parser": touchdownParser.parse,
        "db_action": update_runway_db_with_tdlo_info
    },
    atcParser.SUPPORTED_TAG: {
        "parser": atcParser.parse,
        "db_action": insert_into_db_table_radio
    }
}

inhibitClearing = False
for event, elem in etree.iterparse(nasr.get_filepath_apt(), events=("start", "end")):
    if(elem.tag in parsers):
        if(event == "start"):
            inhibitClearing = True
        else:
            inhibitClearing = False
            result = parsers[elem.tag]["parser"](elem)
            parsers[elem.tag]["db_action"](dbConn, result)

    if(inhibitClearing == False):
        elem.clear()

print " => Done!"

dbConn.commit()
dbConn.close()
sys.exit(0)
