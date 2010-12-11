(function (window, document, undefined) {


var simulator = new (function () {

    this.start = start;

    var time;

    // for making random devices
    var states = ['up', 'suspended', 'hibernated', 'resuming', 'down'];
    var usedAddresses = {mac: {}, ip: {}};

    function start() {
        for (var i = 0; i < 10; ++i) {
            var d = makeRandomDevice();
            devices.push(d);
            devicesById[d.mac] = d;
        }
        time = now();
        setInterval(simulate, 1000);
    }

    function simulate() {
        var currTime = now();
        var diff1 = currTime - time;
        for (var i = 0, n = devices.length; i < n; ++i) {
            var d = devices[i];
            var diff2 = currTime - d._lastStateChange;
            switch (d.state) {
            case 'up':
                d.totalUptime += diff1;
                var a = Math.random() < .005;
                var b = Math.random() < .5;
                var c = Math.random() < .0005;
                if (a)
                    d.changeState(b ? 'suspended' : 'hibernated');
                if (c)
                    d.changeState('down');
                break;
            case 'down':
                d.totalDowntime += diff1;
                var a = Math.random() < .001;
                if (a || d._resume && diff2 > d._timeToResume) {
                    d.changeState('up');
                    delete d._resume;
                }
                break;
            case 'resuming':
                d.totalUptime += diff1;
                d.sleepTime += diff1;
                if (diff2 > d._timeToResume)
                    d.changeState('up');
                break;
            default:  // suspended or hibernated
                d.totalUptime += diff1;
                d.sleepTime += diff1;
                var a = Math.random() < .005;
                if (a)
                    d.changeState('resuming');
                break;
            }
        }
        time = currTime;
    }

    // Mock object constructor!
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
        this._lastStateChange = now();
    }

    Device.prototype.changeState = function (state) {
        this.state = state;
        this._lastStateChange = now();
    }

    function makeRandomDevice() {
        var mac = rollExclude(randomMac, usedAddresses.mac);
        var ip = rollExclude(randomIp, usedAddresses.ip);
        var state = states[randomInt(0, states.length - 1)];
        var currTime = now();
        var monitoredSince = currTime + randomInt(-600 * 1e6, -300 * 1e6);
        var totalDowntime = randomInt(.5 * 1e6, 20 * 1e6);
        var totalUptime = currTime - monitoredSince - totalDowntime;
        var sleepTime = Math.random() * .9 * totalUptime;
        return new Device(mac, ip, state, monitoredSince, totalUptime, sleepTime,
            totalDowntime);
    }

    function rollExclude(random, exclude) {
        while (true) {
            var n = random();
            if (exclude[n])
                continue;
            exclude[n] = true;
            return n;
        }
    }

    function randomMac() {
        var a = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
        var b = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
        return '00:11:22:33:' + a + ':' + b;
    }

    function randomIp() {
        return '10.0.0.' + randomInt(2, 192);
    }

    // Return a random integer N such that a <= N <= b.
    function randomInt(a, b) {
        a = Math.floor(a);
        b = Math.floor(b);
        var c = b + 1 - a;
        return Math.floor(Math.random() * c) % c + a;
    }

    function now() {
        // IE8 and less doesn't have Date.now()
        return Date.now ? Date.now() : new Date().getTime();
    }
});

var devices = [];

var devicesById = {};

var ajaxHandlers = {
    '/ajax/resume': resume,
    '/ajax/suspend': suspend,
    '/ajax/devicelist': getDeviceList
};


function resume(queryString) {
    var deviceId = /deviceId=(.*)/.exec(queryString)[1];
    var d = devicesById[deviceId];
    if (d.state === 'up' || d.state === 'resuming')
        return;
    else if (d.state !== 'down')
        d.changeState('resuming');
    else if (Math.random() < .25)
        d._resume = true;
}

function suspend(queryString) {
    console.log('demo: suspend(' + queryString + ')');
}

function getDeviceList(queryString, callback) {
    callback(devices);
}


simulator.start();

window.demo = {

    get: function (url, callback) {
        var i = url.indexOf('?');
        var path = i < 0 ? url : url.substring(0, i);
        var queryString = i < 0 ? '' : url.substring(i + 1);
        ajaxHandlers[path](queryString, callback);
    }
};


})(this, this.document);
