var deviceListTable = $('#device-list');
var deviceListTbody = deviceListTable.find('tbody');
var selectAllOrNone = $('#select-all-or-none');

$(function () {
    getDeviceList();
    setInterval(getDeviceList, 30000);
});

selectAllOrNone.click(function () {
    deviceListTbody.find('input').attr('checked', this.checked);
});

function getDeviceList() {
    $.get('/ajax/getdevicelist', function (deviceList) {
        updateTable(deviceList);
    });
}

var rowTemplate = '<tr>'
    + '<td class="checkbox"><input type="checkbox"></td>'
    + '<td class="mac value"></td>'
    + '<td class="ip value"></td>'
    + '<td class="status value"></td>'
    + '<td class="monitored-since value"></td>'
    + '<td class="total-uptime"><div class="graph"><div>'
    + '<span class="value"></span> (<span class="value"></span>%)'
    + '</div></div></td>'
    + '<td class="total-downtime value"></td>'
    + '<td class="availability"><span class="value"></span>%</td>'
    + '</tr>';

function updateTable(deviceList) {
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
        $(fields[2]).addClass(d.status);
        fields[2].innerHTML = d.status;
        fields[3].innerHTML = toDateString(d.monitoredSince);
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

function toDateString(milliseconds) {
    return new Date(milliseconds).toUTCString();
}

function toTimeString(milliseconds) {
    var t = milliseconds / 1000 | 0;
    var seconds = t % 60 | 0; t = t / 60 | 0;
    var minutes = t % 60 | 0; t = t / 60 | 0;
    var hours = t % 24 | 0; t = t / 24 | 0;
    var days = t % 7 | 0; t = t / 7 | 0;
    var weeks = t | 0;
    var tokens1 = [];
    var tokens2 = [];
    if (weeks)
        tokens1.push(weeks, 'weeks');
    if (days)
        tokens1.push(days, 'days');
    if (hours < 10)
        tokens2.push('0');
    tokens2.push(hours, ':');
    if (minutes < 10)
        tokens2.push('0');
    tokens2.push(minutes, ':');
    if (seconds < 10)
        tokens2.push('0');
    tokens2.push(seconds);
    tokens1.push(tokens2.join(''));
    return tokens1.join(' ');
}

function toPercentage3(a, b) {
    return new Number(a / b * 100).toPrecision(3);
}
