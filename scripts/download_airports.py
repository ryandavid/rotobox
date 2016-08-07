#!/usr/bin/python

import datetime
import json
import os
import re
import Rotobox
import requests
import shutil
import subprocess
import sys
import zipfile

# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

ROTOBOX_ROOT = os.path.dirname(SCRIPT_DIR)
AIRPORT_DB = os.path.join(ROTOBOX_ROOT, "rotobox.sqlite")
AIRPORT_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "airports")
AIRPORT_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "airports")

# Ensure the chart directory exists.
if(os.path.exists(AIRPORT_DIRECTORY) is False):
    os.makedirs(AIRPORT_DIRECTORY)

# Ensure the chart output directory exists.
if(os.path.exists(AIRPORT_PROCESSED_DIRECTORY) is False):
    os.makedirs(AIRPORT_PROCESSED_DIRECTORY)

print "Airport DB:\t\t{0}".format(AIRPORT_DB)
print "Airport Directory:\t{0}".format(AIRPORT_DIRECTORY)
print "Airport WWW Root:\t{0}".format(AIRPORT_PROCESSED_DIRECTORY)


# DO WORK
nasr = Rotobox.FAA_NASR_Data()
nasr.update_all(AIRPORT_DIRECTORY)

db = Rotobox.Database(AIRPORT_DB)
db.reset_tables()

print "Parsing '{0}'".format(nasr.get_filepath_apt())
parser = Rotobox.XML_Parser(nasr.get_filepath_apt())
parser.register_end_tag_hook(Rotobox.FAA_AirportParser,
                             db.insert_into_db_table_airports)
parser.register_end_tag_hook(Rotobox.FAA_RunwayParser,
                             db.insert_into_db_table_runways)
parser.register_end_tag_hook(Rotobox.FAA_RadioCommunicationServiceParser,
                             db.update_radio_db_with_frequency)
parser.register_end_tag_hook(Rotobox.FAA_TouchDownLiftOffParser,
                             db.update_runway_db_with_tdlo_info)
parser.register_end_tag_hook(Rotobox.FAA_AirTrafficControlServiceParser,
                             db.insert_into_db_table_radio)
parser.run()
db.commit()


def process_dtp_chart(charts):
    for chart in charts:
        # We need to repackage the chart to match what is expeced for the DB
        chart_db = {
            "airport_id": db.fetch_airport_id_for_designator(chart["apt_ident"]),
            "chart_name": chart["chart_name"],
            "chart_code": chart["chart_code"],
            "filename": "",
            "url": nasr.assemble_procedures_url(chart["pdf_name"])
        }
        
        # If we have the Airport Directory, download it and convert it to SVG
        if(chart["chart_code"] == "APD"):
            basename = os.path.join(AIRPORT_PROCESSED_DIRECTORY,
                                    os.path.splitext(chart["pdf_name"])[0])
            target_dir = basename + ".pdf"
            svg_target = basename + ".svg"

            if(os.path.exists(svg_target) is False):
                print " => Downloading airport directory for {0}".format(chart["apt_ident"])
                nasr.download_with_progress(chart_db["url"], target_dir)
                command = ["pdftocairo", "-svg", target_dir, svg_target]
                subprocess.call(command)
                os.remove(target_dir)

            chart_db["filename"] = os.path.basename(svg_target)

        # Insert the result to the DB
        db.insert_terminal_procedure_url(chart_db)

print "Updating airport diagrams"
# TODO: Check the modified date on the XML to see if it needs refreshed
target_path = os.path.join(AIRPORT_DIRECTORY, "current_dtpp.xml")
if(os.path.exists(target_path) is False):
    print " => Downloading current DTPP XML"
    nasr.download_dtpp_list(target_path)

print " => Parsing DTPP XML"
parser = Rotobox.XML_Parser(target_path)
parser.register_start_tag_hook(Rotobox.FAA_DttpAttrParser, nasr.set_procedures_cycle)
parser.register_end_tag_hook(Rotobox.FAA_DtppAirportParser, process_dtp_chart)
parser.run()
print " => Done!"

db.commit()
db.close()
