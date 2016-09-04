var markerArray = new Array();
var runwayArray = new Array();
var map;
var ownshipIcon;

const airspace_styles = {
  "class_b": {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#2464A9",
    "fillOpacity": 0.1,
    "dashArray": ""
  },
  "class_c": {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#7A2B51",
    "fillOpacity": 0.1,
    "dashArray": ""
  },
  "class_d": {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#0C3573",
    "fillOpacity": 0.1,
    "dashArray": "10, 5"
  },
  "class_e": {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#741646",
    "fillOpacity": 0.1,
    "dashArray": "0.9"
  }
}

function recenter_map_on_current_position(){
  rotobox_api(API_URI_LOCATION, {}, function(data) {
    var marker = L.marker([data.latitude, data.longitude], {icon: ownshipIcon}).addTo(map);
    map.setView([data.latitude, data.longitude], 10);
  });
}

function search_for_nearest_airports() {
  rotobox_api(API_AIRPORT_NEAREST, {}, function(results){
    sidebar_showAirportSearchResults(results);
  });
}

function show_large_AD(){
  $('#airport-diagram-modal').modal();
}

function update_airport_markers(data){
    // Clear out the old markers
    for (var i = 0; i < markerArray.length; i++) {
        map.removeLayer(markerArray[i]);
    }
    markerArray.length = 0;

    // Setup our marker for the airports
    // Size dynamically based on zoom
    var dynamicSize = (map.getZoom() - 8) * 15;
    var airportIcon = L.divIcon({
        className: 'airport-icon',
        iconAnchor: [dynamicSize / 2, dynamicSize / 2],
        iconSize: [dynamicSize, dynamicSize]
    });

    // Add the markers in this viewport
    for (var i = 0; i < data.length; i++) {
        var marker = L.marker([data[i].latitude, data[i].longitude],
            {
                icon: airportIcon,
                title: data[i].facility_name,
                airport_id: data[i].id
            });
        marker.on("click", function(){
            sidebar_showAirportResult(this.options.airport_id, false);
        })
        markerArray.push(marker);
        map.addLayer(marker);
    }
}

function text_search_airports(query){
  rotobox_api(API_AIRPORT_SEARCH, {"name": query}, function(results){
    if(results.length == 1) {
      sidebar_showAirportResult(results[0].id, true);
    } else {
      sidebar_showAirportSearchResults(results);
    }
  });
}

// Referenced http://leafletjs.com/examples/choropleth.html
function highlight_airspace(e) {
  var layer = e.target;

  layer.setStyle({
    fillOpacity: 0.2
  });
}

function reset_airspace(e) {
  var layer = e.target;

  layer.setStyle({
    fillOpacity: 0.1
  });
}

function on_each_airspace(feature, layer) {
  layer.on({
    mouseover: highlight_airspace,
    mouseout: reset_airspace,
    click: sidebar_showAirspaceDetail
  });
}

