#include "metar.h"

#include "api.h"
#include "3rd_party/dump978/uat_decode.h"

extern pthread_mutex_t gps_mutex;
extern struct gps_data_t rx_gps_data;

extern pthread_mutex_t uat_traffic_mutex;
extern struct uat_adsb_mdb tracked_traffic[20];

static bool get_argument_text(struct mg_str *args, const char *name, char *buf, int buflen) {
    return mg_get_http_var(args, name, buf, buflen) > 0;
}

static int get_argument_int(struct mg_str *args, const char *name, int *value) {
    bool success = false;
    char buffer[64];

    if(get_argument_text(args, name, &buffer[0], sizeof(buffer)) == true) {
        *value = atoi(buffer);
        success = true;
    }

    return success;
}

static int get_argument_float(struct mg_str *args, const char *name, float *value) {
    bool success = false;
    char buffer[64];

    if(get_argument_text(args, name, &buffer[0], sizeof(buffer)) == true) {
        *value = atof(buffer);
        success = true;
    }

    return success;
}

static void api_send_empty_result(struct mg_connection *nc) {
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{}\n");
}

static void generic_api_db_dump(struct mg_connection *nc) {
    bool first = true;
    size_t numColumns = database_num_columns();

    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n[\n");
    while (database_fetch_row() == true) {
        if(!first) {
            mg_printf(nc, ",\n");
        } else {
            first = false;
        }

        mg_printf(nc, "    {\n");
        for (size_t i = 0; i < numColumns; i++) {
            mg_printf(nc, "        \"%s\": ", database_column_name(i));

            switch (database_column_type(i)) {
                case(TYPE_INTEGER):
                    mg_printf(nc, "%d", database_column_int(i));
                    break;

                case(TYPE_FLOAT):
                    mg_printf(nc, "%f", database_column_double(i));
                    break;

                case(TYPE_NULL):
                    mg_printf(nc, "null");
                    break;

                case(TYPE_BLOB):
                case(TYPE_TEXT):
                default:
                    // Some hackery to figure out if this column contains geometry (ie, WKT).
                    if(strcmp(database_column_name(i), "geometry") == 0) {
                        mg_printf(nc, "%s", database_column_text(i));
                    } else {
                        mg_printf(nc, "\"%s\"", database_column_text(i));
                    }
            }

            // If this is the last column, then don't put a comma!
            if (i < numColumns - 1) {
                mg_printf(nc, ",\n");
            } else {
                mg_printf(nc, "\n");
            }
        }
        mg_printf(nc, "    }");
    }
    mg_printf(nc, "\n]\n");
}

// Copied/modified from mdsplib, print_decoded_metar.c
static void generic_api_metar_dump(struct mg_connection *nc, Decoded_METAR* metar) {
    int i = 0;


    if(metar->NIL_rpt) {
      mg_printf(nc, "        \"nil_report\": true,\n");
    }

    if(metar->AUTO) {
      mg_printf(nc, "        \"auto_report\": true,\n");
    }

    if(metar->COR) {
      mg_printf(nc, "        \"corrected_report\": true,\n");
    }

    if(metar->winData.windVRB) {
      mg_printf(nc, "        \"variable_wind_direction\": true,\n");
    }

    if(metar->winData.windDir != INT_MAX) {
      mg_printf(nc, "        \"wind_direction\": %d,\n",metar->winData.windDir);
    }

    if(metar->winData.windSpeed != INT_MAX) {
      mg_printf(nc, "        \"wind_speed\": %d,\n",metar->winData.windSpeed);
    }

    if(metar->winData.windGust != INT_MAX) {
      mg_printf(nc, "        \"wind_gust\": %d,\n",metar->winData.windGust);
    }

    if(metar->winData.windUnits[0] != '\0') {
      mg_printf(nc, "        \"wind_units\": \"%s\",\n",metar->winData.windUnits);
    }

    if(metar->minWnDir != INT_MAX) {
      mg_printf(nc, "        \"min_wind_direction\": %d,\n",metar->minWnDir);
    }

    if(metar->maxWnDir != INT_MAX) {
      mg_printf(nc, "        \"max_wind_direction\": %d,\n",metar->maxWnDir);
    }

    if(metar->prevail_vsbyM != (float) INT_MAX) {
      mg_printf(nc, "        \"prevail_visibility_m\": %f,\n",metar->prevail_vsbyM);
    }

    if(metar->prevail_vsbyKM != (float) INT_MAX) {
      mg_printf(nc, "        \"prevail_visibility_km\": %f,\n",metar->prevail_vsbyKM);
    }

    if(metar->prevail_vsbySM != (float) INT_MAX) {
      mg_printf(nc, "        \"prevail_visibility_sm\": %.3f,\n",metar->prevail_vsbySM);
    }
    /*
    if(metar->charPrevailVsby[0] != '\0') {
      mg_printf(nc, "        \"PREVAIL VSBY (CHAR) : \"%s\",\n",metar->charPrevailVsby);
    }
    */
    if(metar->vsby_Dir[0] != '\0') {
      mg_printf(nc, "        \"visibility_direction\": \"%s\",\n",metar->vsby_Dir);
    }

    if(metar->RVRNO) {
      mg_printf(nc, "        \"rvrno\": true,\n");
    }

    mg_printf(nc, "        \"rvr\": [");

    for(i = 0; i < MAX_RUNWAYS; i++ ) {
      if( metar->RRVR[i].runway_designator[0] != '\0') {
        mg_printf(nc, "        %s{\n", i > 0 ? ",\n" : "");
        mg_printf(nc, "            \"runway_designator\": \"%s\",\n", metar->RRVR[i].runway_designator);
      } else {
        continue;
      }

      if( metar->RRVR[i].visRange != INT_MAX) {
         mg_printf(nc, "            \"vis_range_ft\": %d,\n", metar->RRVR[i].visRange);
      }

      if(metar->RRVR[i].vrbl_visRange) {
         mg_printf(nc, "            \"vrbl_vis_range\": true,\n");
      }

      if(metar->RRVR[i].below_min_RVR) {
         mg_printf(nc, "            \"below_min_rvr\": true,\n");
      }

      if(metar->RRVR[i].above_max_RVR) {
         mg_printf(nc, "            \"above_max_rvr\": true,\n");
      }

      if( metar->RRVR[i].Max_visRange != INT_MAX) {
         mg_printf(nc, "            \"max_vis_range_ft\": %d,\n", metar->RRVR[i].Max_visRange);
      }

      if( metar->RRVR[i].Min_visRange != INT_MAX) {
         mg_printf(nc, "            \"min_vis_range_ft\": %d,\n", metar->RRVR[i].Min_visRange);
      }
      mg_printf(nc, "            \"rvr\": %d\n", i);
      mg_printf(nc, "        }");
    }
    mg_printf(nc, "],\n");

    if( metar->DVR.visRange != INT_MAX) {
      mg_printf(nc, "        \"dispatch_vis_range\": %d,\n", metar->DVR.visRange);
    }

    if(metar->DVR.vrbl_visRange) {
      mg_printf(nc, "        \"vrbl_dispatch_vis_range\": true,\n");
    }

    if(metar->DVR.below_min_DVR) {
      mg_printf(nc, "        \"below_min_dvr\": true,\n");
    }

    if(metar->DVR.above_max_DVR) {
      mg_printf(nc, "        \"above_max_dvr\": true,\n");
    }

    if( metar->DVR.Max_visRange != INT_MAX) {
      mg_printf(nc, "        \"max_dispatch_vis_range_ft\": %d,\n", metar->DVR.Max_visRange);
    }

    if( metar->DVR.Min_visRange != INT_MAX) {
      mg_printf(nc, "        \"min_dispatch_vis_range_ft\": %d,\n", metar->DVR.Min_visRange);
    }

    // TODO(rdavid): Put upper limit back to MAXWXSYMBOLS.
    mg_printf(nc, "        \"wx_obstruct_vision\": [");
    for(i = 0; metar->WxObstruct[i][0] != '\0' && i < 10; i++) {
      mg_printf(nc, "\"%s\", ", metar->WxObstruct[i]);
    }
    mg_printf(nc, "],\n");

    mg_printf(nc, "        \"partial_obscurations\": [");
    for(i = 0; i < MAX_PARTIAL_OBSCURATIONS; i++) {
        if(metar->PartialObscurationAmt[i][0] != '\0') {
            mg_printf(nc, "        %s{\n", i > 0 ? ",\n" : "");
            mg_printf(nc, "            \"obscuration_amount\": \"%s\",\n", metar->PartialObscurationAmt[i]);
        } else {
            continue;
        }
     
        if(metar->PartialObscurationPhenom[i][0] != '\0') {
            mg_printf(nc, "        \"obscuration_phenom\": \"%s\",\n", metar->PartialObscurationPhenom[i]);
        }
        mg_printf(nc, "            \"partial_obscuration\": %d\n", i);
        mg_printf(nc, "        }");
    }
    mg_printf(nc, "],\n");

    mg_printf(nc, "        \"cloud_groups\": [");
    for(i = 0; (metar->cloudGroup[ i ].cloud_type[0] != '\0') && (i < MAX_CLOUD_GROUPS); i++) {
        if(metar->cloudGroup[i].cloud_type[0] != '\0') {
            mg_printf(nc, "        %s{\n", i > 0 ? ",\n" : "");
            mg_printf(nc, "            \"cloud_cover\": \"%s\",\n", metar->cloudGroup[ i ].cloud_type);
        } else {
            continue;
        }

        if(metar->cloudGroup[i].cloud_hgt_char[0] != '\0') {
            mg_printf(nc, "            \"cloud_height_str\": \"%s\",\n", metar->cloudGroup[i].cloud_hgt_char);
        }

        if(metar->cloudGroup[i].cloud_hgt_meters != INT_MAX) {
            mg_printf(nc, "            \"cloud_height_m\": %d,\n", metar->cloudGroup[i].cloud_hgt_meters);
        }

        if(metar->cloudGroup[i].other_cld_phenom[0] != '\0') {
            mg_printf(nc, "            \"other_cloud_phenom\": \"%s\",\n", metar->cloudGroup[i].other_cld_phenom);
        }

        mg_printf(nc, "            \"cloud_group\": %d\n", i);
        mg_printf(nc, "        }");
    }
    mg_printf(nc, "],\n");

    if(metar->temp != INT_MAX) {
      mg_printf(nc, "        \"temp_c\": %d,\n", metar->temp);
    }

    if(metar->dew_pt_temp != INT_MAX) {
      mg_printf(nc, "        \"dew_point_temp_c\": %d,\n", metar->dew_pt_temp);
    }

    if(metar->A_altstng) {
      mg_printf(nc, "        \"altimeter_inches\": %.2f,\n", metar->inches_altstng );
    }

    if(metar->Q_altstng) {
      mg_printf(nc, "        \"altimeter_pa\": %d,\n", metar->hectoPasc_altstng );
    }

    //sprintf_tornadic_info (string, metar);

    if(metar->autoIndicator[0] != '\0') {
        mg_printf(nc, "        \"auto_indicator\": \"%s\",\n", metar->autoIndicator);
    }

    if(metar->PKWND_dir !=  INT_MAX) {
      mg_printf(nc, "        \"peak_wind_direction\": %d,\n",metar->PKWND_dir);
    }

    if(metar->PKWND_speed !=  INT_MAX) {
      mg_printf(nc, "        \"peak_wind_speed\": %d,\n",metar->PKWND_speed);
    }

    if(metar->PKWND_hour !=  INT_MAX) {
      mg_printf(nc, "        \"peak_wind_hour\": %d,\n",metar->PKWND_hour);
    }

    if(metar->PKWND_minute !=  INT_MAX) {
      mg_printf(nc, "        \"peak_wind_minute\": %d,\n",metar->PKWND_minute);
    }

    if(metar->WshfTime_hour != INT_MAX) {
      mg_printf(nc, "        \"windshift_hour\": %d,\n",metar->WshfTime_hour);
    }

    if(metar->WshfTime_minute != INT_MAX) {
      mg_printf(nc, "        \"windshift_minute\": %d,\n",metar->WshfTime_minute);
    }

    if(metar->Wshft_FROPA != false) {
      mg_printf(nc, "        \"fropa_assoc_wind_shift\": true,\n");
    }

    if(metar->TWR_VSBY != (float) INT_MAX) {
      mg_printf(nc, "        \"tower_visibility\": %.2f,\n",metar->TWR_VSBY);
    }

    if(metar->SFC_VSBY != (float) INT_MAX) {
      mg_printf(nc, "        \"surface_visibility\": %.2f,\n",metar->SFC_VSBY);
    }

    if(metar->minVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"min_visibility_sm\": %.4f,\n",metar->minVsby);
    }

    if(metar->maxVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"max_visibility_sm\": %.4f,\n",metar->maxVsby);
    }

    if( metar->VSBY_2ndSite != (float) INT_MAX) {
      mg_printf(nc, "        \"visibility_2nd_site_sm\": %.4f,\n",metar->VSBY_2ndSite);
    }

    if( metar->VSBY_2ndSite_LOC[0] != '\0') {
      mg_printf(nc, "        \"visibility_2nd_site_location\": \"%s\",\n", metar->VSBY_2ndSite_LOC);
    }

    if(metar->OCNL_LTG) {
      mg_printf(nc, "        \"occassional_lightning\": true,\n");
    }

    if(metar->FRQ_LTG) {
      mg_printf(nc, "        \"frequent_lightning\": true,\n");
    }

    if(metar->CNS_LTG) {
      mg_printf(nc, "        \"continuous_lightning\": true,\n");
    }

    if(metar->CG_LTG) {
      mg_printf(nc, "        \"cloud_ground_lightning\": true,\n");
    }

    if(metar->IC_LTG) {
      mg_printf(nc, "        \"in_cloud_lightning\": true,\n");
    }

    if(metar->CC_LTG) {
      mg_printf(nc, "        \"cloud_cloud_lightning\": true,\n");
    }

    if(metar->CA_LTG) {
      mg_printf(nc, "        \"cloud_air_lightning\": true,\n");
    }

    if(metar->AP_LTG) {
      mg_printf(nc, "        \"lightning_at_airport\": true,\n");
    }

    if(metar->OVHD_LTG) {
      mg_printf(nc, "        \"lightning_overhead\": true,\n");
    }

    if(metar->DSNT_LTG) {
      mg_printf(nc, "        \"distant_lightning\": true,\n");
    }

    if(metar->LightningVCTS) {
      mg_printf(nc, "        \"lightning_within_5_10\": true,\n");
    }

    if(metar->LightningTS) {
      mg_printf(nc, "        \"lightning_within_5\": true,\n");
    }

    if(metar->VcyStn_LTG) {
      mg_printf(nc, "        \"vcy_stn_lightning\": true,\n");
    }

    if(metar->LTG_DIR[0] != '\0') {
      mg_printf(nc, "        \"direction_of_lightning\": \"%s\",\n", metar->LTG_DIR);
    }

    i = 0;
    mg_printf(nc, "        \"recent_weather\": [");
    while((i < 3) && (metar->ReWx[ i ].Recent_weather[0] != '\0')) {
      mg_printf(nc, "        %s{\n", i > 0 ? ",\n" : "");
      mg_printf(nc, "            \"description\": \"%s\",\n", metar->ReWx[i].Recent_weather);

      if(metar->ReWx[i].Bhh != INT_MAX) {
         mg_printf(nc, "            \"beginning_hh\": %d,\n",metar->ReWx[i].Bhh);
      }
      if(metar->ReWx[i].Bmm != INT_MAX) {
         mg_printf(nc, "            \"beginning_mm\": %d,\n",metar->ReWx[i].Bmm);
      }

      if(metar->ReWx[i].Ehh != INT_MAX) {
         mg_printf(nc, "            \"end_hh\": %d,\n",metar->ReWx[i].Ehh);
      }
      if(metar->ReWx[i].Emm != INT_MAX) {
         mg_printf(nc, "            \"end_mm\": %d,\n",metar->ReWx[i].Emm);
      }
      mg_printf(nc, "            \"recent_weather\": %d\n", i);
      mg_printf(nc, "}\n");
      i++;
    }
    mg_printf(nc, "],\n");

    if(metar->minCeiling != INT_MAX) {
      mg_printf(nc, "        \"min_ceiling_ft\": %d,\n",metar->minCeiling);
    }

    if(metar->maxCeiling != INT_MAX) {
      mg_printf(nc, "        \"max_ceiling_Ft\": %d,\n",metar->maxCeiling);
    }

    if(metar->CIG_2ndSite_Meters != INT_MAX) {
      mg_printf(nc, "        \"ceiling_2nd_site_meters\": %d,\n",metar->CIG_2ndSite_Meters);
    }

    if(metar->CIG_2ndSite_LOC[0] != '\0') {
      mg_printf(nc, "        \"ceiling_2nd_site_location\": \"%s\",\n",metar->CIG_2ndSite_LOC);
    }

    if(metar->PRESFR) {
      mg_printf(nc, "        \"pressure_falling_rapidly\": true,\n");
    }
    if(metar->PRESRR) {
      mg_printf(nc, "        \"pressure_rising_rapidly\": true,\n");
    }

    if(metar->SLPNO) {
      mg_printf(nc, "        \"sea_level_pressure_not_avail\": true,\n");
    }

    if(metar->SLP != (float) INT_MAX) {
      mg_printf(nc, "        \"sea_level_pressure_hPa\": %.1f,\n", metar->SLP);
    }

    if(metar->SectorVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"sector_visibility_miles\": %.2f,\n", metar->SectorVsby );
    }

    if(metar->SectorVsby_Dir[0] != '\0') {
      mg_printf(nc, "        \"sector_visibility_octant\": \"%s\",\n", metar->SectorVsby_Dir );
    }

    if(metar->TS_LOC[0] != '\0') {
      mg_printf(nc, "        \"thunderstorm_location\": \"%s\",\n", metar->TS_LOC );
    }

    if(metar->TS_MOVMNT[0] != '\0') {
      mg_printf(nc, "        \"thunderstorm_movement\": \"%s\",\n", metar->TS_MOVMNT);
    }

    if(metar->GR) {
      mg_printf(nc, "        \"hailstones\": true,\n");
    }

    if(metar->GR_Size != (float) INT_MAX) {
      mg_printf(nc, "        \"hailstone_size_inches\": %.3f,\n",metar->GR_Size);
    }

    if(metar->VIRGA) {
      mg_printf(nc, "        \"virga\": true,\n");
    }

    if(metar->VIRGA_DIR[0] != '\0') {
      mg_printf(nc, "        \"direction_of_virga\": \"%s\",\n", metar->VIRGA_DIR);
    }

    mg_printf(nc, "        \"surface_obscurations\": [");
    for(i = 0; i < MAX_SURFACE_OBSCURATIONS; i++) {
      if( metar->SfcObscuration[i][0] != '\0') {
         mg_printf(nc, "%s\"%s\"", i > 0 ? ", " : "", metar->SfcObscuration[i]);
      }
    }
    mg_printf(nc, "],\n");

    if(metar->Num8thsSkyObscured != INT_MAX) {
      mg_printf(nc, "        \"8ths_of_sky_obscured\": %d,\n",metar->Num8thsSkyObscured);
    }

    if(metar->CIGNO) {
      mg_printf(nc, "        \"ceiling_unavailable\": true,\n");
    }

    if(metar->Ceiling != INT_MAX) {
      mg_printf(nc, "        \"ceiling_ft\": %d,\n",metar->Ceiling);
    }

    if(metar->Estimated_Ceiling != INT_MAX) {
      mg_printf(nc, "        \"estimated_ceiling_ft\": %d,\n",metar->Estimated_Ceiling);
    }

    if(metar->VrbSkyBelow[0] != '\0') {
      mg_printf(nc, "        \"vrb_sky_below\": \"%s\",\n",metar->VrbSkyBelow);
    }

    if(metar->VrbSkyAbove[0] != '\0') {
      mg_printf(nc, "        \"vrb_sky_above\": \"%s\",\n",metar->VrbSkyAbove);
    }

    if(metar->VrbSkyLayerHgt != INT_MAX) {
      mg_printf(nc, "        \"vrb_sky_layer_height_ft\": %d,\n",metar->VrbSkyLayerHgt);
    }

    if(metar->ObscurAloftHgt != INT_MAX) {
      mg_printf(nc, "        \"hgt_obscur_aloft_ft\": %d,\n",metar->ObscurAloftHgt);
    }

    if(metar->ObscurAloft[0] != '\0') {
      mg_printf(nc, "        \"obscur_phenom_aloft\": \"%s\",\n",metar->ObscurAloft);
    }

    if(metar->ObscurAloftSkyCond[0] != '\0') {
      mg_printf(nc, "        \"obscur_aloft_sky_cond\": \"%s\",\n",metar->ObscurAloftSkyCond);
    }

    if(metar->NOSPECI) {
      mg_printf(nc, "        \"nospeci\": true,\n");
    }

    if(metar->LAST) {
      mg_printf(nc, "        \"last\": true,\n");
    }

    if(metar->synoptic_cloud_type[0] != '\0') {
      mg_printf(nc, "        \"synoptic_cloud_group\": \"%s\",\n",metar->synoptic_cloud_type);
    }

    if(metar->CloudLow != '\0') {
      mg_printf(nc, "        \"low_cloud_code\": %c,\n",metar->CloudLow);
    }

    if(metar->CloudMedium != '\0') {
      mg_printf(nc, "        \"medium_cloud_code\": %c,\n",metar->CloudMedium);
    }

    if(metar->CloudHigh != '\0') {
      mg_printf(nc, "        \"high_cloud_code\": %c,\n",metar->CloudHigh);
    }

    if(metar->SNINCR != INT_MAX) {
      mg_printf(nc, "        \"snow_incr_rapid_inches\": %d,\n",metar->SNINCR);
    }

    if(metar->SNINCR_TotalDepth != INT_MAX) {
      mg_printf(nc, "        \"snow_incr_total_inches\": %d,\n",metar->SNINCR_TotalDepth);
    }

    if(metar->snow_depth_group[0] != '\0') {
      mg_printf(nc, "        \"snow_depth_group\": \"%s\",\n",metar->snow_depth_group);
    }

    if(metar->snow_depth != INT_MAX) {
      mg_printf(nc, "        \"snow_depth_inches\": %d,\n",metar->snow_depth);
    }

    if(metar->WaterEquivSnow != (float) INT_MAX) {
      mg_printf(nc, "        \"water_equival_snow\": %.2f,\n",metar->WaterEquivSnow);
    }

    if(metar->SunshineDur != INT_MAX) {
      mg_printf(nc, "        \"sunshine_minutes\": %d,\n",metar->SunshineDur);
    }

    if(metar->SunSensorOut) {
      mg_printf(nc, "        \"sun_sensor_out\": true,\n");
    }

    if(metar->hourlyPrecip != (float) INT_MAX) {
      mg_printf(nc, "        \"hh_preception_inches\": %.2f,\n",metar->hourlyPrecip);
    }

    if( metar->precip_amt != (float) INT_MAX) {
      mg_printf(nc, "        \"3-6hr_precip_inches\": %.2f,\n", metar->precip_amt);
    }

    if( metar->Indeterminant3_6HrPrecip) {
      mg_printf(nc, "        \"indeterminant_3-6hr_precip\": true,\n");
    }

    if( metar->precip_24_amt !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24_hour_precipitation_inches\": %.2f,\n", metar->precip_24_amt);
    }

    if(metar->Indeterminant_24HrPrecip) {
      mg_printf(nc, "        \"indeterminant_24_hour_precip\": true,\n");
    }

    if(metar->Temp_2_tenths != (float) INT_MAX) {
      mg_printf(nc, "        \"temp_2_tenths_celcius\": %.1f,\n",metar->Temp_2_tenths);
    }

    if(metar->DP_Temp_2_tenths != (float) INT_MAX) {
      mg_printf(nc, "        \"dew_point_2_tenths_celcius\": %.1f,\n",metar->DP_Temp_2_tenths);
    }

    if(metar->maxtemp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"max_temp_celcius\": %.1f,\n", metar->maxtemp);
    }

    if(metar->mintemp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"min_temp_celcius\": %.1f,\n", metar->mintemp);
    }

    if(metar->max24temp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24hr_max_temp_celcius\": %.1f,\n", metar->max24temp);
    }

    if(metar->min24temp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24hr_min_temp_celcius\": %.1f,\n", metar->min24temp);
    }

    if(metar->char_prestndcy != INT_MAX) {
      mg_printf(nc, "        \"char_pressure_tendency\": %d,\n", metar->char_prestndcy );
    }

    if(metar->prestndcy != (float) INT_MAX) {
      mg_printf(nc, "        \"char_pressure_tendency_hpa\": %.1f,\n", metar->prestndcy );
    }

    if(metar->PWINO) {
      mg_printf(nc, "        \"pwino\": true,\n");
    }

    if(metar->PNO) {
      mg_printf(nc, "        \"pno\": true,\n");
    }

    if(metar->CHINO) {
      mg_printf(nc, "        \"chino\": true,\n");
    }

    if(metar->CHINO_LOC[0] != '\0') {
      mg_printf(nc, "        \"chino_loc\": \"%s\",\n",metar->CHINO_LOC);
    }

    if(metar->VISNO) {
      mg_printf(nc, "        \"visno\": true,\n");
    }

    if(metar->VISNO_LOC[0] != '\0') {
      mg_printf(nc, "        \"visno_location\": \"%s\",\n",metar->VISNO_LOC);
    }

    if(metar->FZRANO) {
      mg_printf(nc, "        \"fzrano\": true,\n");
    }

    if(metar->TSNO) {
      mg_printf(nc, "        \"tsno\": true,\n");
    }

    if(metar->DollarSign) {
      mg_printf(nc, "        \"dollar_sign_indicator\": true,\n");
    }

    if(metar->horiz_vsby[0] != '\0') {
      mg_printf(nc, "        \"horizon_visibility\": \"%s\",\n",metar->horiz_vsby);
    }

    if(metar->dir_min_horiz_vsby[0] != '\0') {
      mg_printf(nc, "        \"dir_min_horiz_vsby\": \"%s\",\n",metar->dir_min_horiz_vsby);
    }

    if(metar->CAVOK) {
      mg_printf(nc, "        \"cavok\": true,\n");
    }

    if( metar->VertVsby != INT_MAX) {
      mg_printf(nc, "        \"vertical_visibility_meters\": %d,\n", metar->VertVsby );
    }

    /*
    if( metar->charVertVsby[0] != '\0' )
      mg_printf(nc, "        \"Vert. Vsby (CHAR)   : \"%s\"\n",
                  metar->charVertVsby );
    */

    if(metar->QFE != INT_MAX) {
      mg_printf(nc, "        \"qfe\": %d,\n", metar->QFE);
    }

    if(metar->VOLCASH) {
      mg_printf(nc, "        \"volcanic_ash\": true,\n");
    }

    if(metar->min_vrbl_wind_dir != INT_MAX) {
      mg_printf(nc, "        \"win_vrbl_wind_dir\": %d,\n",metar->min_vrbl_wind_dir);
    }

    if(metar->max_vrbl_wind_dir != INT_MAX) {
      mg_printf(nc, "        \"max_vrbl_wind_dir\": %d,\n",metar->max_vrbl_wind_dir);
    }

    // Hacky way to make sure the last entry never gets a trailing comma.
    mg_printf(nc, "        \"decoded\": true\n");
}

