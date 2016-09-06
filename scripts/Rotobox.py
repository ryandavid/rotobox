#!/usr/bin/env python

from clint.textui import progress
import datetime
import json
from lxml import html
import ogr
import os
import pyspatialite.dbapi2 as sqlite3
import re
import requests
import shutil
import subprocess
import sys
import xml.etree.ElementTree as etree
import zipfile

class Database():
    # Definition of tables to store in sqlite.  Always keep the column type as the first item in the
    # list.
    TABLES = {
        "airports" : {
            "id": ["VARCHAR(32)", "PRIMARY KEY", "UNIQUE"], # Landing Facility Site Number
            "landing_facility_type": ["VARCHAR(32)"],
            "location_identifier": ["VARCHAR(8)"],
            "effective_date": ["INTEGER"],
            "region_code": ["VARCHAR(4)"],
            "faa_district_code": ["VARCHAR(4)"],
            "state_code": ["VARCHAR(4)"],
            "state_name": ["VARCHAR(32)"],
            "county_name": ["VARCHAR(32)"],
            "county_state": ["VARCHAR(4)"],
            "city_name": ["VARCHAR(64)"],
            "facility_name": ["VARCHAR(64)"],
            "ownership_type": ["VARCHAR(4)"],
            "facility_use": ["VARCHAR(4)"],
            "owner_name": ["VARCHAR(32)"],
            "owner_address": ["VARCHAR(128)"],
            "owner_address2": ["VARCHAR(64)"],
            "owner_phone": ["VARCHAR(16)"],
            "manager_name": ["VARCHAR(64)"],
            "manager_address": ["VARCHAR(128)"],
            "manager_address2": ["VARCHAR(64)"],
            "manager_phone": ["VARCHAR(16)"],
            "location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "location_surveyed": ["BOOLEAN"],
            "elevation": ["FLOAT"],
            "elevation_surveyed": ["BOOLEAN"],
            "magnetic_variation": ["FLOAT"],
            "magnetic_epoch_year": ["INTEGER"],
            "tpa": ["INTEGER"],
            "sectional": ["VARCHAR(32)"],
            "associated_city_distance": ["INTEGER"],
            "associated_city_direction": ["VARCHAR(4)"],
            "land_covered": ["FLOAT"],
            "boundary_artcc_id": ["VARCHAR(4)"],
            "boundary_artcc_computer_id": ["VARCHAR(4)"],
            "boundary_artcc_name": ["VARCHAR(32)"],
            "responsible_artcc_id": ["VARCHAR(4)"],
            "responsible_artcc_computer_id": ["VARCHAR(4)"],
            "responsible_artcc_name": ["VARCHAR(32)"],
            "fss_on_site": ["BOOLEAN"],
            "fss_id": ["VARCHAR(16)"],
            "fss_name": ["VARCHAR(32)"],
            "fss_admin_phone": ["VARCHAR(16)"],
            "fss_pilot_phone": ["VARCHAR(16)"],
            "alt_fss_id": ["VARCHAR(16)"],
            "alt_fss_name": ["VARCHAR(32)"],
            "alt_fss_pilot_phone": ["VARCHAR(16)"],
            "notam_facility_id": ["VARCHAR(32)"],
            "notam_d_avail": ["BOOLEAN"],
            "activation_date": ["INTEGER"],
            "status_code": ["VARCHAR(4)"],
            "arff_certification_type": ["VARCHAR(32)"],
            "agreements_code": ["VARCHAR(16)"],
            "airspace_analysis_det": ["VARCHAR(16)"],
            "entry_for_customs": ["BOOLEAN"],
            "landing_rights": ["BOOLEAN"],
            "mil_civ_joint_use": ["BOOLEAN"],
            "mil_landing_rights": ["BOOLEAN"],
            "inspection_method": ["VARCHAR(4)"],
            "inspection_agency": ["VARCHAR(4)"],
            "inspection_date": ["INTEGER"],
            "information_request_date": ["VARCHAR(4)"],
            "fuel_types_avail": ["VARCHAR(64)"],
            "airframe_repair_avail": ["VARCHAR(16)"],
            "powerplant_repair_avail": ["VARCHAR(16)"],
            "oxygen_avail": ["VARCHAR(16)"],
            "bulk_oxygen_avail": ["VARCHAR(16)"],
            "lighting_schedule": ["VARCHAR(16)"],
            "beacon_schedule": ["VARCHAR(16)"],
            "tower_onsite": ["BOOLEAN"],
            "unicom_freq": ["FLOAT"],
            "ctaf_freq": ["FLOAT"],
            "segmented_circle": ["VARCHAR(4)"],
            "beacon_lens_color": ["VARCHAR(4)"],
            "non_commerical_ldg_fee": ["BOOLEAN"],
            "medical_use": ["BOOLEAN"],
            "num_se_aircraft": ["INTEGER"],
            "num_me_aircraft": ["INTEGER"],
            "num_jet_aircraft": ["INTEGER"],
            "num_helicopters": ["INTEGER"],
            "num_gliders": ["INTEGER"],
            "num_mil_aircraft": ["INTEGER"],
            "num_ultralight": ["INTEGER"],
            "ops_commerical": ["INTEGER"],
            "ops_commuter": ["INTEGER"],
            "ops_air_taxi": ["INTEGER"],
            "ops_general_local": ["INTEGER"],
            "ops_general_iternant": ["INTEGER"],
            "ops_military": ["INTEGER"],
            "operations_date": ["INTEGER"],
            "position_source": ["VARCHAR(16)"],
            "position_date": ["INTEGER"],
            "elevation_source": ["VARCHAR(16)"],
            "elevation_date": ["INTEGER"],
            "contract_fuel_avail": ["BOOLEAN"],
            "transient_storage_facilities": ["VARCHAR(16)"],
            "other_services": ["VARCHAR(128)"],
            "wind_indicator": ["VARCHAR(16)"],
            "icao_identifier": ["VARCHAR(8)"],
            "attendance_schedule": ["VARCHAR(128)"],
        },

        "runways" : {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "airport_id": ["VARCHAR(32)"],
            "name": ["VARCHAR(16)"],
            "length": ["INTEGER"],
            "width": ["INTEGER"],
            "surface_type": ["VARCHAR(16)"],
            "surface_treatment": ["VARCHAR(16)"],
            "pavement_classification": ["VARCHAR(16)"],
            "lights_intensity": ["VARCHAR(8)"],

            "base_id": ["VARCHAR(8)"],
            "base_true_hdg": ["INTEGER"],
            "base_ils_type": ["VARCHAR(16)"],
            "base_rh_traffic": ["BOOLEAN"],
            "base_markings": ["VARCHAR(16)"],
            "base_markings_condition": ["VARCHAR(4)"],
            "base_location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "base_elevation": ["FLOAT"],
            "base_threshold_height": ["INTEGER"],
            "base_glide_angle": ["FLOAT"],
            "base_disp_threshold_location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "base_disp_threshold_elevation": ["FLOAT"],
            "base_disp_threshold_distance": ["FLOAT"],
            "base_touchdown_elevation": ["FLOAT"],
            "base_glideslope_indicators": ["VARCHAR(8)"],
            "base_visual_range_equip": ["VARCHAR(8)"],
            "base_visual_range_avail": ["BOOLEAN"],
            "base_app_lighting": ["VARCHAR(16)"],
            "base_reil_avail": ["BOOLEAN"],
            "base_center_lights_avail": ["BOOLEAN"],
            "base_touchdown_lights_avail": ["BOOLEAN"],
            "base_obstacle_description": ["VARCHAR(16)"],
            "base_obstacle_lighting": ["VARCHAR(4)"],
            "base_obstacle_category": ["VARCHAR(8)"],
            "base_obstacle_slope": ["INTEGER"],
            "base_obstacle_height": ["INTEGER"],
            "base_obstacle_distance": ["INTEGER"],
            "base_obstacle_offset": ["INTEGER"],

            "recip_id": ["VARCHAR(8)"],
            "recip_true_hdg": ["INTEGER"],
            "recip_ils_type": ["VARCHAR(16)"],
            "recip_rh_traffic": ["BOOLEAN"],
            "recip_markings": ["VARCHAR(16)"],
            "recip_markings_condition": ["VARCHAR(4)"],
            "recip_location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "recip_elevation": ["FLOAT"],
            "recip_threshold_height": ["INTEGER"],
            "recip_glide_angle": ["FLOAT"],
            "recip_disp_threshold_location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "recip_disp_threshold_elevation": ["FLOAT"],
            "recip_disp_threshold_distance": ["FLOAT"],
            "recip_touchdown_elevation": ["FLOAT"],
            "recip_glideslope_indicators": ["VARCHAR(8)"],
            "recip_visual_range_equip": ["VARCHAR(8)"],
            "recip_visual_range_avail": ["BOOLEAN"],
            "recip_app_lighting": ["VARCHAR(16)"],
            "recip_reil_avail": ["BOOLEAN"],
            "recip_center_lights_avail": ["BOOLEAN"],
            "recip_touchdown_lights_avail": ["BOOLEAN"],
            "recip_obstacle_description": ["VARCHAR(16)"],
            "recip_obstacle_lighting": ["VARCHAR(4)"],
            "recip_obstacle_category": ["VARCHAR(8)"],
            "recip_obstacle_slope": ["INTEGER"],
            "recip_obstacle_height": ["INTEGER"],
            "recip_obstacle_distance": ["INTEGER"],
            "recip_obstacle_offset": ["INTEGER"],

            "length_source": ["VARCHAR(16)"],
            "length_source_date": ["INTEGER"],
            "weight_cap_single_wheel": ["INTEGER"],
            "weight_cap_dual_wheel": ["INTEGER"],
            "weight_cap_two_dual_wheel": ["INTEGER"],
            "weight_cap_tandem_dual_wheel": ["INTEGER"],

            "base_gradient": ["FLOAT"],
            "base_position_source": ["VARCHAR(16)"],
            "base_position_source_date": ["INTEGER"],
            "base_elevation_source": ["VARCHAR(16)"],
            "base_elevation_source_date": ["INTEGER"],
            "base_disp_threshold_source": ["VARCHAR(16)"],
            "base_disp_threshold_source_date": ["INTEGER"],
            "base_disp_threshold_elevation_source": ["VARCHAR(16)"],
            "base_disp_threshold_elevation_source_date": ["INTEGER"],
            "base_takeoff_run": ["INTEGER"],
            "base_takeoff_distance": ["INTEGER"],
            "base_aclt_stop_distance": ["INTEGER"],
            "base_landing_distance": ["INTEGER"],
            "base_lahso_distance": ["INTEGER"],
            "base_intersecting_runway_id": ["VARCHAR(16)"],
            "base_hold_short_description": ["VARCHAR(64)"],
            "base_lahso_position": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "base_lahso_source": ["VARCHAR(16)"],
            "base_lahso_source_date": ["INTEGER"],

            "recip_gradient": ["FLOAT"],
            "recip_position_source": ["VARCHAR(16)"],
            "recip_position_source_date": ["INTEGER"],
            "recip_elevation_source": ["VARCHAR(16)"],
            "recip_elevation_source_date": ["INTEGER"],
            "recip_disp_threshold_source": ["VARCHAR(16)"],
            "recip_disp_threshold_source_date": ["INTEGER"],
            "recip_disp_threshold_elevation_source": ["VARCHAR(16)"],
            "recip_disp_threshold_elevation_source_date": ["INTEGER"],
            "recip_takeoff_run": ["INTEGER"],
            "recip_takeoff_distance": ["INTEGER"],
            "recip_aclt_stop_distance": ["INTEGER"],
            "recip_landing_distance": ["INTEGER"],
            "recip_lahso_distance": ["INTEGER"],
            "recip_intersecting_runway_id": ["VARCHAR(16)"],
            "recip_hold_short_description": ["VARCHAR(64)"],
            "recip_lahso_position": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "recip_lahso_source": ["VARCHAR(16)"],
            "recip_lahso_source_date": ["INTEGER"],
        },
        "awos" : {
            "id": ["VARCHAR(64)", "PRIMARY KEY", "UNIQUE"],
            "type": ["VARCHAR(16)"],
            "commissioning": ["BOOLEAN"],
            "commissioning_date": ["INTEGER"],
            "associated_with_naviad": ["BOOLEAN"],
            "location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "elevation": ["FLOAT"],
            "surveyed": ["BOOLEAN"],
            "frequency": ["FLOAT"],
            "frequency2": ["FLOAT"],
            "phone_number": ["VARCHAR(16)"],
            "phone_number2": ["VARCHAR(16)"],
            "associated_facility": ["VARCHAR(16)"],
            "city": ["VARCHAR(64)"],
            "state": ["VARCHAR(4)"],
            "effective_date": ["INTEGER"],
        },
        "tpp" : {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "airport_id": ["VARCHAR(32)"],
            "chart_code": ["VARCHAR(32)"],
            "chart_name": ["VARCHAR(64)"],
            "filename": ["VARCHAR(64)"],
            "url": ["VARCHAR(256)"]
        },
        "airspaces": {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "name": ["VARCHAR(128)"],
            "airspace": ["VARCHAR(128)"],
            "low_alt": ["VARCHAR(16)"],
            "high_alt": ["VARCHAR(16)"],
            "type": ["VARCHAR(16)"],
            "geometry": {"SRS":4326, "type": "POLYGON", "point_type": "XY"}
        },
        "waypoints":{
            "id": ["VARCHAR(64)", "PRIMARY KEY", "UNIQUE"],
            "state_name": ["VARCHAR(16)"],
            "region_code": ["VARCHAR(64)"],
            "location": {"SRS": 4326, "type": "POINT", "point_type": "XY"},
            "previous_name": ["VARCHAR(64)"],
            "charting_info": ["VARCHAR(64)"],
            "to_be_published": ["BOOLEAN"],
            "fix_use": ["VARCHAR(32)"],
            "nas_identifier": ["VARCHAR(16)"],
            "high_artcc": ["VARCHAR(16)"],
            "low_artcc": ["VARCHAR(16)"],
            "country_name": ["VARCHAR(16)"],
            "sua_atcaa": ["BOOLEAN"],
            "remark": ["VARCHAR(128)"],
            "depicted_chart": ["VARCHAR(32)"],
            "mls_component": ["VARCHAR(32)"],
            "radar_component": ["VARCHAR(32)"],
            "pitch": ["BOOLEAN"],
            "catch": ["BOOLEAN"],
            "type": ["VARCHAR(8)"],
        },
        "updates": {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "product": ["VARCHAR(32)"],
            "cycle": ["VARCHAR(32)"],
            "updated": ["DATETIME", "DEFAULT", "CURRENT_TIMESTAMP"],
        },
        "charts": {
            "id": ["INTEGER", "PRIMARY KEY", "UNIQUE"],
            "chart_type": ["VARCHAR(32)"],
            "chart_name": ["VARCHAR(32)"],
            "current_date": ["INTEGER"],
            "current_number": ["INTEGER"],
            "current_url": ["VARCHAR(256)"],
            "next_date": ["INTEGER"],
            "next_number": ["INTEGER"],
            "next_url": ["VARCHAR(256)"],
            "to_download": ["BOOLEAN"]
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
        geometry = []

        if(table in self.TABLES):
            c = self.dbConn.cursor()
            # Drop existing table and recreate it
            query = "DROP TABLE IF EXISTS {0}".format(table)
            c.execute(query)

            query = "CREATE TABLE {0}(".format(table)
            for column in self.TABLES[table]:
                if(isinstance(self.TABLES[table][column], dict) is True):
                    geometry.append({column: self.TABLES[table][column]})
                else:
                    query += "{0} {1}, ".format(column, " ".join(self.TABLES[table][column]))
            query = query[:-2] + ")"
            c.execute(query)

            for column in geometry:
                columnName = column.keys()[0]
                columnAttr = column[columnName]
                query = "SELECT AddGeometryColumn('{0}', '{1}', {2}, '{3}', '{4}');".format(
                    table, columnName, columnAttr["SRS"], columnAttr["type"], columnAttr["point_type"])
                c.execute(query)

            c.close()
            self.dbConn.commit()
            success = True

        return success

    def reset_tables(self, tables):
        for table in tables:
            self.reset_table(table)

    # TODO: This is a touch shitty because we could reset the table, but not touch the corresponding
    # rows in the 'updates' table.  For, if we reset the 'airports' table it should be smart enough
    # to reset the row in 'updates' for whatever source(s) populate it.
    def verify_tables(self, fix=False):
        c = self.dbConn.cursor()

        # Make sure the spatialite tables are still valid
        c.execute("SELECT InitSpatialMetadata()")

        for table in self.TABLES:
            table_valid = True

            query = "PRAGMA table_info({0});".format(table)
            c.execute(query)

            result = c.fetchall()
            if(result is not None):
                # 0 : cid
                # 1 : name
                # 2 : type
                # 3 : notnull
                # 4 : default value
                # 5 : primary key

                actual_table_columns = {}
                for column in result:
                    actual_table_columns[column[1]] = column[2]

                for column in self.TABLES[table]:
                    # Check that the column 
                    if(column not in actual_table_columns):
                        print "WARN: Column '{0}' is missing!".format(column)
                        table_valid = False
                        break

                    # Special handling for geometry columns.
                    if(isinstance(self.TABLES[table][column], dict) is True):
                        if(self.TABLES[table][column]["type"] != actual_table_columns[column]):
                            print "WARN: Column '{0}' has mismatched type - " \
                                "expecting '{1}' and but actually '{2}'!".format(
                                column,
                                self.TABLES[table][column]["type"],
                                actual_table_columns[column])
                            table_valid = False
                            break

                    # Check that the type is the same. This is a bit hackish because we have to
                    # assume that the type is the first item in our definition. Fix.... later.
                    elif(self.TABLES[table][column][0] != actual_table_columns[column]):
                        print "WARN: Column '{0}' has mismatched type - " \
                              "expecting '{1}' and but actually '{2}'!".format(
                                column, self.TABLES[table][column][0], actual_table_columns[column])
                        table_valid = False
                        break

            # The actual DB didn't even have the table we wanted!
            else:
                print "WARN: Table '{0}' not found!".format(table)
                table_valid = False

            if(table_valid is False):
                if(fix is False):
                    print "ERROR: DB needs touchup for table '{0}'!".format(table)
                else:
                    print "WARN: Fixing table '{0}' by resetting it!".format(table)
                    self.reset_table(table)


    def insert_into_db_table_airports(self, airport):
        c = self.dbConn.cursor()
        columns = ""
        values = ""

        latitude = airport.pop("latitude")
        longitude = airport.pop("longitude")
        geometry = "GeomFromText('POINT({0} {1})', 4326)".format(longitude, latitude)

        for item in airport:
            if(item not in self.TABLES["airports"]):
                "Unknown item '{0}' when inserting row into airports table".format(item)

        query = "INSERT INTO airports ({0}, location)" \
                "VALUES ({1}, {2})".format(", ".join(airport.keys()),
                                           ", ".join("?"*len(airport)),
                                           geometry)
        c.execute(query, airport.values())
        c.close()

    def insert_into_db_table_runways(self, runway):
        c = self.dbConn.cursor()
        columns = ""
        values = ""

        base_latitude = runway.pop("base_latitude")
        base_longitude = runway.pop("base_longitude")
        base_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            base_longitude, base_latitude)

        recip_latitude = runway.pop("recip_latitude")
        recip_longitude = runway.pop("recip_longitude")
        recip_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            recip_longitude, recip_latitude)

        base_disp_threshold_latitude = runway.pop("base_disp_threshold_latitude")
        base_disp_threshold_longitude = runway.pop("base_disp_threshold_longitude")
        base_disp_threshold_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            base_disp_threshold_longitude, base_disp_threshold_latitude)

        recip_disp_threshold_latitude = runway.pop("recip_disp_threshold_latitude")
        recip_disp_threshold_longitude = runway.pop("recip_disp_threshold_longitude")
        recip_disp_threshold_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            recip_disp_threshold_longitude, recip_disp_threshold_latitude)

        base_lahso_position_latitude = runway.pop("base_lahso_position_latitude")
        base_lahso_position_longitude = runway.pop("base_lahso_position_longitude")
        base_lahso_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            base_lahso_position_longitude, base_lahso_position_latitude)

        recip_lahso_position_latitude = runway.pop("recip_lahso_position_latitude")
        recip_lahso_position_longitude = runway.pop("recip_lahso_position_longitude")
        recip_lahso_location = "GeomFromText('POINT({0} {1})', 4326)".format(
            recip_lahso_position_longitude, recip_lahso_position_latitude)

        geometry = [base_location, recip_location, base_disp_threshold_location, 
                    recip_disp_threshold_location, base_lahso_location, recip_lahso_location]

        for item in runway:
            if(item not in self.TABLES["runways"]):
                "Unknown item '{0}' when inserting row into runways table".format(item)

        query = "INSERT INTO runways ({0}, base_location, recip_location, " \
                "base_disp_threshold_location, recip_disp_threshold_location, " \
                "base_lahso_position, recip_lahso_position) VALUES ({1}, {2})".format(
                    ", ".join(runway.keys()), ", ".join("?"*len(runway)), ", ".join(geometry))

        c.execute(query, runway.values())
        c.close()


    def insert_into_db_table_waypoints(self, waypoint):
        c = self.dbConn.cursor()
        columns = ""
        values = ""

        latitude = waypoint.pop("latitude")
        longitude = waypoint.pop("longitude")
        location = "GeomFromText('POINT({0} {1})', 4326)".format(longitude, latitude)

        for item in waypoint:
            if(item not in self.TABLES["waypoints"]):
                "Unknown item '{0}' when inserting row into waypoints table".format(item)

        query = "INSERT INTO waypoints ({0}, location) VALUES ({1}, {2})".format(
            ", ".join(waypoint.keys()), ", ".join("?"*len(waypoint)), location)

        try:
            c.execute(query, waypoint.values())
        except:
            print "Failed to add row to 'waypoints':"
            print waypoint.values()
        c.close()

    def insert_into_db_table_awos(self, awos):
        c = self.dbConn.cursor()
        columns = ""
        values = ""

        latitude = awos.pop("latitude")
        longitude = awos.pop("longitude")
        location = "GeomFromText('POINT({0} {1})', 4326)".format(longitude, latitude)

        for item in awos:
            if(item not in self.TABLES["awos"]):
                "Unknown item '{0}' when inserting row into awos table".format(item)

        query = "INSERT INTO awos ({0}, location) VALUES ({1}, {2})".format(
            ", ".join(awos.keys()), ", ".join("?"*len(awos)), location)

        try:
            c.execute(query, awos.values())
        except:
            print "Failed to add row to 'awos':"
            print awos.values()
        c.close()


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

    def insert_processed_airspace_shapefile(self, feature):
        c = self.dbConn.cursor()
        query = """
                INSERT INTO airspaces (name, airspace, low_alt, high_alt, type, geometry)
                VALUES (?, ?, ?, ?, ?, GeomFromText(?, 4326))
                """

        c.execute(query, (feature["name"],
                          feature["airspace"],
                          feature["low_alt"],
                          feature["high_alt"],
                          feature["type"],
                          feature["geometry"]))
        c.close()

    def upsert_into_db_table_charts(self, chart):
        c = self.dbConn.cursor()
        columns = ""
        values = ""

        query = "SELECT id FROM charts WHERE chart_type = ? AND chart_name = ?;"
        c.execute(query, (chart["chart_type"], chart["chart_name"]))
        result = c.fetchone()

        if(result is None):
            for item in chart:
                if(item not in self.TABLES["charts"]):
                    "Unknown item '{0}' when inserting row into charts table".format(item)

            query = "INSERT INTO charts ({0}) VALUES ({1})".format(", ".join(chart.keys()),
                                                                     ", ".join("?"*len(chart)))
            c.execute(query, chart.values())
        else:
            query = """
                    UPDATE charts
                    SET current_number = ?, current_date = ?, current_url = ?,
                    next_number = ?, next_date = ?, next_url = ?
                    WHERE id = ?
                    """
            c.execute(query, (chart["current_number"], chart["current_date"], chart["current_url"],
                              chart["next_number"], chart["next_date"], chart["next_url"],
                              result[0]))
        c.close()

    def get_chart_list_to_be_downloaded(self):
        charts = []
        c = self.dbConn.cursor()

        query = """
                SELECT chart_name, chart_type, current_date, current_number, current_url,
                next_date, next_number, next_url
                FROM charts
                WHERE to_download = 1
                """

        c.execute(query)
        result = c.fetchall()

        if(result is not None):
            for row in result:
                charts.append({
                    "chart_name": row[0],
                    "chart_type": row[1],
                    "current_date": row[2],
                    "current_number": row[3],
                    "current_url": row[4],
                    "next_date": row[5],
                    "next_number": row[6],
                    "next_url": row[7]
                })

        c.close()
        return charts



    def fetch_airport_id_for_designator(self, designator):
        airport_id = None

        c = self.dbConn.cursor()
        query = "SELECT id FROM airports WHERE location_identifier = ?;"
        c.execute(query, (designator,))

        result = c.fetchone()
        if(result is not None):
            airport_id = result[0]

        c.close()
        return airport_id

    def set_table_updated_cycle(self, product, cycle):
        c = self.dbConn.cursor()
        query = "SELECT id FROM updates WHERE product = ?"
        c.execute(query, (product,))

        result = c.fetchone()
        if(result is not None):
            product_id = result[0]
            query = "UPDATE updates " \
                    "SET cycle = ?, updated = (datetime('now','localtime')) " \
                    "WHERE id = ?"
            c.execute(query, (cycle, product_id))
        else:
            query = "INSERT INTO updates (product, cycle, updated) " \
                    "VALUES (?, ?, (datetime('now','localtime')))"
            c.execute(query, (product, cycle))

        c.close()
        return True

    def get_product_updated_cycle(self, product):
        cycle = None

        c = self.dbConn.cursor()
        query = "SELECT cycle FROM updates WHERE product = ?"
        c.execute(query, (product,))

        result = c.fetchone()
        if(result is not None):
            cycle = result[0]

        c.close()
        return cycle

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

