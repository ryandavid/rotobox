#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "archive.h"
#include "archive_entry.h"
#include "cpl_conv.h"
#include "gdal.h"
#include "gdal_utils.h"

#include "database.h"
#include "download.h"

CURL *curl;
CURLcode res;

struct archive *a;
struct archive_entry *entry;
int r;

void download_init() {
    fprintf(stdout, "Curl Version: %s\n", curl_version());
    fprintf(stdout, "Libarchive Version: %s\n", archive_version_details());
    fprintf(stdout, "GDAL Version: %s\n", GDALVersionInfo("RELEASE_NAME"));
    curl = curl_easy_init();
}

void download_cleanup() {
    curl_easy_cleanup(curl);
}

static bool determine_56day_subscription_date(struct tm* t) {
    // We know September 15, 2016 as a valid subscription effective date.
    const int start_year = 2016;
    const int start_month = 7;
    const int start_day = 21;

    const int subscription_length_days = 56;

    time_t now;
    time(&now);

    t->tm_year = start_year - 1900;
    t->tm_mon = start_month - 1;
    t->tm_mday = start_day;
    t->tm_hour = 0;
    t->tm_min = 0;
    t->tm_sec = 0;
    t->tm_isdst = -1;

    while(difftime(now, mktime(t)) > 0) {
        t->tm_mday += subscription_length_days;
    }

    // We need to roll back one subscription interval.
    t->tm_mday -= subscription_length_days;
    mktime(t);

    return true;
}

// Find the current AIRAC cycle number (ie, 1611) as an integer.
static int determine_airac_cycle() {
    // We know Jan 7, 2016 as a valid start (1601).
    const int start_year = 2016;
    const int start_month = 1;
    const int start_day = 7;
    const int start_airac_index = 1;

    // Length on one AIRAC cycle
    const int airac_length_days = 28;

    time_t now;
    time(&now);
    /*struct tm future;
    future.tm_year = 2020 - 1900;
    future.tm_mon = 8 - 1;
    future.tm_mday = 14;
    future.tm_hour = 0;
    future.tm_min = 0;
    future.tm_sec = 0;
    future.tm_isdst = -1;
    now = mktime(&future);*/

    struct tm t;

    t.tm_year = start_year - 1900;
    t.tm_mon = start_month - 1;
    t.tm_mday = start_day;
    t.tm_hour = 0;
    t.tm_min = 0;
    t.tm_sec = 0;
    t.tm_isdst = -1;

    int airac_index = start_airac_index - 1;
    int last_year = t.tm_year;
    int last_airac_index = airac_index;

    // Difftime is a double containing the number of seconds, hence the comparison against -1.
    while(difftime(now, mktime(&t)) > -1.0f) {
        // If we've rolled over to the next year, snapshot the previous year's last index
        // just in case we need to roll back to it.
        if(t.tm_year > last_year) {
            last_year = t.tm_year;
            last_airac_index = airac_index;
            airac_index = 0;
        }

        t.tm_mday += airac_length_days;
        airac_index++;
    }

    // We need to roll back one subscription interval.
    t.tm_mday -= airac_length_days;
    mktime(&t);

    // If we crossed back over the year boundary, use the cycle number from last year.
    if(t.tm_year < last_year) {
        airac_index = last_airac_index;
    }

    return ((t.tm_year - 100) * 100) + airac_index;
}

// https://github.com/libarchive/libarchive/wiki/Examples#Constructing_Objects_On_Disk
static int copy_data(struct archive *ar, struct archive *aw) {
    int r;
    const void *buff;
    size_t size;
    off_t offset;

    while(true) {
        r = archive_read_data_block(ar, &buff, &size, &offset);
        if(r == ARCHIVE_EOF) {
            return (ARCHIVE_OK);
        }

        if(r < ARCHIVE_OK) {
            return (r);
        }

        r = archive_write_data_block(aw, buff, size, offset);
        if(r < ARCHIVE_OK) {
            fprintf(stderr, "%s\n", archive_error_string(aw));
            return (r);
        }
    }
}

