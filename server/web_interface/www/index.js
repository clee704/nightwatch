(function (window, document, undefined) {


var deviceListTable = $('#device-list');
var deviceListTbody = deviceListTable.find('tbody');
var selectAllOrNone = $('#select-all-or-none');

var updateDeviceList = (function () {

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

    return function () {
        get('/ajax/devicelist', fillTable);
    };

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
            var checkbox = row.find('input')[0];
            $.data(checkbox, 'deviceId', d.mac);
            $.data(checkbox, 'ip', d.ip);
            $.data(checkbox, 'state', d.state);
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
})();

var updateDeviceListPeriodically = (function () {

    var updateTimer;

    return function () {
        clearTimeout(updateTimer);
        updateDeviceList();
        updateTimer = setInterval(updateDeviceList, 15000);
    }
})();

var get = $.get;


function enterDemoModeIfHashMatches() {
    if (location.hash !== '#demo')
        return;
    $('#demo').attr('href', '.').html('Exit Demo Mode');
    $.getScript('demo.js', function () {
        get = window.demo.get;
        updateDeviceList();
    });
}


// Check or uncheck all checkboxes according to the master checkbox
selectAllOrNone.click(function () {
    deviceListTbody.find('input').attr('checked', this.checked).change();
});

// Define checkbox behaviors 
deviceListTbody.delegate('input', 'change', function () {
    var checked = this.checked;
    var method = checked ? 'addClass' : 'removeClass';
    if (checked)
        selectAllOrNone[0].checked = true;
    else if (deviceListTbody.find('input:checked').length === 0)
        selectAllOrNone[0].checked = false;
    $(this).parents('tr')[method]('selected');
});

$('#refresh').click(function () {
    updateDeviceListPeriodically();
    this.disabled = true;
    setTimeout($.proxy(function () {
        this.disabled = false;
    }, this), 1000);
});

$('#resume').click(function () {
    var checked = deviceListTbody.find('input:checked');
    var n = checked.length;
    if (n === 0)
        return;
    updateDeviceListPeriodically();
    for (var i = 0; i < n; ++i) {
        var c = checked[i];
        var deviceId = $.data(c, 'deviceId');
        var ip = $.data(c, 'ip');
        var state = $.data(c, 'state');
        var name = deviceId + ' (' + ip + ')';
        if (state === 'up' || state === 'resuming')
            alert(name + ' is already up or resuming.');
        else if (confirm('Resume ' + name + '?'))
            get('/ajax/resume?deviceId=' + deviceId);
    }
    updateDeviceListPeriodically();
});

$('#suspend').click(function () {
});

$('#hibernate').click(function () {
});

// Open the page in a new window when an a.new-window is clicked
$(document).delegate('a.new-window', 'click', function () {
    window.open(this.href);
    return false;
});

window.onhashchange = enterDemoModeIfHashMatches;

// When the DOM is ready
$(function () {
    updateDeviceListPeriodically();  // every 15 seconds
    enterDemoModeIfHashMatches();
});


})(this, this.document);
