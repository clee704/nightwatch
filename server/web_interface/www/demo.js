(function (window, document, undefined) {


function EnvironmentSimulator(devices) {

    this.start = start;

    var time;

    function start() {
        time = Date.now();
        setInterval(simulate, 1000);
    }

    function simulate() {
        var currTime = Date.now();
        var interval = currTime - time;
        for (var i = 0, n = devices.length; i < n; ++i) {
            var d = devices[i];
            switch (d.state) {
            case 'up':
                d.totalUptime += interval;
                if (Math.random() < .0025)
                    d.changeState('suspended');
                if (Math.random() < .0001)
                    d.changeState('down');
                break;
            case 'suspended':  // suspended
                d.totalUptime += interval;
                d.sleepTime += interval;
                if (Math.random() < .0025)
                    d.changeState('resuming');
                break;
            case 'resuming':
                d.totalUptime += interval;
                d.sleepTime += interval;
                if (currTime - d._lastStateChange > d._resumeTime)
                    d.changeState('up');
                break;
            case 'down':
                d.totalDowntime += interval;
                if (Math.random() < .0005)
                    d.changeState('up');
                break;
            }
        }
        time = currTime;
    }
}


function AjaxHandler(devices) {

    this.handle = handle;

    var devicesById = devices.indexBy(function (d) { return d.mac; });
    var handlers = {
        '/ajax/resume': resume,
        '/ajax/suspend': suspend,
        '/ajax/devicelist': getDeviceList
    };

    function handle(url, callback) {
        var i = url.indexOf('?');
        var path = i < 0 ? url : url.substring(0, i);
        var queryString = i < 0 ? '' : url.substring(i + 1);
        handlers[path](queryString, callback);
    }

    function resume(queryString) {
        var deviceId = /deviceId=(.*)/.exec(queryString)[1];
        var d = devicesById[deviceId];
        if (d.state === 'up' || d.state === 'resuming')
            return;
        else if (d.state !== 'down')
            d.changeState('resuming');
    }

    function suspend(queryString) {
        var deviceId = /deviceId=([^&]*)/.exec(queryString)[1];
        var suspendType = /type=([^&]*)/.exec(queryString)[1];
        var d = devicesById[deviceId];
        if (d.state === 'up')
            d.changeState(suspendType.replace(/e?$/, 'ed'));
    }

    function getDeviceList(queryString, callback) {
        callback(devices);
    }
}


/* @singleton */
var mockFactory = new (function MockFactory() {

    this.make = make;

    var pool = {
        states: [
            'up', 'up', 'up', 'up', 'up', 'suspended', 'suspended',
            'suspended', 'resuming', 'down'
        ],
        hostnames: [
            'martini', 'mimosa', 'bacardi', 'kahlua', '', '', '', '', '', ''
        ],
        used: {mac: {}, ip: {}}
    };

    function make(n) {
        var devices = [];
        for (var i = 0; i < n; ++i)
            devices.push(makeRandomDevice(i, n));
        devices.sort(function (a, b) {
            return a.ip.value - b.ip.value;
        });
        return devices;
    }

    function makeRandomDevice(i, n) {
        var hostname = pick(pool.hostnames);
        var mac = rollEx(randomMac, pool.used.mac);
        var ip = rollEx(randomIp, pool.used.ip);
        var state = pick(pool.states);
        var currTime = Date.now();
        var duration = randomInt(0, 300 * 1e6);
        var monitoredSince = currTime - duration;
        var totalDowntime = randomInt(0, Math.min(20 * 1e6, duration));
        var totalUptime = duration - totalDowntime;
        var sleepTime = Math.floor(Math.random() * .95 * totalUptime);
        return new Device(hostname, mac, ip, state, monitoredSince,
            totalUptime, sleepTime, totalDowntime);
    }

    function randomMac() {
        return '00:11:22:33:' + randomHex(2) + ':' + randomHex(2);
    }

    function randomIp() {
        return '10.0.0.' + randomInt(2, 192);
    }

    function rollEx(random, exclude) {
        while (true) {
            var n = random();
            if (exclude[n])
                continue;
            exclude[n] = true;
            return n;
        }
    }

    function pick(sequence) {
        return sequence.splice(randomInt(0, sequence.length - 1), 1)[0];
    }

    // Return a random integer N such that a <= N <= b.
    function randomInt(a, b) {
        a = Math.floor(a);
        b = Math.floor(b);
        var c = b + 1 - a;
        return Math.floor(Math.random() * c) % c + a;
    }

    function randomHex(n) {
        var a = [];
        for (var i = 0; i < n; ++i)
            a.push(new Number(randomInt(0, 15)).toString(16));
        return a.join('');
    }

    // Mock object constructor
    function Device(hostname, mac, ip, state, monitoredSince, totalUptime,
            sleepTime, totalDowntime) {
        this.hostname = hostname;
        this.mac = mac;
        this.ip = new IpAddress(ip);
        this.state = state;
        this.monitoredSince = monitoredSince;
        this.totalUptime = totalUptime;
        this.sleepTime = sleepTime;
        this.totalDowntime = totalDowntime;
        this._resumeTime = randomInt(3500, 12500);
        this._lastStateChange = Date.now();
    }

    Device.prototype.changeState = function (state) {
        this.state = state;
        this._lastStateChange = Date.now();
    }

    function IpAddress(str) {
        this.str = str;
        var b = str.split('.');
        var value = 0;
        for (var i = 0, n = b.length; i < n; ++i)
            value = value * 256 + Number(b[i]);
        this.value = value;
    }

    IpAddress.prototype.toString = function () {
        return this.str;
    }
});


(function extendLanguage() {

    Date.now = Date.now || function () {
        new Date().getTime();
    };

    Array.prototype.indexBy = Array.prototype.indexBy || function (index) {
        var o = {};
        for (var i = 0, n = this.length; i < n; ++i) {
            var e = this[i];
            o[index(e)] = e;
        }
        return o;
    };
})();


(function init() {

    var devices = mockFactory.make(10);
    var simulator = new EnvironmentSimulator(devices);
    var handler = new AjaxHandler(devices);

    window.demo = {
        get: function (url, callback) {
            handler.handle(url, callback);
        }
    };

    simulator.start();
})();


})(this, this.document);
