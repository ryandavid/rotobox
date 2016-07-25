
function populate_satellite_list(data) {
    var html = $("#satelliteItemTemplate").render(data.satellites);
    $("#satellites-in-view").find("tbody").append(html);
}



$(document).ready(function(){
    rotobox_api(API_URI_SATELLITES, {}, populate_satellite_list);
});