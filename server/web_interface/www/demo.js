(function (window, document, undefined) {


var devices = [];
var time;

var STATE = ['up', 'suspended', 'hibernated', 'resuming', 'down'];
var used = {mac: {}, ip: {}};


function start() {
    for (var i = 0; i < 10; ++i)
        devices.push(makeRandomDevice());
    time = new Date().getTime();
    setInterval(simulate, 1000);
}

function simulate() {
    var now = new Date().getTime();
    var diff1 = now - time;
    for (var i = 0, n = devices.length; i < n; ++i) {
        var d = devices[i];
        var diff2 = now - d._lastStateChange;
        switch (d.state) {
        case 'up':
            d.totalUptime += diff1;
            if (Math.random() < .005)
                d.changeState(Math.random() < .5 ? 'suspended' : 'hibernated');
            else if (Math.random() < .0005)
                d.changeState('down');
            break;
        case 'down':
            d.totalDowntime += diff1;
            if (Math.random() < .001)
                d.changeState('up');
            break;
        case 'resuming':
            d.totalUptime += diff1;
            if (diff2 > d._timeToResume)
                d.changeState('up');
            break;
        default:  // suspended or hibernated
            d.totalUptime += diff1;
            d.sleepTime += diff1;
            if (Math.random() < .005)
                d.changeState('resuming');
            break;
        }
    }
    time = now;
}

function getDeviceList(callback) {
    callback(devices);
}

function Device(mac, ip, state, monitoredSince, totalUptime, sleepTime,
    totalDowntime) {
    this.mac = mac;
    this.ip = ip;
    this.state = state;
    this.monitoredSince = monitoredSince;
    this.totalUptime = totalUptime;
    this.sleepTime = sleepTime;
    this.totalDowntime = totalDowntime;
    this._timeToResume = randomInt(5000, 15000);
    this._lastStateChange = new Date().getTime();
}

Device.prototype.changeState = function (state) {
    this.state = state;
    this._lastStateChange = new Date().getTime();
}

function makeRandomDevice() {
    var mac = roll(randomMacAddress, used.mac);
    var ip = roll(randomIpAddress, used.ip);
    var state = STATE[randomInt(0, STATE.length - 1)];
    var now = new Date().getTime();
    var monitoredSince = randomInt(now - 600 * 1e6, now - 300 * 1e6);
    var totalDowntime = randomInt(.5 * 1e6, 20 * 1e6);
    var totalUptime = now - monitoredSince - totalDowntime;
    var sleepTime = Math.random() * .9 * totalUptime;
    return new Device(mac, ip, state, monitoredSince, totalUptime, sleepTime,
        totalDowntime);
}

function roll(random, exclude) {
    while (true) {
        var n = random();
        if (exclude[n])
            continue;
        exclude[n] = true;
        return n;
    }
}

function randomMacAddress() {
    var a = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
    var b = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
    return '00:11:22:33:' + a + ':' + b;
}

function randomIpAddress() {
    return '10.0.0.' + randomInt(2, 192);
}

// Return a random integer N such that a <= N <= b.
function randomInt(a, b) {
    a = Math.floor(a);
    b = Math.floor(b);
    var c = b + 1 - a;
    return Math.floor(Math.random() * c) % c + a;
}


start();

window.demo = {
    get: function (url, callback) {
        switch (url) {
        case '/ajax/getdevicelist':
            getDeviceList(callback); break;
        }
    }
};


})(this, this.document);
