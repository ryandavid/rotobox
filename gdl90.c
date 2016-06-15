#include "gdl90.h"

// Copied from the GDL90 ICD
uint16_t crc16table[256];

void process_gdl90_message() {
    gdl_message_t message;
    gdl90_msg_traffic_report_t traffic_report_msg;
    /*uint8_t rx_traffic_report[30] = { 0x7E, 0x14, 0x00, 0xAB, 0x45, 0x49, 0x1F, 0xEF, 0x15, 0xA8,
                                      0x89, 0x78, 0x0F, 0x09, 0xA9, 0x07, 0xB0, 0x01, 0x20, 0x01,
                                      0x4E, 0x38, 0x32, 0x35, 0x56, 0x20, 0x20, 0x20, 0x00, 0x7E };

    uint8_t rx_heartbeat_msg[11] = {0x7E, 0x00, 0xA1, 0x81, 0xCC, 0x51, 0x00, 0x00, 0x00, 0x00, 0x7E};

    memcpy(&message, &rx_traffic_report[0], sizeof(rx_traffic_report));*/

    message.messageId = MSG_ID_TRAFFIC_REPORT;

    traffic_report_msg.trafficAlertStatus = TRAFFIC_ALERT;
    traffic_report_msg.addressType = ADS_B_WITH_ICAO_ADDRESS;

    traffic_report_msg.address = 0x29CBB8;  // Should end up with octal 12345670
    traffic_report_msg.latitude = 10.90708f;
    traffic_report_msg.longitude = -148.99488f;
    traffic_report_msg.altitude = -500.0f;

    traffic_report_msg.airborne = false;
    traffic_report_msg.reportType = REPORT_UPDATED;
    traffic_report_msg.ttType = TT_TYPE_MAG_HEADING;

    traffic_report_msg.nic = 7;
    traffic_report_msg.nacp = 5;

    traffic_report_msg.horizontalVelocity = 98.0f;
    traffic_report_msg.verticalVelocity = -128.0f;
    traffic_report_msg.trackOrHeading = 123.0f;
    traffic_report_msg.emitterCategory = EMITTER_ROTORCRAFT;
    traffic_report_msg.callsign[0] = 'N';
    traffic_report_msg.callsign[1] = '1';
    traffic_report_msg.callsign[2] = '2';
    traffic_report_msg.callsign[3] = '3';
    traffic_report_msg.callsign[4] = '4';
    traffic_report_msg.callsign[5] = '5';
    traffic_report_msg.callsign[6] = 'W';
    traffic_report_msg.callsign[7] = 'Y';
    traffic_report_msg.emergencyCode = EMERGENCY_MEDICAL;

    encode_gdl90_traffic_report(&message.data[0], &traffic_report_msg);

    switch(message.messageId) {
        case(MSG_ID_TRAFFIC_REPORT):
            fprintf(stdout, "Processing Traffic Report Message!\n");
            uint16_t calc_crc = gdl90_crcCompute(&message.messageId, GDL90_MSG_LEN_TRAFFIC_REPORT);

            fprintf(stdout, "calc_crc = %04X\n", calc_crc);
            decode_gdl90_traffic_report(&message.data[0], &traffic_report_msg);
            print_gdl90_traffic_report(&traffic_report_msg);
            break;

        case(MSG_ID_HEARTBEAT):
            fprintf(stdout, "Processing Heartbeat Message\n");
            break;

        default:
            fprintf(stdout, "Unknown message ID = %d!\n", message.messageId);
    }
}


