(function (window, document, undefined) {


// Frequently used DOM elements
var deviceListTable = $('#device-list');
var deviceListTbody = deviceListTable.find('tbody');
var selectAllOrNone = $('#select-all-or-none');

var get = $.get;


function refresh() {
    updateDeviceListPeriodically();
    this.disabled = true;
    setTimeout($.proxy(function () {
        this.disabled = false;
    }, this), 1000);
}

function resume(deviceId, state, name, callback) {
    if (state === 'up' || state === 'resuming')
        alert(name + ' is already up or resuming.');
    else if (confirm('Resume ' + name + '?'))
        get('/ajax/resume?deviceId=' + deviceId, callback);
}

function suspend(deviceId, state, name, callback) {
    if (state !== 'up')
        alert(name + ' is not up.');
    else if (confirm('Suspend ' + name + '?'))
        get('/ajax/suspend?deviceId=' + deviceId, callback);
}

function enterDemoModeIfHashMatches() {
    if (location.hash !== '#demo')
        return false;
    $('#demo').attr('href', '.').html('Exit Demo Mode');
    $.getScript('demo.js', function () {
        get = window.demo.get;
        updateDeviceListPeriodically();
    });
    return true;
}

function forEachBoxChecked(callback) {
    function createNotifier(n, callback) {
        return function () {
            if (--n == 0) {
                callback();
            }
        }
    }
    return function () {
        var checkboxes = deviceListTbody.find('input:checked');
        var n = checkboxes.length;
        if (n === 0)
            return;
        updateDeviceList();
        var notify = createNotifier(n, updateDeviceList);
        for (var i = 0; i < n; ++i) {
            var c = checkboxes[i];
            var d = $.data(c, 'deviceInfo');
            var deviceId = d.mac;
            var state = d.state;
            // Define human-friendly name for dialog boxes
            var name = (d.hostname ? (d.hostname + ' ') : '')
                + '(' + d.ip + ')';
            callback(deviceId, state, name, notify);
        }
    };
}

/* @function */
var updateDeviceList = (function () {

    var rowTemplate = '<tr>'
        + '<td class="checkbox"><input type="checkbox"></td>'
        + '<td class="hostname value"></td>'
        + '<td class="ip value"></td>'
        + '<td class="mac value"></td>'
        + '<td class="state value"></td>'
        + '<td class="monitored-since value"></td>'
        + '<td class="total-uptime"><div class="wrapper">'
        + '<div class="graph"></div>'
        + '<div class="content">'
        + '<span class="value"></span> (<span class="value"></span>%)'
        + '</div>'
        + '</div></td>'
        + '<td class="total-downtime value"></td>'
        + '<td class="availability"><span class="value"></span>%</td>'
        + '</tr>';

    return function updateDeviceList() {
        get('/ajax/devicelist', fillTable);
    };

    function fillTable(json) {
        var deviceList = json.data;
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
            fields[0].innerHTML = d.hostname;
            fields[1].innerHTML = d.ip;
            fields[2].innerHTML = d.mac;
            $(fields[3]).removeClass(fields[3].innerHTML);
            $(fields[3]).addClass(d.state);
            fields[3].innerHTML = d.state;
            fields[4].innerHTML = toDateTimeString(d.monitoredSince);
            fields[5].innerHTML = toTimeString(d.totalUptime);
            var pctSleep = toPercentage3(d.sleepTime, d.totalUptime);
            fields[6].innerHTML = pctSleep;
            fields[7].innerHTML = toTimeString(d.totalDowntime);
            fields[8].innerHTML = toPercentage3(d.totalUptime,
                d.totalUptime + d.totalDowntime);
            row.find('.total-uptime .graph').css('width', pctSleep + '%');
            var checkbox = row.find('input')[0];
            $.data(checkbox, 'deviceInfo', d);
        }
        deviceListTable.append(deviceListTbody);
    }

    function toDateTimeString(seconds) {
        var d = new Date(seconds * 1000);
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

    function toTimeString(seconds) {
        // The bitwise OR operator "|" here substitutes for Math.floor().
        // Be cautious: It only works for a number N such that
        // -2**31-1 < N < 2**31.
        var t = seconds;
        var seconds = zeroPad2(t % 60 | 0); t = t / 60 | 0;
        var minutes = zeroPad2(t % 60 | 0); t = t / 60 | 0;
        var hours = zeroPad2(t % 24 | 0); t = t / 24 | 0;
        var days = t | 0;
        var hms = [hours, minutes, seconds];
        return days ? days + (days === 1 ? ' day ' : ' days ') + hms.join(':')
                    : hms.join(':');
    }

    function toPercentage3(a, b) {
        var n = new Number(b == 0 ? 0 : a / b * 100);
        return n.toPrecision(3);
    }

    function zeroPad2(v) {
        return v < 10 ? '0' + v : v;
    }
})();

/* @function */
var updateDeviceListPeriodically = (function () {
    var updateTimer;
    return function updateDeviceListPeriodically() {
        clearTimeout(updateTimer);
        updateDeviceList();
        updateTimer = setInterval(updateDeviceList, 5000);
    }
})();


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

// Define actions
$('button#refresh').click(refresh);
$('button#resume').click(forEachBoxChecked(resume));
$('button#suspend').click(forEachBoxChecked(suspend));

// Open the page in a new window when an a.new-window is clicked
$(document).delegate('a.new-window', 'click', function () {
    window.open(this.href);
    return false;
});

window.onhashchange = enterDemoModeIfHashMatches;

// When the DOM is ready
$(function () {
    if (!enterDemoModeIfHashMatches())
        updateDeviceListPeriodically();  // every 15 seconds
});


})(this, this.document);
