#!/usr/bin/env python

from clint.textui import progress
import os
import sqlite3
import xml.etree.ElementTree as etree
import datetime
import json
import os
import re
import requests
import shutil
import subprocess
import sys
import zipfile

class Database():
    # Definition of tables to store in sqlite
    TABLES = {
        "airports" : {
            "id": ["VARCHAR(32)", "PRIMARY KEY", "UNIQUE"],
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
            "attendance_schedule": ["VARCHAR(64)"],
        },
        "runways" : {
            "id": ["VARCHAR(32)", "PRIMARY KEY", "UNIQUE"],
            "airport_id": ["VARCHAR(32)"],
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
            "id": ["VARCHAR(64)", "PRIMARY KEY", "UNIQUE"],
            "airport_id": ["VARCHAR(32)"],
            "channel_name": ["VARCHAR(32)"],
            "tx_frequency": ["FLOAT"],
            "rx_frequency": ["FLOAT"]
        },
        "tpp" : {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "airport_id": ["VARCHAR(32)"],
            "chart_code": ["VARCHAR(32)"],
            "chart_name": ["VARCHAR(64)"],
            "filename": ["VARCHAR(64)"],
            "url": ["VARCHAR(256)"]
        }
    }

    def __init__(self, db_path):
        self.dbConn = sqlite3.connect(db_path)

    def close(self):
        self.dbConn.close()

    def commit(self):
        self.dbConn.commit()

    def reset_table(self, table):
        success = False

        if(table in self.TABLES):
            c = self.dbConn.cursor()
            # Drop existing table and recreate it
            query = "DROP TABLE IF EXISTS {0}".format(table)
            c.execute(query)

            query = "CREATE TABLE IF NOT EXISTS {0}(".format(table)
            for column in self.TABLES[table]:
                query += "{0} {1}, ".format(column, " ".join(self.TABLES[table][column]))
            query = query[:-2] + ")"
            c.execute(query)

            c.close()
            self.dbConn.commit()
            success = True

        return success

    def reset_tables(self):
        for table in self.TABLES:
            self.reset_table(table)

    def insert_into_db_table_airports(self, airport):
        c = self.dbConn.cursor()
        columns = ""
        values = ""
        for item in airport:
            if(item not in self.TABLES["airports"]):
                "Unknown item '{0}' when inserting row into airports table".format(item)

        query = "INSERT INTO airports ({0}) VALUES ({1})".format(", ".join(airport.keys()),
                                                                 ", ".join("?"*len(airport)))
        c.execute(query, airport.values())
        c.close()

    def insert_into_db_table_runways(self, runway):
        c = self.dbConn.cursor()
        columns = ""
        values = ""
        for item in runway:
            if(item not in self.TABLES["runways"]):
                "Unknown item '{0}' when inserting row into runways table".format(item)

        query = "INSERT INTO runways ({0}) VALUES ({1})".format(", ".join(runway.keys()),
                                                                 ", ".join("?"*len(runway)))
        c.execute(query, runway.values())
        c.close()

    def update_runway_db_with_tdlo_info(self, tdlo):
        c = self.dbConn.cursor()

        designator = tdlo["designator"]
        airport_id = tdlo["airport_id"]

        tdlo.pop("designator")
        tdlo.pop("airport_id")

        queryNames = []
        queryValues = []
        for item in tdlo:
            if(item not in self.TABLES["runways"]):
                "Unknown item '{0}' when inserting row into runways table".format(item)
            else:
                queryNames.append("{0} = ?".format(item))
                queryValues.append(tdlo[item])

        if(len(queryNames) != 0):
            query = "UPDATE runways SET {0} WHERE designator = ? AND airport_id = ?".format(
                ", ".join(queryNames))
            c.execute(query, queryValues + [designator, airport_id])


    def insert_into_db_table_radio(self, radio):
        c = self.dbConn.cursor()
        columns = ""
        values = ""
        for item in radio:
            if(item not in self.TABLES["radio"]):
                "Unknown item '{0}' when inserting row into radio table".format(item)

        query = "INSERT INTO radio ({0}) VALUES ({1})".format(", ".join(radio.keys()),
                                                                 ", ".join("?"*len(radio)))
        c.execute(query, radio.values())
        c.close()

    def update_radio_db_with_frequency(self, radio):
        c = self.dbConn.cursor()
        radio_comm_id = radio["id"]
        radio.pop("id")

        queryNames = []
        queryValues = []
        for item in radio:
            if(item not in self.TABLES["radio"]):
                "Unknown item '{0}' when updating row in radio table".format(item)
            else:
                queryNames.append("{0} = ?".format(item))
                queryValues.append(radio[item])

        queryValues.append(radio_comm_id)

        if(len(queryNames) != 0):
            query = "UPDATE radio SET {0} WHERE id = ?".format(", ".join(queryNames))
            c.execute(query, queryValues)

    def insert_terminal_procedure_url(self, chart):
        c = self.dbConn.cursor()
        columns = ""
        values = ""
        for item in chart:
            if(item not in self.TABLES["tpp"]):
                "Unknown item '{0}' when inserting row into tpp table".format(item)

        query = "INSERT INTO tpp ({0}) VALUES ({1})".format(", ".join(chart.keys()),
                                                                 ", ".join("?"*len(chart)))
        c.execute(query, chart.values())
        c.close()

    def fetch_airport_id_for_designator(self, designator):
        airport_id = None

        c = self.dbConn.cursor()
        query = "SELECT id FROM airports WHERE designator = ?;"
        c.execute(query, (designator,))

        result = c.fetchone()
        if(result is not None):
            airport_id = result[0]

        return airport_id

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

    def __init__(self):
        self.print_parser_name()
        self.create_tag_table()
        self.SUPPORTED_TAG = self.get_supported_tag()

    def get_supported_tag(self):
        return

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

    def parse(self, element):
        return

