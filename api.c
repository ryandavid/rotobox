#include "api.h"
#include "mdsplib/include/metar.h"

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

    if(metar->codeName[0] != '\0') {
        mg_printf(nc, "        \"REPORT CODE NAME\": \"%s\"\n",metar->codeName);
    }

    if(metar->stnid[0] != '\0') {
        mg_printf(nc, "        \"STATION ID\": \"%s\"\n",metar->stnid);
    }

    if(metar->ob_date != INT_MAX) {
        mg_printf(nc, "        \"OBSERVATION DAY\": %d\n",metar->ob_date);
    }

    if(metar->ob_hour != INT_MAX) {
      mg_printf(nc, "        \"OBSERVATION HOUR\": %d\n",metar->ob_hour);
    }

    if(metar->ob_minute != INT_MAX) {
      mg_printf(nc, "        \"OBSERVATION MINUTE\": %d\n",metar->ob_minute);
    }

    if(metar->NIL_rpt) {
      mg_printf(nc, "        \"NIL REPORT\": true\n");
    }

    if(metar->AUTO) {
      mg_printf(nc, "        \"AUTO REPORT\": true\n");
    }

    if(metar->COR) {
      mg_printf(nc, "        \"CORRECTED REPORT\": true\n");
    }

    if(metar->winData.windVRB) {
      mg_printf(nc, "        \"WIND DIRECTION VRB  : true\n");
    }

    if(metar->winData.windDir != INT_MAX) {
      mg_printf(nc, "        \"WIND DIRECTION\": %d\n",metar->winData.windDir);
    }

    if(metar->winData.windSpeed != INT_MAX) {
      mg_printf(nc, "        \"WIND SPEED\": %d\n",metar->winData.windSpeed);
    }

    if(metar->winData.windGust != INT_MAX) {
      mg_printf(nc, "        \"WIND GUST\": %d\n",metar->winData.windGust);
    }

    if(metar->winData.windUnits[0] != '\0') {
      mg_printf(nc, "        \"WIND UNITS\": \"%s\"\n",metar->winData.windUnits);
    }

    if(metar->minWnDir != INT_MAX) {
      mg_printf(nc, "        \"MIN WIND DIRECTION\": %d\n",metar->minWnDir);
    }

    if(metar->maxWnDir != INT_MAX) {
      mg_printf(nc, "        \"MAX WIND DIRECTION\": %d\n",metar->maxWnDir);
    }

    if(metar->prevail_vsbyM != (float) INT_MAX) {
      mg_printf(nc, "        \"PREVAIL VSBY (M)\": %f\n",metar->prevail_vsbyM);
    }

    if(metar->prevail_vsbyKM != (float) INT_MAX) {
      mg_printf(nc, "        \"PREVAIL VSBY (KM)\": %f\n",metar->prevail_vsbyKM);
    }

    if(metar->prevail_vsbySM != (float) INT_MAX) {
      mg_printf(nc, "        \"PREVAIL VSBY (SM)\": %.3f\n",metar->prevail_vsbySM);
    }
    /*
    if(metar->charPrevailVsby[0] != '\0') {
      mg_printf(nc, "        \"PREVAIL VSBY (CHAR) : \"%s\"\n",metar->charPrevailVsby);
    }
    */
    if(metar->vsby_Dir[0] != '\0') {
      mg_printf(nc, "        \"VISIBILITY DIRECTION\": \"%s\"\n",metar->vsby_Dir);
    }

    if(metar->RVRNO) {
      mg_printf(nc, "        \"RVRNO\": true\n");
    }

    for(i = 0; i < MAX_RUNWAYS; i++ ) {
      if( metar->RRVR[i].runway_designator[0] != '\0') {
         mg_printf(nc, "        \"RUNWAY DESIGNATOR\": \"%s\"\n", metar->RRVR[i].runway_designator);
      }

      if( metar->RRVR[i].visRange != INT_MAX) {
         mg_printf(nc, "        \"R_WAY VIS RANGE (FT)\": %d\n", metar->RRVR[i].visRange);
      }

      if(metar->RRVR[i].vrbl_visRange) {
         mg_printf(nc, "        \"VRBL VISUAL RANGE\": true\n");
      }

      if(metar->RRVR[i].below_min_RVR) {
         mg_printf(nc, "        \"BELOW MIN RVR\": true\n");
      }

      if(metar->RRVR[i].above_max_RVR) {
         mg_printf(nc, "        \"ABOVE MAX RVR\": true\n");
      }

      if( metar->RRVR[i].Max_visRange != INT_MAX) {
         mg_printf(nc, "        \"MX R_WAY VISRNG (FT)\": %d\n", metar->RRVR[i].Max_visRange);
      }

      if( metar->RRVR[i].Min_visRange != INT_MAX) {
         mg_printf(nc, "        \"MN R_WAY VISRNG (FT)\": %d\n", metar->RRVR[i].Min_visRange);
      }
    }


    if( metar->DVR.visRange != INT_MAX) {
      mg_printf(nc, "        \"DISPATCH VIS RANGE\": %d\n", metar->DVR.visRange);
    }

    if(metar->DVR.vrbl_visRange) {
      mg_printf(nc, "        \"VRBL DISPATCH VISRNG\": true\n");
    }

    if(metar->DVR.below_min_DVR) {
      mg_printf(nc, "        \"BELOW MIN DVR\": true\n");
    }

    if(metar->DVR.above_max_DVR) {
      mg_printf(nc, "        \"ABOVE MAX DVR\": true\n");
    }

    if( metar->DVR.Max_visRange != INT_MAX) {
      mg_printf(nc, "        \"MX DSPAT VISRNG (FT)\": %d\n", metar->DVR.Max_visRange);
    }

    if( metar->DVR.Min_visRange != INT_MAX) {
      mg_printf(nc, "        \"MN DSPAT VISRNG (FT)\": %d\n", metar->DVR.Min_visRange);
    }

    // TODO(rdavid): Put upper limit back to MAXWXSYMBOLS.
    for(i = 0; metar->WxObstruct[i][0] != '\0' && i < 10; i++) {
      mg_printf(nc, "        \"WX/OBSTRUCT VISION\": \"%s\"\n", metar->WxObstruct[i] );
    }

    for(i = 0; i < MAX_PARTIAL_OBSCURATIONS; i++) {
        if(metar->PartialObscurationAmt[i][0] != '\0') {
            mg_printf(nc, "        \"OBSCURATION AMOUNT\": \"%s\"\n", metar->PartialObscurationAmt[i]);
        }
     
        if(metar->PartialObscurationPhenom[i][0] != '\0') {
            mg_printf(nc, "        \"OBSCURATION PHENOM\": \"%s\"\n", metar->PartialObscurationPhenom[i]);
        }
    }

    for(i = 0; (metar->cloudGroup[ i ].cloud_type[0] != '\0') && (i < MAX_CLOUD_GROUPS); i++) {
        if(metar->cloudGroup[ i ].cloud_type[0] != '\0') {
            mg_printf(nc, "        \"CLOUD COVER\": \"%s\"\n", metar->cloudGroup[ i ].cloud_type);
        }

        if(metar->cloudGroup[ i ].cloud_hgt_char[0] != '\0') {
            mg_printf(nc, "        \"CLOUD HGT (CHARAC.)\": \"%s\"\n", metar->cloudGroup[ i ].cloud_hgt_char);
        }

        if(metar->cloudGroup[ i ].cloud_hgt_meters != INT_MAX) {
            mg_printf(nc, "        \"CLOUD HGT (METERS)\": %d\n", metar->cloudGroup[ i ].cloud_hgt_meters);
        }

        if(metar->cloudGroup[ i ].other_cld_phenom[0] != '\0') {
            mg_printf(nc, "        \"OTHER CLOUD PHENOM\": \"%s\"\n", metar->cloudGroup[ i ].other_cld_phenom);
        }
    }

    if(metar->temp != INT_MAX) {
      mg_printf(nc, "        \"TEMP. (CELSIUS)\": %d\n", metar->temp);
    }

    if(metar->dew_pt_temp != INT_MAX) {
      mg_printf(nc, "        \"D.P. TEMP. (CELSIUS)\": %d\n", metar->dew_pt_temp);
    }

    if(metar->A_altstng) {
      mg_printf(nc, "        \"ALTIMETER (INCHES)\": %.2f\n", metar->inches_altstng );
    }

    if(metar->Q_altstng) {
      mg_printf(nc, "        \"ALTIMETER (PASCALS)\": %d\n", metar->hectoPasc_altstng );
    }

    //sprintf_tornadic_info (string, metar);

    if(metar->autoIndicator[0] != '\0') {
        mg_printf(nc, "        \"AUTO INDICATOR\": \"%s\"\n", metar->autoIndicator);
    }

    if(metar->PKWND_dir !=  INT_MAX) {
      mg_printf(nc, "        \"PEAK WIND DIRECTION\": %d\n",metar->PKWND_dir);
    }

    if(metar->PKWND_speed !=  INT_MAX) {
      mg_printf(nc, "        \"PEAK WIND SPEED\": %d\n",metar->PKWND_speed);
    }

    if(metar->PKWND_hour !=  INT_MAX) {
      mg_printf(nc, "        \"PEAK WIND HOUR\": %d\n",metar->PKWND_hour);
    }

    if(metar->PKWND_minute !=  INT_MAX) {
      mg_printf(nc, "        \"PEAK WIND MINUTE\": %d\n",metar->PKWND_minute);
    }

    if(metar->WshfTime_hour != INT_MAX) {
      mg_printf(nc, "        \"HOUR OF WIND SHIFT\": %d\n",metar->WshfTime_hour);
    }

    if(metar->WshfTime_minute != INT_MAX) {
      mg_printf(nc, "        \"MINUTE OF WIND SHIFT\": %d\n",metar->WshfTime_minute);
    }

    if(metar->Wshft_FROPA != false) {
      mg_printf(nc, "        \"FROPA ASSOC. W/WSHFT\": true\n");
    }

    if(metar->TWR_VSBY != (float) INT_MAX) {
      mg_printf(nc, "        \"TOWER VISIBILITY\": %.2f\n",metar->TWR_VSBY);
    }

    if(metar->SFC_VSBY != (float) INT_MAX) {
      mg_printf(nc, "        \"SURFACE VISIBILITY\": %.2f\n",metar->SFC_VSBY);
    }

    if(metar->minVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"MIN VRBL_VIS (SM)\": %.4f\n",metar->minVsby);
    }

    if(metar->maxVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"MAX VRBL_VIS (SM)\": %.4f\n",metar->maxVsby);
    }

    if( metar->VSBY_2ndSite != (float) INT_MAX) {
      mg_printf(nc, "        \"VSBY_2ndSite (SM)\": %.4f\n",metar->VSBY_2ndSite);
    }

    if( metar->VSBY_2ndSite_LOC[0] != '\0') {
      mg_printf(nc, "        \"VSBY_2ndSite LOC.\": \"%s\"\n", metar->VSBY_2ndSite_LOC);
    }

    if(metar->OCNL_LTG) {
      mg_printf(nc, "        \"OCCASSIONAL LTG\": true\n");
    }

    if(metar->FRQ_LTG) {
      mg_printf(nc, "        \"FREQUENT LIGHTNING\": true\n");
    }

    if(metar->CNS_LTG) {
      mg_printf(nc, "        \"CONTINUOUS LTG\": true\n");
    }

    if(metar->CG_LTG) {
      mg_printf(nc, "        \"CLOUD-GROUND LTG\": true\n");
    }

    if(metar->IC_LTG) {
      mg_printf(nc, "        \"IN-CLOUD LIGHTNING\": true\n");
    }

    if(metar->CC_LTG) {
      mg_printf(nc, "        \"CLD-CLD LIGHTNING\": true\n");
    }

    if(metar->CA_LTG) {
      mg_printf(nc, "        \"CLOUD-AIR LIGHTNING\": true\n");
    }

    if(metar->AP_LTG) {
      mg_printf(nc, "        \"LIGHTNING AT AIRPORT\": true\n");
    }

    if(metar->OVHD_LTG) {
      mg_printf(nc, "        \"LIGHTNING OVERHEAD\": true\n");
    }

    if(metar->DSNT_LTG) {
      mg_printf(nc, "        \"DISTANT LIGHTNING\": true\n");
    }

    if(metar->LightningVCTS) {
      mg_printf(nc, "        \"L'NING W/I 5-10(ALP)\": true\n");
    }

    if(metar->LightningTS) {
      mg_printf(nc, "        \"L'NING W/I 5 (ALP)\": true\n");
    }

    if(metar->VcyStn_LTG) {
      mg_printf(nc, "        \"VCY STN LIGHTNING\": true\n");
    }

    if(metar->LTG_DIR[0] != '\0') {
      mg_printf(nc, "        \"DIREC. OF LIGHTNING\": \"%s\"\n", metar->LTG_DIR);
    }

    i = 0;
    while((i < 3) && (metar->ReWx[ i ].Recent_weather[0] != '\0')) {
      mg_printf(nc, "        \"RECENT WEATHER\": \"%s\"", metar->ReWx[i].Recent_weather);

      if(metar->ReWx[i].Bhh != INT_MAX) {
         mg_printf(nc, "        \"BEG_hh\": %d",metar->ReWx[i].Bhh);
      }
      if(metar->ReWx[i].Bmm != INT_MAX) {
         mg_printf(nc, "        \"BEG_mm\": %d",metar->ReWx[i].Bmm);
      }

      if(metar->ReWx[i].Ehh != INT_MAX) {
         mg_printf(nc, "        \"END_hh\": %d",metar->ReWx[i].Ehh);
      }
      if(metar->ReWx[i].Emm != INT_MAX) {
         mg_printf(nc, "        \"END_mm\": %d",metar->ReWx[i].Emm);
      }

      i++;
    }

    if(metar->minCeiling != INT_MAX) {
      mg_printf(nc, "        \"MIN VRBL_CIG (FT)\": %d\n",metar->minCeiling);
    }

    if(metar->maxCeiling != INT_MAX) {
      mg_printf(nc, "        \"MAX VRBL_CIG (FT))\": %d\n",metar->maxCeiling);
    }

    if(metar->CIG_2ndSite_Meters != INT_MAX) {
      mg_printf(nc, "        \"CIG2ndSite (FT)\": %d\n",metar->CIG_2ndSite_Meters);
    }

    if(metar->CIG_2ndSite_LOC[0] != '\0') {
      mg_printf(nc, "        \"CIG @ 2nd Site LOC\": \"%s\"\n",metar->CIG_2ndSite_LOC);
    }

    if(metar->PRESFR) {
      mg_printf(nc, "        \"PRESFR\": true\n");
    }
    if(metar->PRESRR) {
      mg_printf(nc, "        \"PRESRR\": true\n");
    }

    if(metar->SLPNO) {
      mg_printf(nc, "        \"SLPNO\": true\n");
    }

    if(metar->SLP != (float) INT_MAX) {
      mg_printf(nc, "        \"SLP (hPa)\": %.1f\n", metar->SLP);
    }

    if(metar->SectorVsby != (float) INT_MAX) {
      mg_printf(nc, "        \"SECTOR VSBY (MILES)\": %.2f\n", metar->SectorVsby );
    }

    if(metar->SectorVsby_Dir[0] != '\0') {
      mg_printf(nc, "        \"SECTOR VSBY OCTANT\": \"%s\"\n", metar->SectorVsby_Dir );
    }

    if(metar->TS_LOC[0] != '\0') {
      mg_printf(nc, "        \"THUNDERSTORM LOCAT\": \"%s\"\n", metar->TS_LOC );
    }

    if(metar->TS_MOVMNT[0] != '\0') {
      mg_printf(nc, "        \"THUNDERSTORM MOVMNT\": \"%s\"\n", metar->TS_MOVMNT);
    }

    if(metar->GR) {
      mg_printf(nc, "        \"GR (HAILSTONES)\": true\n");
    }

    if(metar->GR_Size != (float) INT_MAX) {
      mg_printf(nc, "        \"HLSTO SIZE (INCHES)\": %.3f\n",metar->GR_Size);
    }

    if(metar->VIRGA) {
      mg_printf(nc, "        \"VIRGA\": true\n");
    }

    if(metar->VIRGA_DIR[0] != '\0') {
      mg_printf(nc, "        \"DIR OF VIRGA FRM STN\": \"%s\"\n", metar->VIRGA_DIR);
    }

    for(i = 0; i < MAX_SURFACE_OBSCURATIONS; i++) {
      if( metar->SfcObscuration[i][0] != '\0') {
         mg_printf(nc, "        \"SfcObscuration\": \"%s\"\n", metar->SfcObscuration[i]);
      }
    }

    if(metar->Num8thsSkyObscured != INT_MAX) {
      mg_printf(nc, "        \"8ths of SkyObscured\": %d\n",metar->Num8thsSkyObscured);
    }

    if(metar->CIGNO) {
      mg_printf(nc, "        \"CIGNO\": true\n");
    }

    if(metar->Ceiling != INT_MAX) {
      mg_printf(nc, "        \"Ceiling (ft)\": %d\n",metar->Ceiling);
    }

    if(metar->Estimated_Ceiling != INT_MAX) {
      mg_printf(nc, "        \"Estimated CIG (ft)\": %d\n",metar->Estimated_Ceiling);
    }

    if(metar->VrbSkyBelow[0] != '\0') {
      mg_printf(nc, "        \"VRB SKY COND BELOW\": \"%s\"\n",metar->VrbSkyBelow);
    }

    if(metar->VrbSkyAbove[0] != '\0') {
      mg_printf(nc, "        \"VRB SKY COND ABOVE\": \"%s\"\n",metar->VrbSkyAbove);
    }

    if(metar->VrbSkyLayerHgt != INT_MAX) {
      mg_printf(nc, "        \"VRBSKY COND HGT (FT)\": %d\n",metar->VrbSkyLayerHgt);
    }

    if(metar->ObscurAloftHgt != INT_MAX) {
      mg_printf(nc, "        \"Hgt Obscur Aloft(ft)\": %d\n",metar->ObscurAloftHgt);
    }

    if(metar->ObscurAloft[0] != '\0') {
      mg_printf(nc, "        \"Obscur Phenom Aloft\": \"%s\"\n",metar->ObscurAloft);
    }

    if(metar->ObscurAloftSkyCond[0] != '\0') {
      mg_printf(nc, "        \"Obscur ALOFT SKYCOND\": \"%s\"\n",metar->ObscurAloftSkyCond);
    }

    if(metar->NOSPECI) {
      mg_printf(nc, "        \"NOSPECI\": true\n");
    }

    if(metar->LAST) {
      mg_printf(nc, "        \"LAST\": true\n");
    }

    if(metar->synoptic_cloud_type[0] != '\0') {
      mg_printf(nc, "        \"SYNOPTIC CLOUD GROUP\": \"%s\"\n",metar->synoptic_cloud_type);
    }

    if(metar->CloudLow != '\0') {
      mg_printf(nc, "        \"LOW CLOUD CODE\": %c\n",metar->CloudLow);
    }

    if(metar->CloudMedium != '\0') {
      mg_printf(nc, "        \"MEDIUM CLOUD CODE\": %c\n",metar->CloudMedium);
    }

    if(metar->CloudHigh != '\0') {
      mg_printf(nc, "        \"HIGH CLOUD CODE\": %c\n",metar->CloudHigh);
    }

    if(metar->SNINCR != INT_MAX) {
      mg_printf(nc, "        \"SNINCR (INCHES)\": %d\n",metar->SNINCR);
    }

    if(metar->SNINCR_TotalDepth != INT_MAX) {
      mg_printf(nc, "        \"SNINCR(TOT. INCHES)\": %d\n",metar->SNINCR_TotalDepth);
    }

    if(metar->snow_depth_group[0] != '\0') {
      mg_printf(nc, "        \"SNOW DEPTH GROUP\": \"%s\"\n",metar->snow_depth_group);
    }

    if(metar->snow_depth != INT_MAX) {
      mg_printf(nc, "        \"SNOW DEPTH (INCHES)\": %d\n",metar->snow_depth);
    }

    if(metar->WaterEquivSnow != (float) INT_MAX) {
      mg_printf(nc, "        \"H2O EquivSno(inches)\": %.2f\n",metar->WaterEquivSnow);
    }

    if(metar->SunshineDur != INT_MAX) {
      mg_printf(nc, "        \"SUNSHINE (MINUTES)\": %d\n",metar->SunshineDur);
    }

    if(metar->SunSensorOut) {
      mg_printf(nc, "        \"SUN SENSOR OUT\": true\n");
    }

    if(metar->hourlyPrecip != (float) INT_MAX) {
      mg_printf(nc, "        \"HRLY PRECIP (INCHES)\": %.2f\n",metar->hourlyPrecip);
    }

    if( metar->precip_amt != (float) INT_MAX) {
      mg_printf(nc, "        \"3/6HR PRCIP (INCHES)\": %.2f\n", metar->precip_amt);
    }

    if( metar->Indeterminant3_6HrPrecip) {
      mg_printf(nc, "        \"INDTRMN 3/6HR PRECIP\": true\n");
    }

    if( metar->precip_24_amt !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24HR PRECIP (INCHES)\": %.2f\n", metar->precip_24_amt);
    }

    if(metar->Indeterminant_24HrPrecip) {
      mg_printf(nc, "        \"INDTRMN 24 HR PRECIP\": true\n");
    }

    if(metar->Temp_2_tenths != (float) INT_MAX) {
      mg_printf(nc, "        \"TMP2TENTHS (CELSIUS)\": %.1f\n",metar->Temp_2_tenths);
    }

    if(metar->DP_Temp_2_tenths != (float) INT_MAX) {
      mg_printf(nc, "        \"DPT2TENTHS (CELSIUS)\": %.1f\n",metar->DP_Temp_2_tenths);
    }

    if(metar->maxtemp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"MAX TEMP (CELSIUS)\": %.1f\n", metar->maxtemp);
    }

    if(metar->mintemp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"MIN TEMP (CELSIUS)\": %.1f\n", metar->mintemp);
    }

    if(metar->max24temp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24HrMAXTMP (CELSIUS)\": %.1f\n", metar->max24temp);
    }

    if(metar->min24temp !=  (float) INT_MAX) {
      mg_printf(nc, "        \"24HrMINTMP (CELSIUS)\": %.1f\n", metar->min24temp);
    }

    if(metar->char_prestndcy != INT_MAX) {
      mg_printf(nc, "        \"CHAR PRESS TENDENCY\": %d\n", metar->char_prestndcy );
    }

    if(metar->prestndcy != (float) INT_MAX) {
      mg_printf(nc, "        \"PRES. TENDENCY (hPa)\": %.1f\n", metar->prestndcy );
    }

    if(metar->PWINO) {
      mg_printf(nc, "        \"PWINO\": true\n");
    }

    if(metar->PNO) {
      mg_printf(nc, "        \"PNO\": true\n");
    }

    if(metar->CHINO) {
      mg_printf(nc, "        \"CHINO\": true\n");
    }

    if(metar->CHINO_LOC[0] != '\0') {
      mg_printf(nc, "        \"CHINO_LOC\": \"%s\"\n",metar->CHINO_LOC);
    }

    if(metar->VISNO) {
      mg_printf(nc, "        \"VISNO\": true\n");
    }

    if(metar->VISNO_LOC[0] != '\0') {
      mg_printf(nc, "        \"VISNO_LOC\": \"%s\"\n",metar->VISNO_LOC);
    }

    if(metar->FZRANO) {
      mg_printf(nc, "        \"FZRANO\": true\n");
    }

    if(metar->TSNO) {
      mg_printf(nc, "        \"TSNO\": true\n");
    }

    if(metar->DollarSign) {
      mg_printf(nc, "        \"DOLLAR SIGN INDCATR\": true\n");
    }

    if(metar->horiz_vsby[0] != '\0') {
      mg_printf(nc, "        \"HORIZ VISIBILITY\": \"%s\"\n",metar->horiz_vsby);
    }

    if(metar->dir_min_horiz_vsby[0] != '\0') {
      mg_printf(nc, "        \"DIR MIN HORIZ VSBY\": \"%s\"\n",metar->dir_min_horiz_vsby);
    }

    if(metar->CAVOK) {
      mg_printf(nc, "        \"CAVOK\": true\n");
    }

    if( metar->VertVsby != INT_MAX) {
      mg_printf(nc, "        \"Vert. Vsby (meters)\": %d\n", metar->VertVsby );
    }

    /*
    if( metar->charVertVsby[0] != '\0' )
      mg_printf(nc, "        \"Vert. Vsby (CHAR)   : \"%s\"\n",
                  metar->charVertVsby );
    */

    if(metar->QFE != INT_MAX) {
      mg_printf(nc, "        \"QFE\": %d\n", metar->QFE);
    }

    if(metar->VOLCASH) {
      mg_printf(nc, "        \"VOLCANIC ASH\": true\n");
    }

    if(metar->min_vrbl_wind_dir != INT_MAX) {
      mg_printf(nc, "        \"MIN VRBL WIND DIR\": %d\n",metar->min_vrbl_wind_dir);
    }

    if(metar->max_vrbl_wind_dir != INT_MAX) {
      mg_printf(nc, "        \"MAX VRBL WIND DIR\": %d\n",metar->max_vrbl_wind_dir);
    }
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
                    decode_metar_success = decode_metar(database_column_text(i), &metar);
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

                // If this is the last column, then don't put a comma!
                if (i < numColumns - 1) {
                    mg_printf(nc, ",\n");
                } else {
                    mg_printf(nc, "\n");
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
