var API_ROOT = "/api";
var API_URI_LOCATION =  "/location";
var API_URI_SATELLITES = "/satellites";
var API_AIRPORT_SEARCH = "/airports/search_by_name";
var API_AIRPORT_WINDOW = "/airports/search_by_window";
var API_AIRPORT_ID = "/airports/search_by_id";

function rotobox_api(uri, args, callback) {
    var fullURL = API_ROOT + uri;

    if (args != {}) {
        argNames = Object.keys(args);
        for (var i = 0; i < argNames.length; i++) {
            if (i == 0) {
                fullURL += "?";
            } else {
                fullURL += "&";
            }
            name = argNames[i];
            value = args[name];
            fullURL += name + "=" + value;
        }
    }

    $.ajax({url: fullURL, dataType: "json"})
    .done(function(data) {
        if (callback != undefined) {
            callback(data);
        }
    }).fail(function(jqXHR, textStatus, errorThrown){
        console.log("ERROR: Failed fetching " + fullURL);
        console.log("Text Status: " + textStatus);
        console.log("Error Thrown: " + errorThrown);
    });
}

