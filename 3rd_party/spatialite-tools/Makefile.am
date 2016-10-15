ACLOCAL_AMFLAGS = -I m4

if NO_READOSM
bin_PROGRAMS = spatialite \
	spatialite_tool \
	spatialite_network \
	shp_doctor \
	exif_loader \
	spatialite_osm_filter \
	spatialite_gml \
	spatialite_convert \
	spatialite_dxf \
	spatialite_xml_validator \
	spatialite_xml_load \
	spatialite_xml_collapse \
	spatialite_xml_print \
	spatialite_osm_overpass
else
bin_PROGRAMS = spatialite \
	spatialite_tool \
	spatialite_network \
	shp_doctor \
	exif_loader \
	spatialite_osm_net \
	spatialite_osm_map \
	spatialite_osm_raw \
	spatialite_osm_filter \
	spatialite_gml \
	spatialite_convert \
	spatialite_dxf \
	spatialite_xml_validator \
	spatialite_xml_load \
	spatialite_xml_collapse \
	spatialite_xml_print \
	spatialite_osm_overpass
endif

AM_CPPFLAGS = @CFLAGS@ @LIBXML2_CFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)

spatialite_SOURCES = shell.c
spatialite_tool_SOURCES = spatialite_tool.c
spatialite_network_SOURCES = spatialite_network.c
shp_doctor_SOURCES = shp_doctor.c
exif_loader_SOURCES = exif_loader.c
spatialite_xml_validator_SOURCES = spatialite_xml_validator.c
spatialite_xml_load_SOURCES = spatialite_xml_load.c
spatialite_xml_collapse_SOURCES = spatialite_xml_collapse.c
spatialite_xml_print_SOURCES = spatialite_xml_print.c 
if NO_READOSM
else
spatialite_osm_net_SOURCES = spatialite_osm_net.c
spatialite_osm_map_SOURCES = spatialite_osm_map.c
spatialite_osm_raw_SOURCES = spatialite_osm_raw.c
endif
spatialite_osm_filter_SOURCES = spatialite_osm_filter.c
spatialite_gml_SOURCES = spatialite_gml.c
spatialite_osm_overpass_SOURCES = spatialite_osm_overpass.c

spatialite_osm_map_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ @LIBREADOSM_LIBS@ 
spatialite_osm_overpass_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ @LIBXML2_LIBS@ 
spatialite_osm_raw_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ @LIBREADOSM_LIBS@
spatialite_osm_net_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ @LIBREADOSM_LIBS@
spatialite_gml_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ \
	-lexpat 
spatialite_LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@ \
	@READLINE_LIBS@
spatialite_xml_validator_LDADD = @LIBXML2_LIBS@
spatialite_xml_load_LDADD = @LIBSPATIALITE_LIBS@ \
	-lexpat
spatialite_xml_collapse_LDADD = @LIBSPATIALITE_LIBS@
spatialite_xml_print_LADD = @LIBSPATIALIATE_LIBS@
LDADD = @LIBSPATIALITE_LIBS@ \
	@LIBFREEXL_LIBS@

EXTRA_DIST = makefile.vc nmake.opt \
	config-msvc.h \
	Makefile-static-MinGW \
	Makefile-static-Linux \
	Makefile-static-MacOsX

AUTOMAKE_OPTIONS = dist-zip foreign