void print_gdl90_traffic_report(gdl90_msg_traffic_report_t *decodedMsg) {
    // Try and replicate the contents in section 3.5.4 of the GDL90 ICD
    switch(decodedMsg->trafficAlertStatus) {
        case(NO_ALERT):
        fprintf(stdout, "No Traffic Alert\n");
        break;

        case(TRAFFIC_ALERT):
        default:
        fprintf(stdout, "Traffic Alert\n");
        break;
    }

    switch(decodedMsg->addressType){
        case(ADS_B_WITH_ICAO_ADDRESS):
        fprintf(stdout, "ICAO ADS-B ");
        break;

        case(ADS_B_WITH_SELF_ASSIGNED):
        fprintf(stdout, "Self-Assigned ADS-B ");
        break;

        case(TIS_B_WITH_ICAO_ADDRESS):
        fprintf(stdout, "ICAO TIS-B ");
        break;

        case(TIS_B_WITH_TRACK_ID):
        fprintf(stdout, "Track ID TIS-B ");
        break;

        case(SURFACE_VEHICLE):
        fprintf(stdout, "Surface Vehicle ");
        break;

        case(GROUND_STATION_BEACON):
        fprintf(stdout, "Ground Station ");
        break;

        default:
        fprintf(stdout, "Unknown ");
        break;
    }

    fprintf(stdout, "Address (octal): %o\n", decodedMsg->address);
    fprintf(stdout, "Latitude: %f\n", decodedMsg->latitude);
    fprintf(stdout, "Longitude: %f\n", decodedMsg->longitude);
    fprintf(stdout, "Pressure Altitude: %f\n", decodedMsg->altitude);

    if(decodedMsg->airborne == true) {
        fprintf(stdout, "Airborne with ");
    } else {
        fprintf(stdout, "Grounded with ");
    }

    switch(decodedMsg->ttType) {
        case(TT_TYPE_TRUE_TRACK):
        fprintf(stdout, "True Track\n");
        break;

        case(TT_TYPE_MAG_HEADING):
        fprintf(stdout, "Magnetic Heading\n");
        break;

        case(TT_TYPE_TRUE_HEADING):
        fprintf(stdout, "True Heading\n");
        break;

        case(TT_TYPE_INVALID):
        default:
        fprintf(stdout, "Invalid track/heading type\n");
    }

    switch(decodedMsg->reportType) {
        case(REPORT_UPDATED):
        fprintf(stdout, "Report Updated\n");
        break;

        case(REPORT_EXTRAPOLATED):
        fprintf(stdout, "Report Extrapolated\n");
        break;

        default:
        fprintf(stdout, "Report Unknown = %d\n", decodedMsg->reportType);
        break;
    }

    switch(decodedMsg->nic) {
        case(NIC_LESS_20NM):
        fprintf(stderr, "HPL = 20nm, ");
        break;

        case(NIC_LESS_8NM):
        fprintf(stderr, "HPL = 8nm, ");
        break;

        case(NIC_LESS_4NM):
        fprintf(stderr, "HPL = 4nm, ");
        break;
        
        case(NIC_LESS_2NM):
        fprintf(stderr, "HPL = 2nm, ");
        break;
        
        case(NIC_LESS_1NM):
        fprintf(stderr, "HPL = 1nm, ");
        break;
        
        case(NIC_LESS_0_6NM):
        fprintf(stderr, "HPL = 0.6nm, ");
        break;

        case(NIC_LESS_0_2NM):
        fprintf(stderr, "HPL = 0.2nm, ");
        break;

        case(NIC_LESS_0_1NM):
        fprintf(stderr, "HPL = 0.1nm, ");
        break;

        case(NIC_HPL_75M_AND_VPL_112M):
        fprintf(stderr, "HPL = 75m, ");
        break;

        case(NIC_HPL_25M_AND_VPL_37M):
        fprintf(stderr, "HPL = 25m, ");
        break;
        
        case(NIC_HPL_7M_AND_VPL_11M):
        fprintf(stderr, "HPL = 7m, ");
        break;
        
        case(NIC_UNKNOWN):
        default:
        fprintf(stderr, "HPL = Unknown, ");
        break;
    }

    switch(decodedMsg->nacp) {
        case(NACP_LESS_10NM):
        fprintf(stderr, "HFOM = 10nm ");
        break;

        case(NACP_LESS_4NM):
        fprintf(stderr, "HFOM = 4nm ");
        break;

        case(NACP_LESS_2NM):
        fprintf(stderr, "HFOM = 2nm ");
        break;

        case(NACP_LESS_0_5NM):
        fprintf(stderr, "HFOM = 0.5nm ");
        break;

        case(NACP_LESS_0_3NM):
        fprintf(stderr, "HFOM = 0.3nm ");
        break;

        case(NACP_LESS_0_1NM):
        fprintf(stderr, "HFOM = 0.1nm ");
        break;

        case(NACP_LESS_0_05NM):
        fprintf(stderr, "HFOM = 0.05nm ");
        break;

        case(NACP_HFOM_30M_AND_VFOM_45M):
        fprintf(stderr, "HFOM = 30m ");
        break;

        case(NACP_HFOM_10M_AND_VFOM_15M):
        fprintf(stderr, "HFOM = 10m ");
        break;

        case(NACP_HFOM_3M_AND_VFOM_4M):
        fprintf(stderr, "HFOM = 3m ");
        break;

        case(NACP_UNKNOWN):
        default:
        fprintf(stderr, "HFOM = Unknown ");
        break;
    }

    fprintf(stdout, "(NIC = %d, NACp = %d)\n", decodedMsg->nic, decodedMsg->nacp);

    fprintf(stdout, "Horizontal Velocity: %f knots at %f degrees ", decodedMsg->horizontalVelocity, decodedMsg->trackOrHeading);

    switch(decodedMsg->ttType) {
        case(TT_TYPE_TRUE_TRACK):
        fprintf(stdout, "(True Track)\n");
        break;

        case(TT_TYPE_MAG_HEADING):
        fprintf(stdout, "(Magnetic Heading)\n");
        break;

        case(TT_TYPE_TRUE_HEADING):
        fprintf(stdout, "(True Heading)\n");
        break;

        case(TT_TYPE_INVALID):
        default:
        fprintf(stdout, "(Invalid Heading)\n");
        break;
    }

    fprintf(stdout, "Vertical Velocity: %f FPM\n", decodedMsg->verticalVelocity);


    switch(decodedMsg->emergencyCode) {
        case(EMERGENCY_NONE):
        fprintf(stdout, "Emergency Code: None\n");
        break;

        case(EMERGENCY_GENERAL):
        fprintf(stdout, "Emergency Code: General\n");
        break;

        case(EMERGENCY_MEDICAL):
        fprintf(stdout, "Emergency Code: Medical\n");
        break;

        case(EMERGENCY_MIN_FUEL):
        fprintf(stdout, "Emergency Code: Min Fuel\n");
        break;

        case(EMERGENCY_NO_COMM):
        fprintf(stdout, "Emergency Code: No Comm\n");
        break;

        case(EMERGENCY_UNLAWFUL_INT):
        fprintf(stdout, "Emergency Code: Unlawful Interference\n");
        break;

        case(EMERGENCY_DOWNED):
        fprintf(stdout, "Emergency Code: Downed\n");
        break;

        default:
        fprintf(stdout, "Emergency Code: Invalid\n");
        break;
    }

    switch(decodedMsg->emitterCategory) {
        case(EMIITER_NO_INFO):
        fprintf(stdout, "Emitter/Category: No Info\n");
        break;

        case(EMITTER_LIGHT):
        fprintf(stdout, "Emitter/Category: Light\n");
        break;

        case(EMITTER_SMALL):
        fprintf(stdout, "Emitter/Category: Small\n");
        break;

        case(EMITTER_LARGE):
        fprintf(stdout, "Emitter/Category: Large\n");
        break;

        case(EMITTER_HIGH_VORTEX):
        fprintf(stdout, "Emitter/Category: High Vortex\n");
        break;

        case(EMITTER_HEAVY):
        fprintf(stdout, "Emitter/Category: Heavy\n");
        break;

        case(EMITTER_HIGH_MANUEVER):
        fprintf(stdout, "Emitter/Category: High Manueverability\n");
        break;

        case(EMITTER_ROTORCRAFT):
        fprintf(stdout, "Emitter/Category: Rotorcraft\n");
        break;

        case(EMITTER_GLIDER):
        fprintf(stdout, "Emitter/Category: Glider\n");
        break;

        case(EMITTER_LIGHTER_THAN_AIR):
        fprintf(stdout, "Emitter/Category: Lighter Than Air\n");
        break;

        case(EMITTER_PARACHUTIST):
        fprintf(stdout, "Emitter/Category: Parachutist\n");
        break;

        case(EMITTER_ULTRA_LIGHT):
        fprintf(stdout, "Emitter/Category: Ultra-light\n");
        break;

        case(EMITTER_UAV):
        fprintf(stdout, "Emitter/Category: UAV\n");
        break;

        case(EMITTER_SPACE):
        fprintf(stdout, "Emitter/Category: Space\n");
        break;

        case(EMITTER_SURFACE_EMERG):
        fprintf(stdout, "Emitter/Category: Surface Emergency\n");
        break;

        case(EMITTER_SURFACE_SERVICE):
        fprintf(stdout, "Emitter/Category: Surface Service\n");
        break;

        case(EMITTER_POINT_OBSTACLE):
        fprintf(stdout, "Emitter/Category: Point Obstacle\n");
        break;

        case(EMITTER_CLUSTER_OBST):
        fprintf(stdout, "Emitter/Category: Cluster Obstacle\n");
        break;

        case(EMITTER_LINE_OBSTACLE):
        fprintf(stdout, "Emitter/Category: Line Obstacle\n");
        break;

        default:
        fprintf(stdout, "Emitter/Category: Unknown\n");
        break;
    }

    fprintf(stdout, "Tail Number: ");
    for(int i=0; i < GDL90_TRAFFICREPORT_MSG_CALLSIGN_SIZE; i++) {
        fprintf(stdout, "%c", decodedMsg->callsign[i]);
    }
    fprintf(stdout, "\n");
}


