
## super simple layer-2 ping

This is a very simple userland application for layer-2 ping on
Linux. `s2pingd` runs as a daemon through systemd, and listens on
ethrenet interfaces using AF_PACKET socket. You can send echo request
frames using `s2ping` command, and receive reply frames from `s2pingd`

```
$ s2ping -h
s2ping [options] DSTMAC
    -i interface, outgoing interface name (required)
    -c count, how many packets xmit (0 means infinite)
    -s size, size of probe packet
    -I interval, second
    -T timeout, second
    -h print this help


$ s2ping -i bridge ce:e1:2e:bd:76:44 -c 5
S2PING to ce:e1:2e:bd:76:44 from 4a:d7:07:c2:e4:f2 (bridge) 64 bytes frame
64 bytes from ce:e1:2e:bd:76:44 seq=1 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=2 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=3 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=4 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=5 time=0.00 ms

5 frame(s) sent, 5 frame(s) received, 0.00% lost
```


### compile and install

```
git clone https://github.com/upa/s2pingd
cd s2pingd
make
sudo make install
```

### configure and start s2pingd

Edit /etc/default/s2pingd.

```
$ cat /etc/default/s2pingd
# s2pingd options
#
# list of interfaces on which s2pingd runs
S2PINGD_OPTS="vetha vethb"
```

and, enable s2pingd service on systemd.

```
sudo systemctl enable s2pingd
sudo systemctl start s2pingd
```

### note

- broadcast is useful.
```
$ s2ping -i bridge ff:ff:ff:ff:ff:ff
S2PING to ff:ff:ff:ff:ff:ff from 4a:d7:07:c2:e4:f2 (bridge) 64 bytes frame
64 bytes from ce:e1:2e:bd:76:44 seq=1 time=0.00 ms
64 bytes from 36:ba:10:e0:43:70 seq=1 time=0.00 ms
64 bytes from 36:ba:10:e0:43:70 seq=2 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=2 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=3 time=0.00 ms
64 bytes from 36:ba:10:e0:43:70 seq=3 time=0.00 ms
64 bytes from 36:ba:10:e0:43:70 seq=4 time=0.00 ms
64 bytes from ce:e1:2e:bd:76:44 seq=4 time=0.00 ms
^C
4 frame(s) sent, 4 frame(s) received, 0.00% lost
$
```


- btw, where is EtherOAM or BFD on Linux...?