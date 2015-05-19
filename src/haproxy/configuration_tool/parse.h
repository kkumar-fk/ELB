#define MAX_NBPROC	8

#define USER		"haproxy"
#define GROUP		"haproxy"

#define HAPROXY_SOCK	"/var/run/haproxy.sock"
#define STATS_TIMEOUT	"2m"

#define CA_BASE		"/etc/ssl/certs"
#define CERT_BASE	"/etc/ssl/private"

#define CIPHERS		"AES:ALL:!aNULL:!eNULL:+RC4:@STRENGTH"
#define CIPHER_OPTIONS	"no-sslv3 no-tlsv10"

#define COMPRESSION_ALGO "gzip"
#define COMPRESSION_TYPE "text/html text/plain text/javascript application/javascript application/xml text/css application/octet-stream"

#define MAX_VIPS	8
#define MAX_SERVERS	32

#define REVERSE_PROXY	"reverse"
#define FORWARD_PROXY	"forward"

#define MAX_LEN		80

#define VIP		100
#define SIP		101

#define SERVER_NAME	"nginx-"
#define CONFIG_FILE	"haproxy.cfg"

#define CONFIG_PATH	"/etc/haproxy/"

/* Define different sections returned as they are parsed */
#define GLOBAL_SECTION		1
#define DEFAULTS_SECTION	2
#define USERLIST_SECTION	3
#define FRONTEND_SECTION	4
#define FRONTEND_SSL_SECTION	5
#define BACKEND_SECTION		6