class FAA_AirportParser(FAA_GenericParser):
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

    def get_supported_tag(self):
        return self.make_tag("aixm", "AirportHeliport")

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
                # Replace all double-quotes with single quotes.
                allText = allText.replace("\"", "'")
                content = {"remarks": allText}

        return content

    def parse(self, element):
        airport = {
            "id": element.get(self.make_tag("gml", "id")),
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
    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "associatedAirportHeliport"): self.helper_associatedAirportHeliport,
            self.make_tag("aixm", "designator"): self.helper_runwayDesignatior,
            self.make_tag("aixm", "lengthStrip"): self.helper_runwayLength,
            self.make_tag("aixm", "widthStrip"): self.helper_runwayWidth,
            self.make_tag("aixm", "surfaceProperties"): self.helper_runwaySurface,
        }

    def get_supported_tag(self):
        return self.make_tag("aixm", "Runway")

    def helper_associatedAirportHeliport(self, element):
        associatedString = element.get(self.make_tag("xlink", "href"))
        startPos = associatedString.find("id='") + len("id='")
        endPos = associatedString.find("'", startPos)
        return {"airport_id": associatedString[startPos:endPos]}

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
            "id": element.get(self.make_tag("gml", "id")),
        }

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                runway.update(result)

            item.clear()
        return runway

class FAA_RadioCommunicationServiceParser(FAA_GenericParser):
    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "interpretation"): self.helper_radioInterpretation,
            self.make_tag("aixm", "frequencyTransmission"): self.helper_frequency_transmission,
            self.make_tag("aixm", "frequencyReception"): self.helper_frequency_reception
        }

    def get_supported_tag(self):
        return self.make_tag("aixm", "RadioCommunicationChannel")

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
            "id": element.get(self.make_tag("gml", "id"))
        }

        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                radio.update(result)
        return radio

class FAA_TouchDownLiftOffParser(FAA_GenericParser):
    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "designator"): self.helper_tdtoDesignatior,
            self.make_tag("aixm", "annotation"): self.helper_tdloAnnoation,
            self.make_tag("aixm", "associatedAirportHeliport"): self.helper_tdloAssociatedAH
        }

    def get_supported_tag(self):
        return self.make_tag("aixm", "TouchDownLiftOff")

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
        return {"airport_id": associatedString[startPos:endPos]}

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
    def create_tag_table(self):
        self.PARSERS = {
            self.make_tag("aixm", "radioCommunication"): self.helper_radioCommunication,
            self.make_tag("aixm", "clientAirport"): self.helper_clientAirport
        }

    def get_supported_tag(self):
        return self.make_tag("aixm", "AirTrafficControlService")

    def helper_radioCommunication(self, element):
        radioCommChannel = element.get(self.make_tag("xlink", "href"))
        # NOTE the wierd formatting for 'id'
        startPos = radioCommChannel.find("id ='") + len("id ='")
        endPos = radioCommChannel.find("'", startPos)
        return {"id": radioCommChannel[startPos:endPos]}

    def helper_clientAirport(self, element):
        clientAirport = element.get(self.make_tag("xlink", "href"))
        # NOTE the wierd formatting for 'id'
        startPos = clientAirport.find("id ='") + len("id ='")
        endPos = clientAirport.find("'", startPos)
        return {"airport_id": clientAirport[startPos:endPos]}

    def parse(self, element):
        atc = {}
        for item in element[0][0]:
            if((item.tag in self.PARSERS) and (self.PARSERS[item.tag] is not None)):
                result = self.PARSERS[item.tag](item)
                atc.update(result)
        return atc

class FAA_DtppAirportParser(FAA_GenericParser):
    # The DTPP XML doesn't use the same notion of namespace as the AIXM
    def get_supported_tag(self):
        return "airport_name"

    def parse(self, element):
        charts = []
        apt_ident = element.get("apt_ident")
        for record in element:
            charts.append({
                "apt_ident": apt_ident,
                "chart_name": record.find("chart_name").text,
                "chart_code": record.find("chart_code").text,
                "pdf_name": record.find("pdf_name").text
            })
        return charts

