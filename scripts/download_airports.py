#!/usr/bin/python

import datetime
import os
import Rotobox
import shutil
import subprocess
import sys

def process_dtp_chart(charts):
    last_apt_ident = None
    airport_id = None

    for chart in charts:
        # Do some caching of the airport identifier so we don't have to do a lookup for every chart.
        if(last_apt_ident != chart["apt_ident"]):
            last_apt_ident = chart["apt_ident"]
            airport_id = db.fetch_airport_id_for_designator(chart["apt_ident"])

        # Make sure that the airport ID lookup succeeded
        if(airport_id is None):
            print "WARN: Airport ID lookup failed for '{0}'".format(chart["apt_ident"])
            continue

        # We need to repackage the chart to match what is expected for the DB
        chart_db = {
            "airport_id": airport_id,
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

            # TODO: Need to catch if existing chart is from previous cycle.
            if(os.path.exists(svg_target) is False):
                print " => Downloading airport directory for {0}".format(chart["apt_ident"])
                nasr.download_with_progress(chart_db["url"], target_dir)
                command = ["pdftocairo", "-svg", target_dir, svg_target]
                subprocess.call(command)
                os.remove(target_dir)

            chart_db["filename"] = os.path.basename(svg_target)

        # Insert the result to the DB
        db.insert_terminal_procedure_url(chart_db)

# Ensure current working directory is the script's path
SCRIPT_DIR = os.path.abspath(os.path.dirname(sys.argv[0]))
os.chdir(SCRIPT_DIR)

ROTOBOX_ROOT = os.path.dirname(SCRIPT_DIR)
AIRPORT_DB = os.path.join(ROTOBOX_ROOT, "rotobox.sqlite")
AIRPORT_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "airports")
AIRPORT_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "airports")
AIRSPACES_PROCESSED_DIRECTORY = os.path.join(os.path.dirname(SCRIPT_DIR), "wwwroot", "airspaces")

# Ensure the chart output directory exists.
if(os.path.exists(AIRPORT_PROCESSED_DIRECTORY) is False):
    os.makedirs(AIRPORT_PROCESSED_DIRECTORY)

print "Airport DB:\t\t{0}".format(AIRPORT_DB)
print "Airport Directory:\t{0}".format(AIRPORT_DIRECTORY)
print "Airport WWW Root:\t{0}".format(AIRPORT_PROCESSED_DIRECTORY)

# DO WORK
nasr = Rotobox.FAA_NASR_Data(AIRPORT_DIRECTORY)
db = Rotobox.Database(AIRPORT_DB)
db.verify_tables(fix=True)

# NASR XML Data
if(db.get_product_updated_cycle("aixm") != nasr.get_current_cycle()):
    nasr.update_aixm()
    db.reset_tables(["airports", "radio", "runways"])

    print "Parsing '{0}'".format(nasr.get_filepath_apt())
    parser = Rotobox.XML_Parser(nasr.get_filepath_apt())
    parser.register(Rotobox.FAA_AirportParser, db.insert_into_db_table_airports)
    parser.register(Rotobox.FAA_RunwayParser, db.insert_into_db_table_runways)
    parser.register(Rotobox.FAA_RadioCommunicationServiceParser, db.update_radio_db_with_frequency)
    parser.register(Rotobox.FAA_TouchDownLiftOffParser, db.update_runway_db_with_tdlo_info)
    parser.register(Rotobox.FAA_AirTrafficControlServiceParser, db.insert_into_db_table_radio)

    parser.run()
    # Make a note of the cycle we just updated with
    db.set_table_updated_cycle("aixm", nasr.get_current_cycle())
    db.commit()
else:
    print " => DB is already up to date with AIXM data!"
print " => Done!"

# # D-TPP, mainly interested in the airport diagrams
# if(db.get_product_updated_cycle("dtpp") != str(nasr.get_procedures_cycle())):
#     nasr.update_dtpp()
#     print "Updating airport diagrams"
#     print " => Emptying DB table 'tpp'"
#     db.reset_table("tpp")

#     print " => Parsing DTPP XML"
#     parser = Rotobox.XML_Parser(nasr.get_filepath_dtpp())
#     parser.register(Rotobox.FAA_DtppAirportParser, process_dtp_chart)

#     parser.run()
    
#     db.set_table_updated_cycle("dtpp", nasr.get_procedures_cycle());
#     db.commit()
# else:
#     print " => Already have the latest charts!"
# print " => Done!"

# Airspace shapefiles
if(db.get_product_updated_cycle("airspaces") != nasr.get_current_cycle()):
    nasr.update_airspace_shapefiles()

    db.reset_table("airspaces")
    for file in nasr.get_filepath_airport_shapefiles():
        print " => Processing '{0}'".format(file)

        # Make a nice name for this airspace
        if("class_b" in file):
            airspace_type = "class_b"
        elif("class_c" in file):
            airspace_type = "class_c"
        elif("class_d" in file):
            airspace_type = "class_d"
        elif("class_e5" in file):
            airspace_type = "class_e5"
        else:
            airspace_type = "class_e"

        shape = Rotobox.Shapefile(file)
        for feature in shape.get_features():
            feature.update({"type": airspace_type})
            db.insert_processed_airspace_shapefile(feature)

    db.set_table_updated_cycle("airspaces", nasr.get_current_cycle());
else:
    print " => Already have the latest airspace shapefiles!"
print " => Done!"

# # Legacy Products
# if(db.get_product_updated_cycle("twr") != nasr.get_current_cycle()):
#     nasr.update_legacy_products()
#     parser = Rotobox.Legacy_FIX_Parser(nasr.get_filepath_legacy_products("FIX"))
#     results = parser.run()

#     for fix in results:
#         print results[fix].keys()
#         break

db.commit()
db.close()
