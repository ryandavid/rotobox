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