void api_location(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;

    pthread_mutex_lock(&gps_mutex);
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n" \
        "{\n" \
        "    \"status\": %d,\n" \
        "    \"timestamp\": %f,\n" \
        "    \"mode\": %d,\n" \
        "    \"sats_used\": %d,\n" \
        "    \"latitude\": %f,\n" \
        "    \"longitude\": %f,\n" \
        "    \"altitude\": %f,\n" \
        "    \"track\": %f,\n" \
        "    \"speed\": %f,\n" \
        "    \"climb\": %f,\n" \
        "    \"uncertainty_pos\": [\n" \
        "         %f,\n" \
        "         %f,\n" \
        "         %f\n" \
        "     ],\n" \
        "    \"uncertainty_speed\": %f\n" \
        "}\n", \
        rx_gps_data.status,
        rx_gps_data.fix.time, \
        rx_gps_data.fix.mode, \
        rx_gps_data.satellites_used, \
        rx_gps_data.fix.latitude, \
        rx_gps_data.fix.longitude, \
        rx_gps_data.fix.altitude, \
        rx_gps_data.fix.track, \
        rx_gps_data.fix.speed, \
        rx_gps_data.fix.climb, \
        rx_gps_data.fix.epx, \
        rx_gps_data.fix.epy, \
        rx_gps_data.fix.epv, \
        rx_gps_data.fix.eps);

    pthread_mutex_unlock(&gps_mutex);

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_satellites(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n{\n");
    mg_printf(nc, "\"satellites\": [\n");

    pthread_mutex_lock(&gps_mutex);
    for (int i = 0; i < rx_gps_data.satellites_visible; i++) {
        mg_printf(nc, \
            "    {\n" \
            "        \"prn\": %d,\n" \
            "        \"snr\": %f,\n" \
            "        \"used\": %d,\n" \
            "        \"elevation\": %d,\n" \
            "        \"azimuth\": %d\n" \
            "    }", \
            rx_gps_data.skyview[i].PRN, \
            rx_gps_data.skyview[i].ss, \
            rx_gps_data.skyview[i].used, \
            rx_gps_data.skyview[i].elevation, \
            rx_gps_data.skyview[i].azimuth);
        if(i != rx_gps_data.satellites_visible - 1){
            mg_printf(nc, ",");
        }
        mg_printf(nc, "\n");
    }
    mg_printf(nc, "],\n");
    mg_printf(nc, "    \"num_satellites\": %d,\n" \
                  "    \"num_satellites_used\": %d\n", \
                  rx_gps_data.satellites_visible, \
                  rx_gps_data.satellites_used);
    mg_printf(nc, "}\n");

    pthread_mutex_unlock(&gps_mutex);

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_name_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char name[256];

    if(get_argument_text(&message->query_string, "name", &name[0], sizeof(name)) == true){
        database_search_airports_by_name(name);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'name'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_window_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    float latMin, latMax, lonMin, lonMax;
    
    if(get_argument_float(&message->query_string, "latMin", &latMin) &&
       get_argument_float(&message->query_string, "latMax", &latMax) &&
       get_argument_float(&message->query_string, "lonMin", &lonMin) &&
       get_argument_float(&message->query_string, "lonMax", &lonMax)) {
        database_search_airports_within_window(latMin, latMax, lonMin, lonMax);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find latMin, latMax, lonMin, or lonMax");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_id_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_airport_by_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}


void api_airport_runway_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_runways_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_radio_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];
    
    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_radio_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_diagram_search(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_search_charts_by_airport_id(&id[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airport_find_nearest(struct mg_connection *nc, int ev, void *ev_data) {
    // TODO: Make sure that GPSD is actually up and running.
    database_find_nearest_airports(rx_gps_data.fix.latitude, rx_gps_data.fix.longitude);
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_available_airspace_shapefiles(struct mg_connection *nc, int ev, void *ev_data) {
    database_available_airspace_shapefiles();
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_airspace_geojson_by_class(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char class[16];

    if(get_argument_text(&message->query_string, "class", &class[0], sizeof(class)) == true) {
        database_get_airspace_geojson_by_class(&class[0]);
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_available_faa_charts(struct mg_connection *nc, int ev, void *ev_data) {
    database_get_available_faa_charts();
    generic_api_db_dump(nc);
    database_finish_query();
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_set_faa_chart_download_flag(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    int chart_id;
    int to_download;

    if((get_argument_int(&message->query_string, "id", &chart_id) == true) &&
       (get_argument_int(&message->query_string, "download", &to_download) == true)) {
        database_set_faa_chart_download_flag(chart_id, (bool)(to_download == 1));
        generic_api_db_dump(nc);
        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }
    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_uat_get_winds(struct mg_connection *nc, int ev, void *ev_data) {
    (void) ev;
    (void) ev_data;
    database_get_recent_winds();
    generic_api_db_dump(nc);
    database_finish_query();

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

void api_metar_by_airport_id(struct mg_connection *nc, int ev, void *ev_data) {
    struct http_message *message = (struct http_message *)ev_data;
    char id[256];

    int8_t decode_metar_success;
    Decoded_METAR metar;

    if(get_argument_text(&message->query_string, "id", &id[0], sizeof(id)) == true) {
        database_get_metar_by_airport_id(&id[0]);

        uint16_t numColumns = database_num_columns();
        bool first = true;
        
        mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n[\n");
        while (database_fetch_row() == true) {
            if(!first) {
                mg_printf(nc, ",\n");
            } else {
                first = false;
            }

            mg_printf(nc, "    {\n");
            for (size_t i = 0; i < numColumns; i++) {
                mg_printf(nc, "        \"%s\": ", database_column_name(i));

                if(strcmp(database_column_name(i), "report") == 0) {
                    decode_metar_success = decode_metar((char*)database_column_text(i), &metar);
                }

                switch (database_column_type(i)) {
                    case(TYPE_INTEGER):
                        mg_printf(nc, "%d", database_column_int(i));
                        break;

                    case(TYPE_FLOAT):
                        mg_printf(nc, "%f", database_column_double(i));
                        break;

                    case(TYPE_NULL):
                        mg_printf(nc, "null");
                        break;

                    case(TYPE_BLOB):
                    case(TYPE_TEXT):
                    default:
                        mg_printf(nc, "\"%s\"", database_column_text(i));
                }

                // If this is the last column and the METAR failed to decode, don't put a comma!
                if((i == numColumns - 1) && (decode_metar_success != 0)) {
                    mg_printf(nc, "\n");
                } else {
                    mg_printf(nc, ",\n");
                }
            }

            // If we successfully decoded the METAR, then dump it out now!
            if(decode_metar_success != 0) {
                fprintf(stdout, "Failed to decode METAR! (%d)\n", decode_metar_success);
            } else {
                generic_api_metar_dump(nc, &metar);
            }

            mg_printf(nc, "    }");
        }
        mg_printf(nc, "\n]\n");


        database_finish_query();
    } else {
        fprintf(stdout, "%s\n", "ERROR: Could not find 'id'");
        api_send_empty_result(nc);
    }

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

// Based on uat_decode.c printf's.
void api_get_traffic(struct mg_connection *nc, int ev, void *ev_data) {
    //struct http_message *message = (struct http_message *)ev_data;

    pthread_mutex_lock(&uat_traffic_mutex);
    mg_printf(nc, "HTTP/1.0 200 OK\r\n\r\n[\n");
    for(size_t i = 0; i < 128; i++) {
        // Short circuit if we've reached the end of the list.
        if(tracked_traffic[i].address == 0) {
            break;
        }

        mg_printf(nc, "%s    {\n", i > 0 ? ",\n" : "");
        mg_printf(nc, "        \"mdb_type\": %d,\n", tracked_traffic[i].mdb_type);
        mg_printf(nc, "        \"address\": \"%06X\",\n", tracked_traffic[i].address);

        mg_printf(nc, "        \"has_sv\": %d,\n", tracked_traffic[i].has_sv != 0);
        if(tracked_traffic[i].has_sv  != 0) {
            mg_printf(nc, "        \"nic\": %d,\n", tracked_traffic[i].nic);
            mg_printf(nc, "        \"latitude\": %.6f,\n", tracked_traffic[i].lat);
            mg_printf(nc, "        \"longitude\": %.6f,\n", tracked_traffic[i].lon);
            mg_printf(nc, "        \"pos_valid\": %d,\n", tracked_traffic[i].position_valid != 0);
            mg_printf(nc, "        \"altitude\": %d,\n", tracked_traffic[i].altitude);
            mg_printf(nc, "        \"altitude_type\": %d,\n", tracked_traffic[i].altitude_type);
            mg_printf(nc, "        \"ns_vel\": %d,\n", tracked_traffic[i].ns_vel);
            mg_printf(nc, "        \"ns_vel_valid\": %d,\n", tracked_traffic[i].ns_vel_valid != 0);
            mg_printf(nc, "        \"ew_vel\": %d,\n", tracked_traffic[i].ew_vel);
            mg_printf(nc, "        \"ew_vel_valid\": %d,\n", tracked_traffic[i].ew_vel_valid != 0);
            mg_printf(nc, "        \"track_type\": %d,\n", tracked_traffic[i].track_type);
            mg_printf(nc, "        \"track\": %d,\n", tracked_traffic[i].track);
            mg_printf(nc, "        \"speed\": %d,\n", tracked_traffic[i].speed);
            mg_printf(nc, "        \"speed_valid\": %d,\n", tracked_traffic[i].speed_valid != 0);
            mg_printf(nc, "        \"vert_rate_source\": %d,\n", tracked_traffic[i].vert_rate_source);
            mg_printf(nc, "        \"dimensions_valid\": %d,\n", tracked_traffic[i].dimensions_valid != 0);
            mg_printf(nc, "        \"length\": %.1f,\n", tracked_traffic[i].length);
            mg_printf(nc, "        \"width\": %.1f,\n", tracked_traffic[i].width);
            mg_printf(nc, "        \"position_offset\": %d,\n", tracked_traffic[i].position_offset);
            mg_printf(nc, "        \"utc_coupled\": %d,\n", tracked_traffic[i].utc_coupled);
            mg_printf(nc, "        \"tisb_site_id\": %d,\n", tracked_traffic[i].tisb_site_id);
        }

        mg_printf(nc, "        \"has_ms\": %d,\n", tracked_traffic[i].has_ms != 0);
        if(tracked_traffic[i].has_ms  != 0) {
            mg_printf(nc, "        \"emitter_category\": %d,\n", tracked_traffic[i].emitter_category);
            mg_printf(nc, "        \"callsign_type\": %d,\n", tracked_traffic[i].callsign_type);
            if(tracked_traffic[i].callsign_type != CS_INVALID) {
                mg_printf(nc, "        \"callsign\": \"%s\",\n", tracked_traffic[i].callsign);
            }
            mg_printf(nc, "        \"emergency_status\": %d,\n", tracked_traffic[i].emergency_status);
            mg_printf(nc, "        \"uat_version\": %d,\n", tracked_traffic[i].uat_version);
            mg_printf(nc, "        \"sil\": %d,\n", tracked_traffic[i].sil);
            mg_printf(nc, "        \"transmit_mso\": %d,\n", tracked_traffic[i].transmit_mso);
            mg_printf(nc, "        \"nac_p\": %d,\n", tracked_traffic[i].nac_p);
            mg_printf(nc, "        \"nac_v\": %d,\n", tracked_traffic[i].nac_v);
            mg_printf(nc, "        \"nic_baro\": %d,\n", tracked_traffic[i].nic_baro);
            mg_printf(nc, "        \"has_cdti\": %d,\n", tracked_traffic[i].has_cdti != 0);
            mg_printf(nc, "        \"has_acas\": %d,\n", tracked_traffic[i].has_acas != 0);
            mg_printf(nc, "        \"acas_ra_active\": %d,\n", tracked_traffic[i].acas_ra_active);
            mg_printf(nc, "        \"ident_active\": %d,\n", tracked_traffic[i].ident_active);
            mg_printf(nc, "        \"atc_services\": %d,\n", tracked_traffic[i].atc_services);
            mg_printf(nc, "        \"heading_type\": %d,\n", tracked_traffic[i].heading_type);
        }

        mg_printf(nc, "        \"index\": %lu\n", i);
        mg_printf(nc, "    }");
    }
    mg_printf(nc, "\n]\n");
    pthread_mutex_unlock(&uat_traffic_mutex);

    nc->flags |= MG_F_SEND_AND_CLOSE;
}