class FAA_NASR_Data():
    URL_CYCLES_LIST = "https://enasr.faa.gov/eNASR/nasr/ValueList/Cycle"
    URL_NASR_SUB = "https://nfdc.faa.gov/webContent/56DaySub/{0}/{1}"  # Cycle, product filename

    NASR_PRODUCT_AIXM = "aixm5.1.zip"
    NASR_PRODUCT_AIRSPACE_SHAPES = "class_airspace_shape_files.zip"
    NASR_PRODUCTS_TXT = ["TWR", "FIX", "APT", "AWOS"]

    NASR_CYCLE_START = datetime.datetime(2016, 7, 21)
    NASR_CYCLE_DURATION = datetime.timedelta(56)  # 56-days

    CHART_TYPES = ["sectional", "terminalArea", "world", "helicopter"]

    URL_DTPP_LIST = "https://nfdc.faa.gov/webContent/dtpp/current.xml"
    URL_PROCEDURES = "http://aeronav.faa.gov/d-tpp/{0}/{1}"  # Cycle, chart name

    def __init__(self, cache_dir):
        self.cycles = {}
        self.procedures_cycle = None

        self.cache_dir = cache_dir
        self.filepath_apt_xml = None
        self.filepath_awos_xml = None
        self.filepath_awy_xml = None
        self.filepath_nav_xml = None
        self.filepath_dtpp_xml = None
        self.filepath_airspace_shapefiles = []
        self.filepath_legacy = {}

        # Ensure the cache directory exists.
        if(os.path.exists(self.cache_dir) is False):
            os.makedirs(self.cache_dir)

    def download_nasr_aixm(self, cycle, target_path):
        if(cycle in self.cycles):
            url = self.URL_NASR_SUB.format(self.cycles[cycle], NASR_PRODUCT_AIXM)
        else:
            url = self.URL_NASR_SUB.format(self.cycles["current"], NASR_PRODUCT_AIXM)
        self.download_with_progress(url, target_path)

    def download_dtpp_list(self, target_path):
        self.download_with_progress(self.URL_DTPP_LIST, target_path)

    def download_nasr_legacy(self, product, target_path):
        if product in self.NASR_PRODUCTS_TXT:
            url = self.URL_NASR_SUB.format(self.cycles["current"], product + ".zip")
            print url
            self.download_with_progress(url, target_path)

    def download_nasr_airspace_shapes(self, target_path):
        url = self.URL_NASR_SUB.format(self.cycles["current"], self.NASR_PRODUCT_AIRSPACE_SHAPES)
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

    def update_dtpp_cycle(self, force=False):
        # Attempt to do some caching.
        if((self.procedures_cycle is None) or (force is True)):
            # We don't have a nice way of knowing what the latest DTPP cycle is, rather just a URL
            # with the latest XML as a generic filename.  Request the first few bytes of it and
            # we'll parse out what the latest cycle number is.
            r = requests.get(self.URL_DTPP_LIST, headers={"Range": "bytes=0-200"})
            if(r.status_code == requests.codes.partial_content):
                m = re.search("(?<=cycle=\")[0-9]{4}", r.text)
                if(m is not None):
                    self.procedures_cycle = int(m.group(0))

        return self.procedures_cycle

    def update_aixm_cycles(self, force=False):
        # Attempt to do some caching.
        if((self.cycles == {}) or (force is True)):
            # Calculate the NASR cycles using a known date, and then keep adding the cycle duration.
            current_date = datetime.datetime.now()
            current_cycle = self.NASR_CYCLE_START
            while(current_cycle + self.NASR_CYCLE_DURATION < current_date):
                current_cycle += self.NASR_CYCLE_DURATION

            self.cycles["current"] = current_cycle.strftime("%Y-%m-%d")
            self.cycles["next"] = (current_cycle + self.NASR_CYCLE_DURATION).strftime("%Y-%m-%d")

        return self.cycles

    def update_aixm(self):
        updatePerformed = False

        self.update_aixm_cycles()

        basename = "aixm_" + self.cycles["current"]
        downloadFullpath = os.path.join(self.cache_dir, basename + ".zip")
        print "Updating NASR AIXM Data"
        if(os.path.exists(downloadFullpath) is True):
            print " => Already have current subscription ({0})".format(self.cycles["current"])
        else:
            updatePerformed = True

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
                outpath = main_zf.extract(file, self.cache_dir)
                sub_zf = zipfile.ZipFile(outpath)
                # HACKY HACK HACK
                childname = os.path.basename(outpath).replace("zip", "xml")
                filename = sub_zf.extract(childname, self.cache_dir)

                os.remove(outpath)
                extractedFilepaths.append(filename)

            # Clean up after ourselves
            shutil.rmtree(os.path.join(self.cache_dir, "AIXM_5.1"))

        # TODO: Find a less hacky way of doing this
        self.filepath_apt_xml = os.path.join(self.cache_dir, "APT_AIXM.xml")
        self.filepath_awos_xml = os.path.join(self.cache_dir, "AWOS_AIXM.xml")
        self.filepath_awy_xml = os.path.join(self.cache_dir, "AWY_AIXM.xml")
        self.filepath_nav_xml = os.path.join(self.cache_dir, "AWOS_NAV.xml")

        return updatePerformed

    def update_dtpp(self):
        updatePerformed = False

        self.update_dtpp_cycle()
        # TODO: Be smarter about when we update the XML
        print "Updating NASR DTPP Data"
        target_path = os.path.join(self.cache_dir, "dtpp_{0}.xml".format(self.procedures_cycle))
        if(os.path.exists(target_path) is False):
            updatePerformed = True

            print " => Downloading current DTPP XML ({0})".format(self.procedures_cycle)
            self.download_dtpp_list(target_path)
        else:
            print " => Already up to date! ({0})".format(self.procedures_cycle)

        # TODO: Find less hacky way of doing this
        self.filepath_dtpp_xml = target_path
        return updatePerformed

    def update_airspace_shapefiles(self):
        self.update_aixm_cycles()

        print "Updating NASR Airspace Shapefiles"
        target_path = os.path.join(self.cache_dir,
                                   "airspace_shapefiles_{0}.zip".format(self.cycles["current"]))
        if(os.path.exists(target_path) is True):
            print " => Already up to date! ({0})".format(self.cycles["current"])
        else:
            print " => Downloading current Airspace Shapefiles ({0})".format(self.cycles["current"])
            self.download_nasr_airspace_shapes(target_path)

        zf = zipfile.ZipFile(target_path)
        zf.extractall(self.cache_dir)

        # HACKY HACK HACK
        self.filepath_airspace_shapefiles = []
        for file in os.listdir(os.path.join(self.cache_dir, "Shape_Files")):
            if ".shp" in file:
                self.filepath_airspace_shapefiles.append(
                    os.path.join(self.cache_dir, "Shape_Files", file))

    def update_legacy_products(self):
        self.filepath_legacy = {}

        self.update_aixm_cycles()
        
        print "Updating NASR Legacy Products"
        for product in self.NASR_PRODUCTS_TXT:
            target_path = os.path.join(self.cache_dir, "{0}_{1}.zip".format(self.cycles["current"],
                                                                            product))
            if(os.path.exists(target_path) is True):
                print " => Already downloaded {0}".format(product)
            else:
                print " => Downloading {0}".format(product)
                self.download_nasr_legacy(product, target_path)

            zf = zipfile.ZipFile(target_path)
            filepath = zf.extract(product + ".txt", self.cache_dir)
            # HACKY HACKY HACKKKYYYYY
            self.filepath_legacy[product] = filepath


    def get_current_cycle(self):
        # Since update_aixm_cycles is caching, make sure we have a valid cycle.
        self.update_aixm_cycles()
        return self.cycles["current"]

    def get_filepath_apt(self):
        return self.filepath_apt_xml

    def get_filepath_awos(self):
        return self.filepath_awos_xml

    def get_filepath_awy(self):
        return self.filepath_awy_xml

    def get_filepath_nav(self):
        return self.filepath_nav_xml

    def get_filepath_dtpp(self):
        return self.filepath_dtpp_xml

    def get_filepath_airport_shapefiles(self):
        return self.filepath_airspace_shapefiles

    def get_filepath_legacy_products(self, product):
        filepath = None
        if(product in self.NASR_PRODUCTS_TXT):
            filepath = self.filepath_legacy[product]
        return filepath

    def assemble_procedures_url(self, pdf_name):
        return self.URL_PROCEDURES.format(self.procedures_cycle, pdf_name)

    def get_procedures_cycle(self):
        self.update_dtpp_cycle()
        return self.procedures_cycle

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
        

    def flush_configuration_file(self):
        with open(self.chart_config_file, "w") as fHandle:
            json.dump(self.config, fHandle, indent=4, sort_keys=True)

    def get_latest_chart_info(self):
        available_charts = {}

        page = requests.get(self.URL_VFR_RASTER_CHARTS)
        tree = html.fromstring(page.content)

        for chart in self.CHART_TYPES:
            print " => Updating {0} charts.".format(chart)
            available_charts[chart] = self.scrape_html_table(tree, chart)

        return available_charts

    def extract_edition_info_from_cells(self, cell):
        edition = cell.text_content().encode("ascii", errors="ignore")
        if(("discontinued" in edition.lower()) | ("tbd" in edition.lower())):
            edition_number = None
            epoch_seconds = None
            edition_link = None
        else:
            edition = edition.split("  ")
            edition_number = edition[0]
            edition_date = edition[1].strip()
            edition_link = None

            if("geo" in edition_date.lower()):
                pos = edition_date.lower().find('geo')
                edition_date = edition_date[0:pos]

            if (("zip" in edition_date.lower())):
                pos = edition_date.find(" (")
                edition_date = edition_date[0:pos]

            edition_date = edition_date.replace(" *", "")

            edition_date = datetime.datetime.strptime(edition_date, "%b %d %Y")
            epoch_delta = edition_date - datetime.datetime(1970, 1, 1)
            epoch_seconds = int(epoch_delta.total_seconds())

            for element, attribute, link, pos in cell.iterlinks():
                edition_link = link.encode("ascii", errors="ignore")
                break

        values = {
                    "number": edition_number,
                    "date": epoch_seconds,
                    "url": edition_link
                }
        return values

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

            charts[chart_name] = {
                "current_edition": self.extract_edition_info_from_cells(cell_current_edition),
                "next_edition": self.extract_edition_info_from_cells(cell_next_edition)
            }
            
        return charts

    def fetch_charts(self, requested_charts, force_download=False):
        localCharts = {}

        # Iterate through the desired charts.  Make sure they are real as compared to our index.
        for chart in requested_charts:
            currentNumber = chart["current_number"]

            expectedFilename = "{0}_{1}_{2}.tif".format(
                self.chart_filename_escape(chart["chart_name"]),
                self.CHART_TYPES[chart["chart_type"]]["filename_suffix"],
                currentNumber)
            expectedFilepath = os.path.join(self.chart_directory, expectedFilename)

            if((os.path.exists(expectedFilepath) is False) or (force_download is True)):
                print " => Downloading/Extracting {0} chart!".format(chart)
                url = chart["current_url"]
                self.download_chart(url, expectedFilepath)
            else:
                print " => Current {0} chart is already downloaded!".format(chart["chart_name"])

            # Finally, append the charts we have locally to a dict.
            if(chart["chart_type"] not in localCharts):
                localCharts[chart["chart_type"]] = []
            localCharts[chart["chart_type"]].append(expectedFilepath)

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

