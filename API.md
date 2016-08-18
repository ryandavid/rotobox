# GPS
## /api/location
```
{
    "status": 1,
    "timestamp": 1468902782.000000,
    "mode": 3,
    "sats_used": 6,
    "latitude": 37.000000,
    "longitude": -120.000000,
    "altitude": 30.627000,
    "track": 154.827200,
    "speed": 0.385000,
    "climb": -0.054000,
    "uncertainty_pos": [
         40.192000,
         40.192000,
         38.504000
     ],
    "uncertainty_speed": 134.550000
}
```

## /api/satellites
Keys are the satellite PRN
```
{
    "8": {
        "snr": 17.000000,
        "used": 0,
        "elevation": 28,
        "azimuth": 310
    },
    "10": {
        "snr": 13.000000,
        "used": 1,
        "elevation": 70,
        "azimuth": 331
    },
    "14": {
        "snr": 20.000000,
        "used": 1,
        "elevation": 32,
        "azimuth": 195
    },
    "21": {
        "snr": 28.000000,
        "used": 1,
        "elevation": 44,
        "azimuth": 113
    },
    "24": {
        "snr": 22.000000,
        "used": 0,
        "elevation": 17,
        "azimuth": 73
    },
    "32": {
        "snr": 21.000000,
        "used": 1,
        "elevation": 50,
        "azimuth": 191
    },
    "81": {
        "snr": 23.000000,
        "used": 1,
        "elevation": 30,
        "azimuth": 158
    },
    "num_satellites": 7,
    "num_satellites_used": 5
}
```

## /api/airports/search_by_window
### latMin, latMax, lonMin, lonMax
```
[
    {
        "id": "AH_0001717",
        "name": "FUNNY FARM",
        "designator": "4CA2",
        "designator": "4CA2",
        "longitude": -121.647453,
        "latitude": 37.946867
    },
    {
        "id": "AH_0001728",
        "name": "BYRON",
        "designator": "C83",
        "designator": "C83",
        "longitude": -121.625833,
        "latitude": 37.828444
    }
]
```

## /api/airports/search_by_id
### id
```
[
    {
        "served_city": "HAYWARD",
        "lighting_schedule": "SEE RMK",
        "magnetic_variation": 15.000000,
        "name": "HAYWARD EXECUTIVE",
        "private_use": 0,
        "sectional_chart": "SAN FRANCISCO",
        "activated": "1947-05-01T00:00:00.000-04:00",
        "beacon_lighting_schedule": "SS-SR",
        "designator": "HWD",
        "traffic_control_tower_on_airport": 1,
        "segmented_circle_marker_on_airport": 1,
        "attendance_schedule": "ALL/ALL/0800-1700",
        "wind_direction_indicator": 0,
        "marker_lens_color": "CG",
        "field_elevation": 52.100000,
        "remarks": "Y-L LIGHTED WIND INDICATOR EXISTS  CITY MANAGER  155 FT ENERGY COMPLEX EXHAUST STACK 1 1/2 NM SW OF ARPT.  DO NOT OVERFLY ENERGY COMPLEX FACILITY BELOW  1,000 FT MSL.  TWY Z1 CLSD TO ACFT WINGSPAN GTR THAN 79FT.  FOR RESTAURANT/GOLD COURSE ITNRNT ACFT PRKG CTC ARPT OPS 510-293-8678  TWY A NOT VSB FM ATCT BTB TWY B & C  FOR CD WHEN ATCT IS CLSD CTC NORCAL APCH AT 916-361-0516.  WHEN ATCT CLSD RWY 10L/28R CLSD.  RY 10R HAS LANDING DISTANCE REMAINING SIGNS (LGTD) NORTH SIDE OF RY.  RY 28L HAS LANDING DISTANCE REMAINING SIGNS (LGTD) SOUTH SIDE OF RY.  FLOCKS OF BIRDS FEEDING ALONG THE SHORELINE, CREEK AREAS AND AT THE GOLF COURSE TO THE NORTH, ON OCCASION MAY FLY ACROSS VARIOUS PARTS OF THE ARPT.  NOISE ABATEMENT PROCEDURES IN EFFECT CTC ARPT FOR NOISE RULES 510-293-8669.  TRANSIENT HELICOPTER TFC USE HELIPADS WEST OF GREEN RAMP LCTD AT BASE OF ATCT.  STRENGTH LIMITED BY STRENGTH OF CONNECTING TAXIWAYS.  APCH RATIO 45:1 FM DSPLCD THR.  WHEN ATCT CLSD MIRL RWY 10R/28L PRESET LOW INTST; TO INCREASE INTST ACTVT - CTAF.  WHEN ATCT CLSD VASI RWY 10R & 28L OPER SS-SR; PAPI RWY 10L & 28R, REIL RWY 10R & 28L UNAVBL.  INCLUDES 4 AMPHIBIANS.  <TPA: 600'AGL EXCEPT RWY 10L-28R 800'AGL.  SE",
        "type": "AH",
        "id": "AH_0001941",
        "control_type": "JOINT",
        "icao_name": "KHWD",
        "longitude": -122.121737,
        "latitude": 37.658929
    }
]
```

## /api/airports/runways
### id
```
[
    {
        "designator": "10L/28R",
        "ils_type": "(null)",
        "airport_id": "AH_0001941",
        "true_bearing": "(null)",
        "id": "RWY_0000001_1941",
        "condition": "GOOD",
        "preparation": "(null)",
        "width": 75.000000,
        "length": 3107.000000,
        "right_traffic_pattern": "(null)",
        "composition": "ASPH"
    },
    {
        "designator": "10L",
        "ils_type": "(null)",
        "airport_id": "AH_0001941",
        "true_bearing": 119.000000,
        "id": "RWY_BASE_END_0000001_1941",
        "condition": "(null)",
        "preparation": "(null)",
        "width": "(null)",
        "length": "(null)",
        "right_traffic_pattern": 0,
        "composition": "(null)"
    },
    {
        "designator": "H1",
        "ils_type": "(null)",
        "airport_id": "AH_0001941",
        "true_bearing": "(null)",
        "id": "RWY_0000003_1941",
        "condition": "FAIR",
        "preparation": "(null)",
        "width": 110.000000,
        "length": 110.000000,
        "right_traffic_pattern": "(null)",
        "composition": "ASPH"
    },
]
```

## /api/airports/radio
### id
```
[
    {
        "channel_name": "BASELINE",
        "tx_frequency": 120.200000,
        "id": "RADIO_COMMUNICATION_CHANNEL_0001941",
        "rx_frequency": "(null)",
        "airport_id": "AH_0001941"
    }
]
```


## /api/airports/nearest
[
    {
        "id": "AH_0002349",
        "name": "NORMAN Y MINETA SAN JOSE INTL",
        "designator": "SJC",
        "distance": 1234.861223
    },
    {
        "id": "AH_0002347",
        "name": "REGIONAL MEDICAL CENTER SAN JOSE H2",
        "designator": "88CA",
        "distance": 2345.796561
    },
    {
        "id": "AH_0002346",
        "name": "COUNTY MEDICAL CENTER",
        "designator": "CA33",
        "distance": 3456.542485
    },
    {
        "id": "AH_0002348",
        "name": "REID-HILLVIEW OF SANTA CLARA COUNTY",
        "designator": "RHV",
        "distance": 4567.065565
    },
    {
        "id": "AH_0002371",
        "name": "SANTA CLARA TOWERS",
        "designator": "CL86",
        "distance": 5678.344991
    }
]