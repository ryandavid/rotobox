function populate_satellite_list(data) {
    var html = $("#satelliteItemTemplate").render(data.satellites);
    $("#satellites-in-view").find("tbody").append(html);
}

function home_init(){
    rotobox_api(API_URI_SATELLITES, {}, populate_satellite_list);
}