void decode_gdl90_traffic_report(uint8_t *data, gdl90_msg_traffic_report_t *decodedMsg) {
    decodedMsg->trafficAlertStatus = GDL90_DECODE_TRAFFIC_ALERT(data);
    decodedMsg->addressType = GDL90_DECODE_ADDRESS_TYPE(data);
    decodedMsg->address = GDL90_DECODE_ADDRESS(data);
    decodedMsg->latitude = GDL90_DECODE_LATITUDE(data);
    decodedMsg->longitude = GDL90_DECODE_LONGITUDE(data);
    decodedMsg->altitude = GDL90_DECODE_ALTITUDE(data);

    decodedMsg->airborne = GDL90_DECODE_AIRBORNE(data);
    decodedMsg->reportType = GDL90_DECODE_REPORT_TYPE(data);
    decodedMsg->ttType = GDL90_DECODE_HEADING_TRACK_TYPE(data);

    decodedMsg->nic = GDL90_DECODE_NIC(data);
    decodedMsg->nacp = GDL90_DECODE_NACP(data);

    decodedMsg->horizontalVelocity = GDL90_DECODE_HORZ_VELOCITY(data);
    decodedMsg->verticalVelocity = GDL90_DECODE_VERT_VELOCITY(data);
    decodedMsg->trackOrHeading = GDL90_DECODE_HEADING(data);
    decodedMsg->emitterCategory = GDL90_DECODE_EMITTER_CATEGORY(data);

    for(int i=0; i < GDL90_TRAFFICREPORT_MSG_CALLSIGN_SIZE; i++) {
        decodedMsg->callsign[i] = data[GDL90_DECODE_CALLSIGN_START_IDX + i];
    }

    decodedMsg->emergencyCode = GDL90_DECODE_EMERGENCY_CODE(data);
}

