
forked from [mwarning/SimpleDNS](https://github.com/mwarning/SimpleDNS)

### Introduction

SimpleDNS is a very simple DNS server.
It was made to learn the basics of the DNS protocol.

Features:
* very small
* single-threaded
* all necessary data structures for further features
* very simplistic memory management
* supports A, AAAA, TXT, CNAME and MX queries
* no full protection against malformed requests :|

### Build

```
git clone https://github.com/mwarning/SimpleDNS.git
cd SimpleDNS
make
```

### Test

Start SimpleDNS:
```
$./main
Listening on port 9000.
```

In another console execute [dig](http://linux.die.net/man/1/dig) to make a DNS request:

```
$ dig @127.0.0.1 -p 9000 foo.bar.com A

; <<>> DiG 9.8.4-rpz2+rl005.12-P1 <<>> @127.0.0.1 -p 9000 foo.bar.com
; (1 server found)
;; global options: +cmd
;; Got answer:
;; -&gt;&gt;HEADER&lt;&lt;- opcode: QUERY, status: NOERROR, id: 15287
;; flags: qr; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; QUESTION SECTION:
;foo.bar.com.                   IN      A

;; ANSWER SECTION:
foo.bar.com.            0       IN      A       192.168.1.1

;; Query time: 0 msec
;; SERVER: 127.0.0.1#9000(127.0.0.1)
;; WHEN: Mon Apr 15 00:50:38 2013
;; MSG SIZE  rcvd: 56


$dig @127.0.0.1 -p 9000 txt.bar.com txt

; <<>> DiG 9.11.3-1ubuntu1.11-Ubuntu <<>> @127.0.0.1 -p 9000 txt.bar.com txt
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 29325
;; flags: qr; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; QUESTION SECTION:
;txt.bar.com.			IN	TXT

;; ANSWER SECTION:
txt.bar.com.		3600	IN	TXT	"abcdefg"

;; Query time: 1 msec
;; SERVER: 127.0.0.1#9000(127.0.0.1)
;; WHEN: Tue Dec 10 00:20:43 CST 2019
;; MSG SIZE  rcvd: 60


$ dig @127.0.0.1 -p 9000 cname.bar.com cname

; <<>> DiG 9.11.3-1ubuntu1.11-Ubuntu <<>> @127.0.0.1 -p 9000 cname.bar.com cname
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 8873
;; flags: qr; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; QUESTION SECTION:
;cname.bar.com.			IN	CNAME

;; ANSWER SECTION:
cname.bar.com.		3600	IN	CNAME	abc.efg.com.

;; Query time: 24 msec
;; SERVER: 127.0.0.1#9000(127.0.0.1)
;; WHEN: Tue Dec 10 00:21:38 CST 2019
;; MSG SIZE  rcvd: 69


$ dig @127.0.0.1 -p 9000 mx.bar.com mx

; <<>> DiG 9.11.3-1ubuntu1.11-Ubuntu <<>> @127.0.0.1 -p 9000 mx.bar.com mx
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 63870
;; flags: qr; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; QUESTION SECTION:
;mx.bar.com.			IN	MX

;; ANSWER SECTION:
mx.bar.com.		3600	IN	MX	1 abc.efg.com.

;; Query time: 0 msec
;; SERVER: 127.0.0.1#9000(127.0.0.1)
;; WHEN: Wed Nov 27 23:39:41 CST 2019
;; MSG SIZE  rcvd: 65


$ dig @127.0.0.1 -p 9000 soa.bar.com soa

; <<>> DiG 9.11.3-1ubuntu1.11-Ubuntu <<>> @127.0.0.1 -p 9000 soa.bar.com soa
; (1 server found)
;; global options: +cmd
;; Got answer:
;; ->>HEADER<<- opcode: QUERY, status: NOERROR, id: 61557
;; flags: qr; QUERY: 1, ANSWER: 1, AUTHORITY: 0, ADDITIONAL: 0

;; QUESTION SECTION:
;soa.bar.com.			IN	SOA

;; ANSWER SECTION:
soa.bar.com.		3600	IN	SOA	ns.xxx.com. admin.xxx.com. 1 2 3 4 5

;; Query time: 1 msec
;; SERVER: 127.0.0.1#9000(127.0.0.1)
;; WHEN: Tue Dec 10 00:22:17 CST 2019
;; MSG SIZE  rcvd: 99

```

Note:
- On Debian Linux, dig is part of the dnsutils package.
- Use AAAA instead of A in the dig command line to request the IPv6 address.

## Modify address entries

The code maps the domain "foo.bar.com" to the IPv4 address 192.168.1.1 and IPv6 address fe80::1.
It is easy to find it in the code and to add other entries.

### Recommended Reading

The DNS section of the [TCP/IP-Guide](http://www.tcpipguide.com/free/t_TCPIPDomainNameSystemDNS.htm) was very helpful for understanding the protocol.