// https://github.com/libarchive/libarchive/wiki/Examples#Constructing_Objects_On_Disk
static bool extract_archive(const char* filename, const char* basefolder) {
    struct archive *a;
    struct archive *ext;
    struct archive_entry *entry;
    int flags;
    int r;

    char tempFilename[1024];

    /* Select which attributes we want to restore. */
    flags = ARCHIVE_EXTRACT_TIME;
    flags |= ARCHIVE_EXTRACT_PERM;
    flags |= ARCHIVE_EXTRACT_ACL;
    flags |= ARCHIVE_EXTRACT_FFLAGS;

    a = archive_read_new();
    archive_read_support_format_all(a);
    //archive_read_support_compression_all(a);

    ext = archive_write_disk_new();
    archive_write_disk_set_options(ext, flags);
    archive_write_disk_set_standard_lookup(ext);

    if((r = archive_read_open_filename(a, filename, 10240))) {
        return false;
    }

    while(true) {
        r = archive_read_next_header(a, &entry);
        if(r == ARCHIVE_EOF) {
            break;
        }
        if(r < ARCHIVE_OK) {
            fprintf(stderr, "%s\n", archive_error_string(a));
        }
        if(r < ARCHIVE_WARN) {
            return false;
        }

        snprintf(&tempFilename[0], sizeof(tempFilename), "%s/%s", basefolder, archive_entry_pathname(entry));
        archive_entry_set_pathname(entry, &tempFilename[0]);

        r = archive_write_header(ext, entry);
        if(r < ARCHIVE_OK){
            fprintf(stderr, "%s\n", archive_error_string(ext));
        } else if(archive_entry_size(entry) > 0) {
            r = copy_data(a, ext);
            if(r < ARCHIVE_OK) {
                fprintf(stderr, "%s\n", archive_error_string(ext));
            }
                
            if(r < ARCHIVE_WARN) {
                return false;
            }
        }

        r = archive_write_finish_entry(ext);
        if (r < ARCHIVE_OK) {
            fprintf(stderr, "%s\n", archive_error_string(ext));
        }
        if (r < ARCHIVE_WARN) {
            return false;
        }
    }

    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);

    return true;
}

static char* filename_from_url(char* url) {
    char* lastForwardSlash = NULL;
    char* ret = url;

    while(ret != NULL) {
        lastForwardSlash = ret + 1;
        ret = strstr(lastForwardSlash, "/");
    }
    
    return lastForwardSlash;
}

