var API_ROOT = "/api";
var API_URI_LOCATION =  "/location";
var API_URI_SATELLITES = "/satellites";


function query_uri(uri, callback) {
    var fullURL = API_ROOT + uri;

    $.ajax({url: fullURL, dataType: "json"})
    .done(function(data) {
        if (callback != undefined) {
            callback(data);
        }
    }).fail(function(){
        console.log("ERROR: Failed fetching " + uri);
    })
}

function print_data(data) {
    console.log(data);
}

query_uri(API_URI_LOCATION, print_data);
query_uri(API_URI_SATELLITES, print_data);