void encode_gdl90_traffic_report(uint8_t *data, gdl90_msg_traffic_report_t *decodedMsg) {
    /*0-st  Traffic Alert Status, Address Type
    1-aa    Address
    2-aa    Address
    3-aa    Address
    4-ll    Latitude
    5-ll    Latitude
    6-ll    Latitude
    7-nn    Longitude
    8-nn    Longitude
    9-nn    Longitude
    10-dd   Altitude
    11-dm   Altitude, Misc
    12-ia   NIC, NACp
    13-hh   Horizontal Velocity
    14-hv   Horizontal Velocity, Vertical Velocity 
    15-vv   Vertical Velocity
    16-tt   Track/Heading
    17-ee   Emitter Category
    18-cc   Callsign
    19-cc   Callsign
    20-cc   Callsign
    21-cc   Callsign
    22-cc   Callsign
    23-cc   Callsign
    24-cc   Callsign
    25-cc   Callsign
    26-px   Emergency/Priority Code */

    // Traffic Alert Status, Address Type
    data[0]     = ((decodedMsg->trafficAlertStatus & 0x0F) << 4) + \
                  (decodedMsg->addressType & 0x0F);
    // Address
    data[1]     = (decodedMsg->address >> 16) & 0xFF;
    data[2]     = (decodedMsg->address >> 8) & 0xFF;
    data[3]     = decodedMsg->address & 0xFF;

    // Latitude
    int32_t convertedLatitude = (int)(decodedMsg->latitude / GDL90_COUNTS_TO_DEGREES);
    data[4]     = (convertedLatitude >> 16) & 0xFF;
    data[5]     = (convertedLatitude >> 8) & 0xFF;
    data[6]     = convertedLatitude & 0xFF;

    // Longitude
    int32_t convertedLongitude = (int)(decodedMsg->longitude / GDL90_COUNTS_TO_DEGREES);
    data[7]     = (convertedLongitude >> 16) & 0xFF;
    data[8]     = (convertedLongitude >> 8) & 0xFF;
    data[9]     = convertedLongitude & 0xFF;

    // Altitude
    int32_t convertedAltitude = (int)((decodedMsg->altitude - GDL90_ALTITUDE_OFFSET) / GDL90_ALTITUDE_FACTOR);
    data[10]    = (convertedAltitude >> 4) & 0xFF;

    // Altitude, Misc
    uint8_t misc = ((decodedMsg->airborne & 0x01) << 3) + ((decodedMsg->reportType & 0x01) << 2) + (decodedMsg->ttType & 0x03);
    data[11]    = ((convertedAltitude & 0x0F) << 4) + misc;

    // NIC, NACp
    data[12]    = ((decodedMsg->nic & 0x0F) << 4) + (decodedMsg->nacp & 0x0F);

    // Horizontal Velocity
    uint16_t convertedHorzVelocity = (int)(decodedMsg->horizontalVelocity / GDL90_HORZ_VELOCITY_FACTOR);
    data[13]    = (convertedHorzVelocity >> 4) & 0xFF;

    // Horizontal Velocity, Vertical Velocity
    int16_t convertedVertVelocity = (int)(decodedMsg->verticalVelocity / GDL90_VERT_VELOCITY_FACTOR);
    data[14]    = ((convertedHorzVelocity & 0x0F) << 4) + ((convertedVertVelocity >> 8) & 0x0F);

    // Vertical Velocity
    data[15]    = (convertedVertVelocity & 0xFF);

    // Track or Heading
    data[16]    = (uint8_t)(decodedMsg->trackOrHeading / GDL90_COUNTS_TO_HEADING);

    // Emitter Type
    data[17]    = (uint8_t)decodedMsg->emitterCategory;

    // Callsign
    for(int i = 0; i < GDL90_TRAFFICREPORT_MSG_CALLSIGN_SIZE; i++) {
        data[18+i] = decodedMsg->callsign[i];
    }

    // Emergency
    data[26]    = (uint8_t)(decodedMsg->emergencyCode << 4);
}

// Copied from the GDL90 ICD
void gdl90_crcInit() {
    uint32_t i, bitctr, crc;
    for (i = 0; i < 256; i++){
        crc = (i << 8);
        for (bitctr = 0; bitctr < 8; bitctr++) {
            crc=(crc<<1)^((crc&0x8000)?0x1021:0);
        }
        crc16table[i] = crc;
    }
}

// Copied from the GDL90 ICD
uint16_t gdl90_crcCompute(uint8_t *block, uint32_t length) {
    uint32_t i;
    uint16_t crc = 0;

    for (i = 0; i < length; i++) {
        crc = crc16table[crc >> 8] ^ (crc << 8) ^ block[i];
    }

    return crc;
}