void download_updates(const char* product) {
    char temp_url[1024];
    char download_filepath[1024];
    char temp_filepath[1024];
    char current_working_dir[256];

    // Figure out where we are.  TODO: Update this to the path of the binary, not the pwd.
    getcwd(&current_working_dir[0], sizeof(current_working_dir));

    if(strncmp("airspaces", product, strlen("airspaces")) == 0) {
        struct tm t;

        // Do not add the extension here.
        const char *expected_airspace_files[] = {
            "class_b",
            "class_c",
            "class_d",
            "class_e0",
            "class_e5",
        };

        // Assemble the correct URL by determining the current cycle date.
        determine_56day_subscription_date(&t);
        strftime(&temp_url[0], sizeof(temp_url), "https://nfdc.faa.gov/webContent/56DaySub/%F/class_airspace_shape_files.zip", &t);
        snprintf(&download_filepath[0], sizeof(download_filepath), "%s/download/%s", &current_working_dir[0], filename_from_url(&temp_url[0]));

        download_file(&temp_url[0], &download_filepath[0]);
        fprintf(stdout, "Downloaded '%s'\n", download_filepath);

        extract_archive(download_filepath, "download");
        fprintf(stdout, "Extracted '%s'\n", download_filepath);


        database_empty_table("airspaces");
        // Hacky. TODO: Make the filepath handling more elegant and not so hardcoded.
        for(size_t i = 0; i < (sizeof(expected_airspace_files) / sizeof(expected_airspace_files[0])); i++) {
            snprintf(&temp_filepath[0], sizeof(temp_filepath), "%s/download/Shape_Files/%s", &current_working_dir[0], expected_airspace_files[i]);
            if(database_load_airspace_shapefile(temp_filepath, (char*)expected_airspace_files[i]) == true) {
                fprintf(stdout, "Successfully added '%s' shapefile.\n", expected_airspace_files[i]);
            } else {
                fprintf(stdout, "Failed creating '%s' shapefile!\n", expected_airspace_files[i]);
            }
        }
    } else if(strncmp("charts", product, strlen("charts")) == 0) {
        int airac_cycle = determine_airac_cycle();
        fprintf(stdout, "Current AIRAC cycle: %d\n", airac_cycle);

        char* temp_url = "http://aeronav.faa.gov/content/aeronav/sectional_files/San_Francisco_97.zip";
        char download_filepath[1024];
        snprintf(&download_filepath[0], sizeof(download_filepath), "%s/download/%s", &current_working_dir[0], filename_from_url(&temp_url[0]));
        //download_file(&temp_url[0], &download_filepath[0]);
        //extract_archive(download_filepath, "download");

    } else if(strncmp("test", product, strlen("test")) == 0) {
        const char* sourceTif = "/Users/ryan/src/rotobox/download/SanFranciscoSEC97.tif";
        const char* shapeFile = "/Users/ryan/src/rotobox/chart_clipping_layers/sectional/San_Francisco_SEC.shp";
        const char* outFile = "/Users/ryan/Desktop/out.tif";
        GDALAllRegister();

        GDALDatasetH srcDataset = GDALOpen(sourceTif, GA_ReadOnly);
        GDALDatasetH destDataset;
        int error = 0;

        char* args[] = {
            "-multi",
            "-dstnodata",
            "0",
            "-crop_to_cutline",
            "-overwrite",
            "-cutline",
            shapeFile,
            NULL
        };

        GDALWarpAppOptions* options = GDALWarpAppOptionsNew(&args[0], NULL);

        fprintf(stdout, "Running! \n");
        destDataset = GDALWarp(outFile, NULL, 1, &srcDataset, options, &error);

        /*GDALDriverH hDriver;
        double adfGeoTransform[6];

        srcDataset = GDALOpen(sourceTif, GA_ReadOnly);
        hDriver = GDALGetDatasetDriver(srcDataset);
        fprintf(stdout, "Driver: %s/%s\n", GDALGetDriverShortName(hDriver), GDALGetDriverLongName(hDriver) );
        fprintf(stdout, "Size is %dx%dx%d\n",
                GDALGetRasterXSize(srcDataset),
                GDALGetRasterYSize(srcDataset),
                GDALGetRasterCount(srcDataset));
        if(GDALGetProjectionRef(srcDataset) != NULL ) {
            fprintf(stdout, "Projection is `%s'\n", GDALGetProjectionRef(srcDataset));
        }
        if(GDALGetGeoTransform(srcDataset, adfGeoTransform) == CE_None) {
            fprintf(stdout, "Origin = (%.6f,%.6f)\n", adfGeoTransform[0], adfGeoTransform[3]);
            fprintf(stdout, "Pixel Size = (%.6f,%.6f)\n", adfGeoTransform[1], adfGeoTransform[5]);
        }*/

        fprintf(stdout, "GDALWarp returned %d\n", error);

        GDALWarpAppOptionsFree(options);
        GDALClose(srcDataset);
        GDALClose(destDataset);
    } else {
        fprintf(stdout, "Unknown product specified: %s\n", product);
    }
}

bool download_file(char* url, char* filepath) {
    bool success = false;
    FILE * fp;

    fp = fopen(filepath, "w");

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    res = curl_easy_perform(curl);
    if(res != CURLE_OK) {
        fprintf(stdout, "Failed to download '%s'! Error Code = %s\n", filepath, curl_easy_strerror(res));
    } else {
        success = true;
    }
    fclose(fp);
    return success;
}