class XML_Parser():
    def __init__(self, filename):
        self.filename = filename
        self.tag_hooks = []
        self.parsers = {}
        return

    def register(self, parser, callback):
        self.tag_hooks.append((parser(), callback))

    def run(self):
        for hook in self.tag_hooks:
            self.parsers[hook[0].SUPPORTED_TAG] = {
                "action": hook[0].parse,
                "callback": hook[1]
            }

        inhibitClearing = False
        for event, elem in etree.iterparse(self.filename, events=("start", "end")):
            if(elem.tag in self.parsers):
                if(event == "start"):
                    inhibitClearing = True
                else:
                    inhibitClearing = False
                    result = self.parsers[elem.tag]["action"](elem)
                    self.parsers[elem.tag]["callback"](result)

            if(inhibitClearing is False):
                elem.clear()

        return


class Generic_Legacy_Parser(object):
    COORDINATES_REGEX = re.compile("(?P<deg>[0-9]{2,3})(?:\-)(?P<min>[0-9]{2})(?:\-)(?P<sec>[0-9.]{5,9})(?P<dir>[NSEW]{1})")

    def __init__(self, filename):
        self.mapping = {}
        self.record_id_length = 4
        self.all_fields = {}

        self.register_mapping()

        with open(filename, "r") as fHandle:
            self.lines = fHandle.read().split("\n")

        self.parse()

        return

    def register_mapping(self,):
        return

    def get_field(self, line, startPos, endPos):
        return line[startPos:endPos].strip()

    def parse(self):
        self.all_fields = []

        for line in self.lines:
            if(line == ""):
                continue

            record_type = line[0:self.record_id_length]
            if(record_type not in self.mapping):
                print "ERROR: Unknown record type '{0}'".format(record_type)
                continue

            fields = {"_internal_type_": record_type}
            for field in self.mapping[record_type]:
                value = self.get_field(line, field[1], field[2])

                if(len(field) == 4):
                    value = field[3](value)

                fields[field[0]] = value

            self.all_fields.append(fields)


    def run(self):
        return self.all_fields

    def helper_coordinates(self, asciiCoordinate):
        decimal_degrees = None

        m = re.match(self.COORDINATES_REGEX, asciiCoordinate)
        if(m is not None):
            degrees = int(m.group("deg"))
            minutes = int(m.group("min"))
            seconds = float(m.group("sec"))

            direction = 1
            if((m.group("dir") == "S") or (m.group("dir") == "W")):
                direction = -1

            decimal_degrees = (degrees + (minutes / 60.0) + (seconds / 3600.0)) * direction

        return decimal_degrees

    def helper_int(self, asciiValue):
        converted = None
        if(asciiValue != ""):
            # Convert to float first just in case the ASCII representation includes a decimal.
            converted = int(float(asciiValue))

        return converted

    def helper_float(self, asciiValue):
        converted = None
        if(asciiValue != ""):
            converted = float(asciiValue)

        return converted

    def helper_boolean(self, asciiValue):
        converted = False

        if((asciiValue == "T") or (asciiValue == "Y")):
            converted = True

        return converted

    def helper_date(self, asciiDate):
        result = None
        if(asciiDate is not None and asciiDate != ""):
            date = datetime.datetime.strptime(asciiDate, "%m/%d/%Y")
            epochDelta = date - datetime.datetime(1970, 1, 1)
            result = int(epochDelta.total_seconds())
        return result

    def helper_date_mmyyyy(self, asciiDate):
        result = None
        if(asciiDate is not None and asciiDate != ""):
            date = datetime.datetime.strptime(asciiDate, "%m/%Y")
            epochDelta = date - datetime.datetime(1970, 1, 1)
            result = int(epochDelta.total_seconds())
        return result

    def helper_date_mmddyyyy(self, asciiDate):
        result = None
        if(asciiDate is not None and asciiDate != ""):
            date = datetime.datetime.strptime(asciiDate, "%m%d%Y")
            epochDelta = date - datetime.datetime(1970, 1, 1)
            result = int(epochDelta.total_seconds())
        return result

    def helper_location_surveyed(self, surveyed):
        return (surveyed == "S")

