(function (window, document, undefined) {


var deviceListTable = $('#device-list');
var deviceListTbody = deviceListTable.find('tbody');
var selectAllOrNone = $('#select-all-or-none');

var rowTemplate = '<tr>'
    + '<td class="checkbox"><input type="checkbox"></td>'
    + '<td class="mac value"></td>'
    + '<td class="ip value"></td>'
    + '<td class="state value"></td>'
    + '<td class="monitored-since value"></td>'
    + '<td class="total-uptime"><div class="graph"><div>'
    + '<span class="value"></span> (<span class="value"></span>%)'
    + '</div></div></td>'
    + '<td class="total-downtime value"></td>'
    + '<td class="availability"><span class="value"></span>%</td>'
    + '</tr>';

var demoMode = false;
var get = $.get;


function enterDemoMode() {
    $('#demo').attr('href', '.').html('Exit Demo Mode');
    $.getScript('demo.js', function () {
        demoMode = true;
        get = window.demo.get;
        updateDeviceList();
    });
}

function updateDeviceList() {
    get('/ajax/getdevicelist', fillTable);
}

function fillTable(deviceList) {
    deviceListTbody.detach();
    var rows = deviceListTbody.find('tr');
    var n = Math.min(rows.length, deviceList.length);
    for (var i = n, m = rows.length; i < m; ++i)
        $(rows[i]).remove();
    for (var i = n, m = deviceList.length; i < m; ++i)
        $(rowTemplate)
            .appendTo(deviceListTbody)
            .find('input[type="checkbox"]').val(i);
    var rows = deviceListTbody.find('tr');
    for (var i = 0, m = deviceList.length; i < m; ++i) {
        var row = $(rows[i]);
        var fields = row.find('.value');
        var d = deviceList[i];
        fields[0].innerHTML = d.mac;
        fields[1].innerHTML = d.ip;
        $(fields[2]).removeClass(fields[2].innerHTML);
        $(fields[2]).addClass(d.state);
        fields[2].innerHTML = d.state;
        fields[3].innerHTML = toDateTimeString(d.monitoredSince);
        fields[4].innerHTML = toTimeString(d.totalUptime);
        var pctSleep = toPercentage3(d.sleepTime, d.totalUptime);
        fields[5].innerHTML = pctSleep;
        fields[6].innerHTML = toTimeString(d.totalDowntime);
        fields[7].innerHTML = toPercentage3(d.totalUptime,
            d.totalUptime + d.totalDowntime);
        row.find('.total-uptime .graph').css('width', pctSleep + '%');
    }
    deviceListTable.append(deviceListTbody);
}

function toDateTimeString(milliseconds) {
    var d = new Date(milliseconds);
    var year = d.getFullYear();
    var month = zeroPad2(d.getMonth() + 1);
    var date = zeroPad2(d.getDate());
    var hours = zeroPad2(d.getHours());
    var minutes = zeroPad2(d.getMinutes());
    var seconds = zeroPad2(d.getSeconds());
    var timezoneOffset = zeroPad2(d.getTimezoneOffset() / -60);
    var ymd = [year, month, date];
    var hms = [hours, minutes, seconds];
    return ymd.join('-') + ' ' + hms.join(':') + '+' + timezoneOffset;
}

function toTimeString(milliseconds) {
    // The bitwise OR operator "|" here substitutes for Math.floor().
    // Be cautious: It only works for a number N such that
    // -2**31-1 < N < 2**31.
    var t = milliseconds / 1000 | 0;
    var seconds = zeroPad2(t % 60 | 0); t = t / 60 | 0;
    var minutes = zeroPad2(t % 60 | 0); t = t / 60 | 0;
    var hours = zeroPad2(t % 24 | 0); t = t / 24 | 0;
    var days = t | 0;
    var hms = [hours, minutes, seconds];
    return (days ? days + ' days ' : '') + hms.join(':');
}

function toPercentage3(a, b) {
    return new Number(a / b * 100).toPrecision(3);
}

function zeroPad2(v) {
    return v < 10 ? '0' + v : v;
}


// Check or uncheck all checkboxes according to the master checkbox
selectAllOrNone.click(function () {
    deviceListTbody.find('input').attr('checked', this.checked);
});

// Open the page in a new window when an a.new-window is clicked
$(document).delegate('a.new-window', 'click', function () {
    window.open(this.href);
    return false;
});

// Enter the demo mode if requested
window.onhashchange = function () {
    if (location.hash === '#demo')
        enterDemoMode();
};

// When the DOM is ready
$(function () {

    // Update the device list every 15 seconds
    updateDeviceList();
    setInterval(updateDeviceList, 15000);

    // Enter the demo mode if requested
    if (location.hash === '#demo')
        enterDemoMode();

});


})(this, this.document);