function map_init(){
  /* **** Leaflet **** */
  L.Icon.Default.imagePath = 'img';
  var lyr = L.tileLayer('./charts/{z}/{x}/{y}.png', {tms: true, attribution: ""});

  map = L.map('map', {
      center: [38.248181295, -121.789805051],
      zoom: 5,
      minZoom: 1,
      maxZoom: 11,
      layers: [lyr],
      attributionControl: false
  });

  ownshipIcon = L.icon({
      iconUrl: 'img/helicopter-top.png',
      iconSize: [48, 48],
      iconAnchor: [24, 24],
  });

  // Button below zoom buttons to re-center the map on the current location
  var centerButton =  L.Control.extend({
    options: {
      position: 'topleft'
    },

    onAdd: function (map) {
      var container = L.DomUtil.create('div', 'leaflet-bar leaflet-control leaflet-control-custom');

      container.style.backgroundColor = 'white';     
      container.style.backgroundImage = "url(img/center.png)";
      container.style.backgroundSize = "16px 16px";
      container.style.backgroundRepeat = "no-repeat";
      container.style.backgroundPosition = "50% 50%";
      container.style.width = '26px';
      container.style.height = '26px';

      container.onclick = function(){
        recenter_map_on_current_position();
      }

      return container;
    }
  });
  map.addControl(new centerButton());

  // Button to show nearest airports
  var nearestButton =  L.Control.extend({
    options: {
      position: 'topleft'
    },

    onAdd: function (map) {
      var container = L.DomUtil.create('div', 'leaflet-bar leaflet-control leaflet-control-custom');

      container.style.backgroundColor = 'white';     
      container.style.backgroundImage = "url(img/directions.png)";
      container.style.backgroundSize = "16px 16px";
      container.style.backgroundRepeat = "no-repeat";
      container.style.backgroundPosition = "50% 50%";
      container.style.width = '26px';
      container.style.height = '26px';

      container.onclick = function(){
        search_for_nearest_airports();
      }

      return container;
    }
  });
  map.addControl(new nearestButton());

  // Reload airport markers when the user pans/zooms.
  map.on('moveend', function(){
    ROUND_DECIMAL_PLACES = 1000;
    if(map.getZoom() >= 9) {
        bounds = map.getBounds();
        zoom = map.getZoom();

        rotobox_api(API_AIRPORT_WINDOW,
          {
              "latMin": Math.round(bounds.getSouth() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES,
              "latMax": Math.round(bounds.getNorth() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES,
              "lonMin": Math.round(bounds.getWest() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES,
              "lonMax": Math.round(bounds.getEast() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES,
          },
          update_airport_markers);
    }
  });

  // Search box on map sidebar.
  $("input.map-search").keyup(function(e){
    if(e.keyCode == 13) {
      var query = $(this).val();
      text_search_airports(query);

      // Clear the text. TODO: Maybe be smarter about when we do this?
      $(this).val("");
    }
  });

  // Start by centering the map on the current location.
  recenter_map_on_current_position();

  // Display airspace shapefiles
  // Skip Class-E5 ('class_e5') because IMHO not very useful.
  var available_airspaces = ["class_b", "class_c", "class_d", "class_e"];
  var layerCtrl = L.control.layers(null, null).addTo(map);

  for (var i = 0; i < available_airspaces.length; i++) {
    rotobox_api(API_AIRSPACE_GEOJSON, {"class": available_airspaces[i]}, function(result) {
      var airspace = L.layerGroup()
      for (var i = 0; i < result.length; i++) {
        L.geoJson(result[i].geometry, {
          style: airspace_styles[result[i].type],
          onEachFeature: on_each_airspace,
          properties: {
            "name": result[i].name,
            "airspace": result[i].airspace,
            "low_alt": result[i].low_alt,
            "high_alt": result[i].high_alt,
            "type": result[i].type
          }
        }).addTo(airspace);
      }
      if(result.length > 0) {
        layerCtrl.addOverlay(airspace, result[0].type); // TODO: Be smarter about the name.
      }
    });
  }
}

function sidebar_showAirportSearchResults(results) {
  if((results.length == 0) || (results == {})) {
    var html = $("#sidebar-airportResultNone").render();
    $("div.sidebar-scrollable").empty().append(html);
  } else {
    var html = $("#sidebar-airportResultList").render();
    $("div.sidebar-scrollable").empty().append(html);
    $("p.result-list-num-results").text("Found " + results.length + " results:");

    for (var i = 0; i < results.length; i++) {
      var html = $("#sidebar-airportResultListItem").render(results[i]);
      $("div.list-group").append(html);
    }
  }
}

function sidebar_showAirspaceDetail(properties) {
  var html = $("#sidebar-airspaceDescription").render(properties.target.options.properties);
  $("div.sidebar-scrollable").empty().append(html);
}

function sidebar_showAirportResult(airport_id, center){
  // TODO: Actually use the rendering functionality
  var html = $("#sidebar-airportResult").render();
  $("div.sidebar-scrollable").empty().append(html);

  rotobox_api(API_AIRPORT_ID, {"id": airport_id}, function(data){
    $("#airport-name").text(data[0].facility_name);
    if(data[0].icao_identifier == "(null)") {
      $("#airport-identifiers").text(data[0].location_identifier);
    } else {
      $("#airport-identifiers").text(data[0].icao_identifier + " (" + data[0].location_identifier + ")");
    }
    $("dd.field-elevation").text(data[0].elevation + "'");
    //$("#airport-remarks").text(data[0].remarks);

    $("h5.airport-tags").empty();
    // TODO: Fix bool values having to be strings
    if(data[0].tower_onsite == "1") {
      $("h5.airport-tags").append("<span class='label label-primary'>Towered</span>\n");
    } else {
      $("h5.airport-tags").append("<span class='label label-default'>Nontowered</span>\n");
    }

    if(data[0].segmented_circle == "1") {
      $("h5.airport-tags").append("<span class='label label-info'>Segmented Circle</span>\n");
    }

    if(data[0].wind_indicator != "N") {
      $("h5.airport-tags").append("<span class='label label-info'>Windsock</span>\n");
    }

    if(data[0].facility_use != "PU") {
      $("h5.airport-tags").append("<span class='label label-danger'>Private</span>\n");
    }

    if(center == true){
      map.setView([data[0].latitude, data[0].longitude], 10);
    }
  });

  // Clear out the previous runways
  for (var i = 0; i < runwayArray.length; i++) {
      map.removeLayer(runwayArray[i]);
  }
  runwayArray.length = 0;
  $("ul#airport-runways").empty();

  rotobox_api(API_AIRPORT_RUNWAYS, {"id": airport_id}, function(data){
    for (var i = 0; i < data.length; i++) {
      var item = "<li class='list-group-item'>" + data[i].name
      if((data[i].length != "(null)") && (data[i].width != "(null)")){
        item += "<span class='badge'>" + Math.round(data[i].width) + "' x " + Math.round(data[i].length) + "'</span>\n";
      }

      //base_rh_traffic
      //recip_rh_traffic
      //if(data[i].right_traffic_pattern != "1"){
      //  item += "<span class='badge rp-badge'>RP</span>\n";
      //}
      item += "</li>\n";
      $("ul#airport-runways").append(item);

      // Lets also draw the runways if available.
      if((data[i].base_latitude != "(null)") && (data[i].base_latitude != "(null)") &&
        (data[i].recip_latitude != "(null)") && (data[i].recip_latitude != "(null)")) {
        var points = [
          new L.LatLng(data[i].base_latitude, data[i].base_longitude),
          new L.LatLng(data[i].recip_latitude, data[i].recip_longitude)
        ];

        var options = {
          color: "#000000",
          weight: 6,
          opacity: 0.7,
          lineCap: "butt"
        }

        var polyline = new L.Polyline(points, options);
        runwayArray.push(polyline)
        map.addLayer(polyline);
      }
    }
  });

  rotobox_api(API_AIRPORT_RADIO, {"id": airport_id}, function(data){
    if(data.length != 0) {
      $("dd.ctaf-frequency").text(data[0].ctaf_freq);
      $("dd.unicom-frequency").text(data[0].unicom_freq);
      $("dd.awos-frequency").text(data[0].awos_freq + " (" + data[0].awos_phone + ")");
    }
  });

  rotobox_api(API_AIRPORT_DIAGRAMS, {"id": airport_id}, function(data){
    var found = false;
    for (var i = 0; i < data.length; i++) {
      if(data[i].chart_name == "AIRPORT DIAGRAM"){
        var svg_url = "airports/" + data[i].filename;
        $("img.airport-diagram").attr("src", svg_url);
        $("img.airport-diagram-large").attr("src", svg_url);
        $("div.airport-diagram").show();
        found = true;
        break;
      }
    }

    if(found == false) {
      $("div.airport-diagram").hide();
    }
  });

  $("div.sidebar-scrollable").scrollTop(0);
}

