var markerArray = new Array();
var map;
var ownshipIcon;

function recenter_map_on_current_position(){
  rotobox_api(API_URI_LOCATION, {}, function(data) {
    var marker = L.marker([data.latitude, data.longitude], {icon: ownshipIcon}).addTo(map);
    map.setView([data.latitude, data.longitude], 10);
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
                title: data[i].name,
                airport_id: data[i].id
            });
        marker.on("click", function(){
            sidebar_showAirportResult(this.options.airport_id, false);
        })
        markerArray.push(marker);
        map.addLayer(markerArray[i]);
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

function click_airpsace(e) {
  sidebar_showAirspaceDetail(e.target.feature.properties);
}

function on_each_airpsace(feature, layer) {
  layer.on({
    mouseover: highlight_airspace,
    mouseout: reset_airspace,
    click: click_airpsace
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
      layers: [lyr]
  });

  ownshipIcon = L.icon({
      iconUrl: 'img/helicopter-top.png',
      iconSize: [48, 48],
      iconAnchor: [24, 24],
  });

  var class_b_style = {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#2464A9",
    "fillOpacity": 0.1,
    "dashArray": ""
  };

  var class_c_style = {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#7A2B51",
    "fillOpacity": 0.1,
    "dashArray": ""
  };

  var class_d_style = {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#0C3573",
    "fillOpacity": 0.1,
    "dashArray": "10, 5"
  };

  var class_e_style = {
    "stroke": true,
    "weight": 4,
    "opacity": 0.3,
    "color": "#741646",
    "fillOpacity": 0.1,
    "dashArray": "0.9"
  };

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

  // Reload airport markers when the user pans/zooms.
  map.on('moveend', function(){
    ROUND_DECIMAL_PLACES = 1000;
    if(map.getZoom() >= 9) {
        bounds = map.getBounds();
        zoom = map.getZoom();

        lonMin = Math.round(bounds.getWest() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES;
        lonMax = Math.round(bounds.getEast() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES;
        latMin = Math.round(bounds.getSouth() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES;
        latMax = Math.round(bounds.getNorth() * ROUND_DECIMAL_PLACES)/ROUND_DECIMAL_PLACES;

        rotobox_api(API_AIRPORT_WINDOW,
            {
                "latMin": latMin,
                "latMax": latMax,
                "lonMin": lonMin,
                "lonMax": lonMax
            },
            update_airport_markers);
    }
  });

  // Load the airspace shapefiles in the form of GeoJSON's.
  rotobox_api(API_AIRSPACE_AVAILABLE, {}, function(data) {
    var overlay_airspaces = {};

    for (var i = 0; i < data.length; i++) {
      // Skip Class-E5 because IMHO not very useful.
      if(data[i].name == "Class E5") {
        continue;
      }

      var airspaceStyle;
      if(data[i].name == "Class B") {
        airspaceStyle = class_b_style;
      } else if(data[i].name == "Class C") {
        airspaceStyle = class_c_style;
      } else if(data[i].name == "Class D") {
        airspaceStyle = class_d_style;
      } else {
        airspaceStyle = class_e_style;
      }

      overlay_airspaces[data[i].name] = new L.GeoJSON.AJAX("/airspaces/" + data[i].filename, {
        style: airspaceStyle,
        onEachFeature: on_each_airpsace
      });
    }

    // Only show the layers control if we actually have airspaces available.
    if(Object.keys(overlay_airspaces).length > 0){
      L.control.layers(null, overlay_airspaces).addTo(map);
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
  var html = $("#sidebar-airspaceDescription").render(properties);
  $("div.sidebar-scrollable").empty().append(html);
}

function sidebar_showAirportResult(airport_id, center){
  // TODO: Actually use the rendering functionality
  var html = $("#sidebar-airportResult").render();
  $("div.sidebar-scrollable").empty().append(html);

  rotobox_api(API_AIRPORT_ID, {"id": airport_id}, function(data){
    $("#airport-name").text(data[0].name);
    if(data[0].icao_name == "(null)") {
      $("#airport-identifiers").text(data[0].designator);
    } else {
      $("#airport-identifiers").text(data[0].icao_name + " (" + data[0].designator + ")");
    }
    $("dd.field-elevation").text(data[0].field_elevation + "'");
    $("#airport-remarks").text(data[0].remarks);

    $("h5.airport-tags").empty();
    // TODO: Fix bool values having to be strings
    if(data[0].traffic_control_tower_on_airport == "1") {
      $("h5.airport-tags").append("<span class='label label-primary'>Towered</span>\n");
    } else {
      $("h5.airport-tags").append("<span class='label label-default'>Nontowered</span>\n");
    }

    if(data[0].segmented_circle_marker_on_airport == "1") {
      $("h5.airport-tags").append("<span class='label label-info'>Segmented Circle</span>\n");
    }

    if(data[0].wind_direction_indicator == "1") {
      $("h5.airport-tags").append("<span class='label label-info'>Windsock</span>\n");
    }

    if(data[0].private_use == "1") {
      $("h5.airport-tags").append("<span class='label label-danger'>Private</span>\n");
    }

    if(center == true){
      map.setView([data[0].latitude, data[0].longitude], 10);
    }
  });

  $("ul#airport-runways").empty();
  rotobox_api(API_AIRPORT_RUNWAYS, {"id": airport_id}, function(data){
    for (var i = 0; i < data.length; i++) {
      var item = "<li class='list-group-item'>" + data[i].designator
      if((data[i].length != "(null)") && (data[i].width != "(null)")){
        item += "<span class='badge'>" + Math.round(data[i].width) + "' x " + Math.round(data[i].length) + "'</span>\n";
      }
      if(data[i].right_traffic_pattern != "1"){
        item += "<span class='badge rp-badge'>RP</span>\n";
      }
      item += "</li>\n";

      $("ul#airport-runways").append(item);
    }
  });

  rotobox_api(API_AIRPORT_RADIO, {"id": airport_id}, function(data){
    if(data.length != 0) {
      $("dd.ctaf-frequency").text(data[0].tx_frequency);
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

