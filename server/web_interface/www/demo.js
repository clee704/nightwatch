(function (window, document, undefined) {


var simulator = new (function () {

    this.start = start;

    var time;

    // for making random devices
    var states = ['up', 'suspended', 'hibernated', 'resuming', 'down'];
    var hostnames = ['martini', 'mimosa', 'bacardi', 'kahlua'];
    var used = {hostnames: {length: 0}, mac: {}, ip: {}};

    function start() {
        var n = 10;
        for (var i = 0; i < n; ++i) {
            var d = makeRandomDevice(i, n);
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
                var a = Math.random() < .0025;
                var b = Math.random() < .5;
                var c = Math.random() < .00025;
                if (a)
                    d.changeState(b ? 'suspended' : 'hibernated');
                if (c)
                    d.changeState('down');
                break;
            case 'down':
                d.totalDowntime += diff1;
                var a = Math.random() < .0005;
                if (a)
                    d.changeState('up');
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
                var a = Math.random() < .0025;
                if (a)
                    d.changeState('resuming');
                break;
            }
        }
        time = currTime;
    }

    // Mock object constructor!
    function Device(hostname, mac, ip, state, monitoredSince, totalUptime,
            sleepTime, totalDowntime) {
        this.hostname = hostname;
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

    function makeRandomDevice(i, n) {
        var hostname = '';
        var numAvailableHostnames = hostnames.length - used.hostnames.length;
        if (randomInt(0, n - i - 1) < numAvailableHostnames)
            hostname = rollExclude(used.hostnames, pick, hostnames);
        var mac = rollExclude(used.mac, randomMac);
        var ip = rollExclude(used.ip, randomIp);
        var state = pick(states);
        var currTime = now();
        var monitoredSince = currTime + randomInt(-600 * 1e6, -300 * 1e6);
        var totalDowntime = randomInt(.5 * 1e6, 20 * 1e6);
        var totalUptime = currTime - monitoredSince - totalDowntime;
        var sleepTime = Math.random() * .9 * totalUptime;
        return new Device(hostname, mac, ip, state, monitoredSince,
            totalUptime, sleepTime, totalDowntime);
    }

    function randomMac() {
        var a = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
        var b = new Number(randomInt(0, 255) + 256).toString(16).substring(1);
        return '00:11:22:33:' + a + ':' + b;
    }

    function randomIp() {
        return '10.0.0.' + randomInt(2, 192);
    }

    function rollExclude(exclude, random, args) {
        while (true) {
            var n = random(args);
            if (exclude[n])
                continue;
            exclude[n] = true;
            ++exclude.length;
            return n;
        }
    }

    function pick(sequence) {
        return sequence[randomInt(0, sequence.length - 1)];
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