class Legacy_FIX_Parser(Generic_Legacy_Parser):
    def register_mapping(self):
        self.record_id_length = 4

        self.mapping = {
            "FIX1": [
                # NOTE! FAA Layout Diagrams are 1-indexed! WTF!?
                # Field                    Start, End Index
                ("id",                       4,  34),
                ("state_name",              34,  64),
                ("region_code",             64,  65),
                ("latitude",                66,  80, self.helper_coordinates),
                ("longitude",               80,  94, self.helper_coordinates),
                ("type",                    94,  97),
                ("mls_component",           97, 119),
                ("radar_component",         119, 141),
                ("previous_name",          141, 174),
                ("charting_info",          174, 212),
                ("to_be_published",        212, 213, self.helper_boolean),
                ("fix_use",                213, 228),
                ("nas_identifier",         228, 233),
                ("high_artcc",             233, 237),
                ("low_artcc",              237, 241),
                ("country_name",           241, 271),
                ("pitch",                  271, 272, self.helper_boolean),
                ("catch",                  272, 273, self.helper_boolean),
                ("sua_atcaa",              273, 274, self.helper_boolean)
            ],
            "FIX2": [],
            #     # Field                     Start, End Index
            #     ("record_identifier",        4,  33),
            #     ("state_name",              34,  63),
            #     ("region_code",             64,  65),
            #     ("navaid_used",             66,  88)
            # ],
            "FIX3": [],
            #     # Field                     Start, End Index
            #     ("record_identifier",        4,  33),
            #     ("state_name",              34,  63),
            #     ("region_code",             64,  65),
            #     ("ils_component",           66,  88)
            # ],
            "FIX4": [],
            #     # Field                     Start, End Index
            #     ("record_identifier",        4,  33),
            #     ("state_name",              34,  63),
            #     ("region_code",             64,  65),
            #     ("field_label",             66, 165),
            #     ("remark",                 166,  -1),
            # ],
            "FIX5": []
            #     # Field                     Start, End Index
            #     ("record_identifier",        4,  33),
            #     ("state_name",              34,  63),
            #     ("region_code",             64,  65),
            #     ("depicted_chart",          66,  87),
            # ]
        }