class FAA_DttpAttrParser(FAA_GenericParser):
    # The DTPP XML doesn't use the same notion of namespace as the AIXM
    def get_supported_tag(self):
        return "digital_tpp"

    def parse(self, element):
        return element.get("cycle")

class FAA_NASR_Data():
    URL_CYCLES_LIST = "https://enasr.faa.gov/eNASR/nasr/ValueList/Cycle"
    URL_NASR_SUB = "https://nfdc.faa.gov/webContent/56DaySub/{0}/{1}"  # Cycle, product filename

    NASR_PRODUCT_AIXM = "aixm5.1.zip"
    NASR_PRODUCT_TWR_TXT = "TWR.zip"

    CHART_TYPES = ["sectional", "terminalArea", "world", "helicopter"]

    URL_DTPP_LIST = "https://nfdc.faa.gov/webContent/dtpp/current.xml"
    URL_PROCEDURES = "http://aeronav.faa.gov/d-tpp/{0}/{1}"  # Cycle, chart name
    PROCEDURES_DEFAULT_CYCLE = 1608

    def __init__(self):
        self.cycles = {}
        self.filepath_apt_xml = None
        self.filepath_awos_xml = None
        self.filepath_awy_aixm = None
        self.filepath_nav_aixm = None
        self.procedures_cycle = self.PROCEDURES_DEFAULT_CYCLE

    def download_nasr_aixm(self, cycle, target_path):
        if(cycle in self.cycles):
            url = self.URL_NASR_SUB.format(self.cycles[cycle], NASR_PRODUCT_AIXM)
        else:
            url = self.URL_NASR_SUB.format(self.cycles["current"], NASR_PRODUCT_AIXM)
        self.download_with_progress(url, target_path)

    def download_dtpp_list(self, target_path):
        self.download_with_progress(self.URL_DTPP_LIST, target_path)

    def download_nasr_txt(self, target_path):
        url = self.URL_NASR_SUB.format(self.cycles["current"], NASR_PRODUCT_TWR_TXT)
        self.download_with_progress(url, target_path)

    # Download with progress bar
    # http://stackoverflow.com/a/20943461
    def download_with_progress(self, url, target_path):
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
        print "Updating NASR AIXM Data".format(self.cycles["current"])
        if(os.path.exists(downloadFullpath) is True):
            print " => Already have current subscription ({0})".format(self.cycles["current"])
        else:
            print " => Fetching current subscription ({0})".format(self.cycles["current"])
            self.download_nasr_aixm("current", downloadFullpath)

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

        self.filepath_apt_xml = os.path.join(target_dir, "APT_AIXM.xml")
        self.filepath_awos_xml = os.path.join(target_dir, "AWOS_AIXM.xml")
        self.filepath_awy_xml = os.path.join(target_dir, "AWY_AIXM.xml")
        self.filepath_nav_xml = os.path.join(target_dir, "AWOS_NAV.xml")

        return True

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

    def assemble_procedures_url(self, pdf_name):
        return self.URL_PROCEDURES.format(self.procedures_cycle, pdf_name)

    def set_procedures_cycle(self, cycle):
        self.procedures_cycle = cycle

    def get_procedures_cycle(self):
        return self.procedures_cycle

class XML_Parser():
    def __init__(self, filename):
        self.filename = filename
        self.start_tag_hooks = []
        self.end_tag_hooks = []

        self.start_parsers = {}
        self.end_parsers = {}
        return

    def register_start_tag_hook(self, parser, callback):
        self.start_tag_hooks.append((parser(), callback))

    def register_end_tag_hook(self, parser, callback):
        self.end_tag_hooks.append((parser(), callback))

    def run(self):
        for hook in self.start_tag_hooks:
            self.start_parsers[hook[0].SUPPORTED_TAG] = {
                "action": hook[0].parse,
                "callback": hook[1]
            }

        for hook in self.end_tag_hooks:
            self.end_parsers[hook[0].SUPPORTED_TAG] = {
                "action": hook[0].parse,
                "callback": hook[1]
            }


        inhibitClearing = False
        for event, elem in etree.iterparse(self.filename, events=("start", "end")):
            if((event == "start") and (elem.tag in self.start_parsers)):
                result = self.start_parsers[elem.tag]["action"](elem)
                self.start_parsers[elem.tag]["callback"](result)

            if(elem.tag in self.end_parsers):
                if(event == "start"):
                    inhibitClearing = True
                else:
                    inhibitClearing = False
                    result = self.end_parsers[elem.tag]["action"](elem)
                    self.end_parsers[elem.tag]["callback"](result)

            if(inhibitClearing is False):
                elem.clear()

        return



