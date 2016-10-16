#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "archive.h"
#include "archive_entry.h"

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

    fprintf(stdout, "Working on updating the following product: %s\n", product);
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

        // Figure out where we are.
        getcwd(&current_working_dir[0], sizeof(current_working_dir));

        // Assemble the correct URL by determining the current cycle date.
        determine_56day_subscription_date(&t);
        strftime(&temp_url[0], sizeof(temp_url), "https://nfdc.faa.gov/webContent/56DaySub/%F/class_airspace_shape_files.zip", &t);
        snprintf(&download_filepath[0], sizeof(download_filepath), "%s/download/%s", &current_working_dir[0], filename_from_url(&temp_url[0]));

        //download_file(&temp_url[0], &download_filepath[0]);
        fprintf(stdout, "Downloaded '%s'\n", download_filepath);

        //extract_archive(download_filepath, "download");
        fprintf(stdout, "Extracted '%s'\n", download_filepath);


        database_empty_table("airspaces");
        // Hacky. TODO: Make the filepath handling more elegant and not so hardcoded.
        for(size_t i = 0; i < (sizeof(expected_airspace_files) / sizeof(expected_airspace_files[0])); i++) {
            snprintf(&temp_filepath[0], sizeof(temp_filepath), "%s/download/Shape_Files/%s", &current_working_dir[0], expected_airspace_files[i]);
            if(database_load_airspace_shapefile(temp_filepath, expected_airspace_files[i]) == true) {
                fprintf(stdout, "Successfully added '%s' shapefile.\n", expected_airspace_files[i]);
            } else {
                fprintf(stdout, "Failed creating '%s' shapefile!\n", expected_airspace_files[i]);
            }
        }
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