class Legacy_APT_Parser(Generic_Legacy_Parser):
    MAGVAR_REGEX = re.compile("(?P<var>[0-9]{1,2})(?P<dir>[EW])")
    OBSTACLE_OFFSET_REGEX = re.compile("(?P<dist>[0-9]{1,6})(?P<dir>[LRB])?")
    OBSTACLE_DIST_REGEX = re.compile("(?P<dist>[0-9]{1,6})")
    GRADIENT_REGEX = re.compile("(?P<gradient>[0-9.]{1,6})(?P<direction>[A-Z]{2,4})?")

    def register_mapping(self):
        self.record_id_length = 3

        self.mapping = {
            "APT": [
                # NOTE! FAA Layout Diagrams are 1-indexed! WTF!?
                # Field                    Start, End Index
                ("id", 3, 13),
                ("landing_facility_type", 14, 27),
                ("location_identifier", 27, 31),
                ("effective_date", 31, 41, self.helper_date),
                ("region_code", 41, 44),
                ("faa_district_code", 44, 48),
                ("state_code", 48, 50),
                ("state_name", 50, 70),
                ("county_name", 70, 91),
                ("county_state", 91, 93),
                ("city_name", 93, 133),
                ("facility_name", 133, 183),
                ("ownership_type", 183, 185),
                ("facility_use", 185, 187),
                ("owner_name", 187, 222),
                ("owner_address", 222, 294),
                ("owner_address2", 294, 339),
                ("owner_phone", 339, 355),
                ("manager_name", 355, 390),
                ("manager_address", 390, 462),
                ("manager_address2", 462, 507),
                ("manager_phone", 507, 523),
                ("latitude", 523, 538, self.helper_coordinates),
                ("longitude", 550, 565, self.helper_coordinates),
                ("location_surveyed", 577, 578, self.helper_location_surveyed),
                ("elevation", 578, 585),
                ("elevation_surveyed", 585, 586, self.helper_location_surveyed),
                ("magnetic_variation", 586, 589, self.helper_magnetic_variation),
                ("magnetic_epoch_year", 589, 593, self.helper_int),
                ("tpa", 593, 597, self.helper_int),
                ("sectional", 597, 627),
                ("associated_city_distance", 627, 629, self.helper_int),
                ("associated_city_direction", 629, 632),
                ("land_covered", 632, 637),
                ("boundary_artcc_id", 637, 641),
                ("boundary_artcc_computer_id", 641, 644),
                ("boundary_artcc_name", 644, 674),
                ("responsible_artcc_id", 674, 678),
                ("responsible_artcc_computer_id", 678, 681),
                ("responsible_artcc_name", 681, 711),
                ("fss_on_site", 711, 712, self.helper_boolean),
                ("fss_id", 712, 716),
                ("fss_name", 716, 746),
                ("fss_admin_phone", 746, 762),
                ("fss_pilot_phone", 762, 778),
                ("alt_fss_id", 778, 782),
                ("alt_fss_name", 782, 812),
                ("alt_fss_pilot_phone", 812, 828),
                ("notam_facility_id", 828, 832),
                ("notam_d_avail", 832, 833, self.helper_boolean),
                ("activation_date", 833, 840, self.helper_date_mmyyyy),
                ("status_code", 840, 842),
                ("arff_certification_type", 842, 857),
                ("agreements_code", 857, 864),
                ("airspace_analysis_det", 864, 877),
                ("entry_for_customs", 877, 878, self.helper_boolean),
                ("landing_rights", 878, 879, self.helper_boolean),
                ("mil_civ_joint_use", 879, 880, self.helper_boolean),
                ("mil_landing_rights", 880, 881, self.helper_boolean),
                ("inspection_method", 881, 883),
                ("inspection_agency", 883, 884),
                ("inspection_date", 884, 892, self.helper_date_mmddyyyy),
                ("information_request_date", 892, 900),
                ("fuel_types_avail", 900, 940),
                ("airframe_repair_avail", 940, 945),
                ("powerplant_repair_avail", 945, 950),
                ("oxygen_avail", 950, 958),
                ("bulk_oxygen_avail", 958, 966),
                ("lighting_schedule", 966, 973),
                ("beacon_schedule", 973, 980),
                ("tower_onsite", 980, 981, self.helper_boolean),
                ("unicom_freq", 981, 988, self.helper_float),
                ("ctaf_freq", 988, 995, self.helper_float),
                ("segmented_circle", 995, 999),
                ("beacon_lens_color", 999, 1002),
                ("non_commerical_ldg_fee", 1002, 1003, self.helper_boolean),
                ("medical_use", 1003, 1004, self.helper_boolean),
                ("num_se_aircraft", 1004, 1007, self.helper_int),
                ("num_me_aircraft", 1007, 1010, self.helper_int),
                ("num_jet_aircraft", 1010, 1013, self.helper_int),
                ("num_helicopters", 1013, 1016, self.helper_int),
                ("num_gliders", 1016, 1019, self.helper_int),
                ("num_mil_aircraft", 1019, 1022, self.helper_int),
                ("num_ultralight", 1022, 1025, self.helper_int),
                ("ops_commerical", 1025, 1031, self.helper_int),
                ("ops_commuter", 1031, 1037, self.helper_int),
                ("ops_air_taxi", 1037, 1043, self.helper_int),
                ("ops_general_local", 1043, 1049, self.helper_int),
                ("ops_general_iternant", 1049, 1055, self.helper_int),
                ("ops_military", 1055, 1061, self.helper_int),
                ("operations_date", 1061, 1071, self.helper_date),
                ("position_source", 1071, 1087),
                ("position_date", 1087, 1097, self.helper_date),
                ("elevation_source", 1097, 1113),
                ("elevation_date", 1113, 1123, self.helper_date),
                ("contract_fuel_avail", 1123, 1124),
                ("transient_storage_facilities", 1124, 1136),
                ("other_services", 1136, 1207),
                ("wind_indicator", 1207, 1210),
                ("icao_identifier", 1210, 1217),
            ],
            "ATT": [
                ("airport_id", 3, 13),
                ("attendance_schedule", 18, 125),
            ],
            "RWY": [
                ("airport_id", 3, 13),
                ("name", 16, 23),
                ("length", 23, 28, self.helper_int),
                ("width", 28, 32, self.helper_int),
                ("surface_type", 32, 44),
                ("surface_treatment", 44, 49),
                ("pavement_classification", 49, 60),
                ("lights_intensity", 60, 65),
                ("base_id", 65, 68),
                ("base_true_hdg", 68, 71, self.helper_int),
                ("base_ils_type", 71, 81),
                ("base_rh_traffic", 81, 82, self.helper_boolean),
                ("base_markings", 82, 87),
                ("base_markings_condition", 87, 88),
                ("base_latitude", 88, 103, self.helper_coordinates),
                ("base_longitude", 115, 130, self.helper_coordinates),
                ("base_elevation", 142, 149, self.helper_float),
                ("base_threshold_height", 149, 152, self.helper_int),
                ("base_glide_angle", 152, 156, self.helper_float),
                ("base_disp_threshold_latitude", 156, 171, self.helper_coordinates),
                ("base_disp_threshold_longitude", 183, 198, self.helper_coordinates),
                ("base_disp_threshold_elevation", 210, 217, self.helper_float),
                ("base_disp_threshold_distance", 217, 221, self.helper_float),
                ("base_touchdown_elevation", 221, 228, self.helper_float),
                ("base_glideslope_indicators", 228, 233),
                ("base_visual_range_equip", 233, 236),
                ("base_visual_range_avail", 236, 237, self.helper_boolean),
                ("base_app_lighting", 237, 245),
                ("base_reil_avail", 245, 246, self.helper_boolean),
                ("base_center_lights_avail", 246, 247, self.helper_boolean),
                ("base_touchdown_lights_avail", 247, 248, self.helper_boolean),
                ("base_obstacle_description", 248, 259),
                ("base_obstacle_lighting", 259, 263),
                ("base_obstacle_category", 263, 268),
                ("base_obstacle_slope", 268, 270, self.helper_int),
                ("base_obstacle_height", 270, 275, self.helper_int),
                ("base_obstacle_distance", 275, 280, self.helper_obstacle_distance),
                ("base_obstacle_offset", 280, 287, self.helper_obstacle_offset),
                ("recip_id", 287, 290),
                ("recip_true_hdg", 290, 293, self.helper_int),
                ("recip_ils_type", 293, 303),
                ("recip_rh_traffic", 303, 304, self.helper_boolean),
                ("recip_markings", 304, 309),
                ("recip_markings_condition", 309, 310),
                ("recip_latitude", 310, 325, self.helper_coordinates),
                ("recip_longitude", 337, 352, self.helper_coordinates),
                ("recip_elevation", 364, 371, self.helper_float),
                ("recip_threshold_height", 371, 374, self.helper_int),
                ("recip_glide_angle", 374, 378, self.helper_float),
                ("recip_disp_threshold_latitude", 378, 393, self.helper_coordinates),
                ("recip_disp_threshold_longitude", 405, 420, self.helper_coordinates),
                ("recip_disp_threshold_elevation", 432, 439, self.helper_float),
                ("recip_disp_threshold_distance", 439, 443, self.helper_float),
                ("recip_touchdown_elevation", 443, 450, self.helper_float),
                ("recip_glideslope_indicators", 450, 455),
                ("recip_visual_range_equip", 455, 458),
                ("recip_visual_range_avail", 458, 459, self.helper_boolean),
                ("recip_app_lighting", 459, 467),
                ("recip_reil_avail", 467, 468, self.helper_boolean),
                ("recip_center_lights_avail", 468, 469, self.helper_boolean),
                ("recip_touchdown_lights_avail", 469, 470, self.helper_boolean),
                ("recip_obstacle_description", 470, 481),
                ("recip_obstacle_lighting", 481, 485),
                ("recip_obstacle_category", 485, 490),
                ("recip_obstacle_slope", 490, 492, self.helper_int),
                ("recip_obstacle_height", 492, 497, self.helper_int),
                ("recip_obstacle_distance", 497, 502, self.helper_obstacle_distance),
                ("recip_obstacle_offset", 502, 509, self.helper_obstacle_offset),
                ("length_source", 509, 525),
                ("length_source_date", 525, 535, self.helper_date),
                ("weight_cap_single_wheel", 535, 541, self.helper_int),
                ("weight_cap_dual_wheel", 541, 547, self.helper_int),
                ("weight_cap_two_dual_wheel", 547, 553, self.helper_int),
                ("weight_cap_tandem_dual_wheel", 553, 559, self.helper_int),
                ("base_gradient", 559, 568, self.helper_gradient),
                ("base_position_source", 568, 584),
                ("base_position_source_date", 584, 594, self.helper_date),
                ("base_elevation_source", 594, 610),
                ("base_elevation_source_date", 610, 620, self.helper_date),
                ("base_disp_threshold_source", 620, 636),
                ("base_disp_threshold_source_date", 636, 646, self.helper_date),
                ("base_disp_threshold_elevation_source", 646, 662),
                ("base_disp_threshold_elevation_source_date", 662, 672, self.helper_date),
                ("base_takeoff_run", 698, 703, self.helper_int),
                ("base_takeoff_distance", 703, 708, self.helper_int),
                ("base_aclt_stop_distance", 708, 713, self.helper_int),
                ("base_landing_distance", 713, 718, self.helper_int),
                ("base_lahso_distance", 718, 723, self.helper_int),
                ("base_intersecting_runway_id", 723, 730),
                ("base_hold_short_description", 730, 770),
                ("base_lahso_position_latitude", 770, 785, self.helper_coordinates),
                ("base_lahso_position_longitude", 797, 812, self.helper_coordinates),
                ("base_lahso_source", 824, 840),
                ("base_lahso_source_date", 840, 850, self.helper_date),

                ("recip_gradient", 850, 859, self.helper_gradient),
                ("recip_position_source", 859, 875),
                ("recip_position_source_date", 875, 885, self.helper_date),
                ("recip_elevation_source", 885, 901),
                ("recip_elevation_source_date", 901, 911, self.helper_date),
                ("recip_disp_threshold_source", 911, 927),
                ("recip_disp_threshold_source_date", 927, 937, self.helper_date),
                ("recip_disp_threshold_elevation_source", 937, 953),
                ("recip_disp_threshold_elevation_source_date", 953, 963, self.helper_date),
                ("recip_takeoff_run", 989, 994, self.helper_int),
                ("recip_takeoff_distance", 994, 999, self.helper_int),
                ("recip_aclt_stop_distance", 999, 1004, self.helper_int),
                ("recip_landing_distance", 1004, 1009, self.helper_int),
                ("recip_lahso_distance", 1009, 1014, self.helper_int),
                ("recip_intersecting_runway_id", 1014, 1021),
                ("recip_hold_short_description", 1021, 1061),
                ("recip_lahso_position_latitude", 1061, 1076, self.helper_coordinates),
                ("recip_lahso_position_longitude", 1088, 1103, self.helper_coordinates),
                ("recip_lahso_source", 1115, 1131),
                ("recip_lahso_source_date", 1131, 1141, self.helper_date),
            ],
            "ARS": [],
            "RMK": [
                ("airport_id", 3, 13),
                ("remark_name", 16, 28),
                ("remark", 29, 1528)
            ]
        }

    def helper_magnetic_variation(self, magvar):
        result = None
        if(magvar is not None and magvar != ""):
            m = re.match(self.MAGVAR_REGEX, magvar)
            if(m is not None):
                degrees = int(m.group("var"))

                direction = 1
                if(m.group("dir") == "W"):
                    direction = -1
                result = direction * degrees

        return result

    def helper_obstacle_offset(self, offset):
        result = None
        if(offset is not None and offset != ""):
            m = re.match(self.OBSTACLE_OFFSET_REGEX, offset)
            if(m is not None):
                distance = int(m.group("dist"))

                direction = 1
                if(m.group("dir") == "L"):
                    direction = -1
                result = direction * distance

        return result

    def helper_obstacle_distance(self, distance):
        result = None
        if(distance is not None and distance != ""):
            m = re.match(self.OBSTACLE_DIST_REGEX, distance)
            if(m is not None):
                result = int(m.group("dist"))

        return result


    def helper_gradient(self, gradient):
        result = None
        if(gradient is not None and gradient != ""):
            m = re.match(self.GRADIENT_REGEX, gradient)
            if(m is not None):
                distance = float(m.group("gradient"))

                direction = 1
                if(m.group("direction") == "DOWN"):
                    direction = -1
                result = direction * distance

        return result


class Legacy_AWOS_Parser(Generic_Legacy_Parser):
    def register_mapping(self):
        self.record_id_length = 5

        self.mapping = {
            "AWOS1": [
                # NOTE! FAA Layout Diagrams are 1-indexed! WTF!?
                # Field                    Start, End Index
                ("id",                       5,   9),
                ("type",                     9,  19),
                ("commissioning",           19,  20, self.helper_boolean),
                ("commissioning_date",      20,  30, self.helper_date),
                ("latitude",                31,  45, self.helper_coordinates),
                ("longitude",               45,  60, self.helper_coordinates),
                ("elevation",               60,  67, self.helper_float),
                ("surveyed",                67,  68, self.helper_location_surveyed),
                ("frequency",               68,  75, self.helper_float),
                ("frequency2",              75,  82, self.helper_float),
                ("phone_number",            82,  96),
                ("phone_number2",           96, 110),
                ("associated_facility",    110, 121),
                ("city",                   121, 161),
                ("state",                  161, 163),
                ("effective_date",         163, 173, self.helper_date),

            ],
            "AWOS2": [],
        }


class Shapefile():
    def __init__(self, filename):
        self.driver = ogr.GetDriverByName("ESRI Shapefile")
        self.dataSource = self.driver.Open(filename, 0)
        self.layer = self.dataSource.GetLayer()
        self.features = []

    def get_features(self):
        # Need to expand any multipolygons into individual polygons.
        for feature in self.layer:
            geometryName = feature.GetGeometryRef().GetGeometryName()
            if(geometryName == "MULTIPOLYGON"):
                parent_name = feature.GetField("NAME")
                parent_airspace = feature.GetField("AIRSPACE")
                parent_low_alt = feature.GetField("LOWALT")
                parent_high_alt = feature.GetField("HIGHALT")

                count = 0
                for part in feature.GetGeometryRef():
                    self.features.append({
                        "name": "{0}-{1}".format(parent_name, count),
                        "airspace": parent_airspace,
                        "low_alt": parent_low_alt,
                        "high_alt": parent_high_alt,
                        "geometry": part.ExportToWkt()
                    })
                    count += 1

            elif(geometryName == "POLYGON"):
                self.features.append({
                    "name": feature.GetField("NAME"),
                    "airspace": feature.GetField("AIRSPACE"),
                    "low_alt": feature.GetField("LOWALT"),
                    "high_alt": feature.GetField("HIGHALT"),
                    "geometry": feature.GetGeometryRef().ExportToWkt()
                })
            else:
                print "Skipping {0} due to unknown geometry type".format(feature.GetField("NAME"))

        return self.features
