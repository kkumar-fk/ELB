#ifndef PARSE_H
#define PARSE_H

/* Maximum number of haproxy instances */
#define MAX_NBPROC		8

/* User/Group id for the proxy */
#define USER			"haproxy"
#define GROUP			"haproxy"

/* Constants */
#define HAPROXY_SOCK		"/var/run/haproxy.sock"
#define STATS_TIMEOUT		"2m"

#define CA_BASE			"/etc/ssl/certs"
#define CERT_BASE		"/etc/ssl/private"

#define SSL_PORT		443
//#define SSL_PEM_FILE		"/etc/ssl/private/haproxy.pem"
#define SSL_PEM_FILE		"/home/kkumar/DLB/SRC/haproxy.pem"
#define DH_PARAM		2048

#define CIPHERS			"AES:ALL:!aNULL:!eNULL:+RC4:@STRENGTH"
#define CIPHER_OPTIONS		"no-sslv3 no-tlsv10"

#define COMPRESSION_ALGO	"gzip"
#define COMPRESSION_TYPE	"text/html text/plain text/javascript application/javascript application/xml text/css application/octet-stream"

/* Maximum separate frontend sections */
#define MAX_FRONTENDS		10

/* Maximum server per frontend */
#define MAX_SERVERS		32

#define REVERSE_PROXY		"reverse"
#define FORWARD_PROXY		"forward"

#define MAX_LEN			80

#define SERVER_NAME		"nginx-"
#define CONFIG_FILE		"/tmp/haproxy.cfg"

#define	MAX_LINES		10000
#define MAX_VALUE		128

#define SKIP_ONE_QUOTE		1
#define SKIP_THREE_QUOTES	3

#define DEFAULT_MAXCONN		10000

/* Max backends per frontend, due to ACL's */
#define MAX_BACKENDS		20

/* Input xml file is parsed to a key/value pair array */
struct xml_data {
	char key[MAX_VALUE];
	char value[MAX_VALUE];
};

/* Structure of a backend configuration */
struct proxy_backend {
	int			num_servers;
	int			is_forward;
	char			vip_server_ips[MAX_SERVERS][MAX_LEN];
	char			vip_server_name[MAX_SERVERS][MAX_LEN];
	int			vip_server_maxconn[MAX_SERVERS];
	char			vip_forward_ips[MAX_SERVERS][MAX_LEN];
	char			vip_backend_names[MAX_BACKENDS][MAX_LEN];
};


/* Structure of a frontend server */
struct proxy_frontend {
	char			vip_ips[MAX_LEN];
	int			vip_port;
	int			vip_maxconn;
	char			vip_frontend_name[MAX_LEN];
	struct proxy_backend	vip_backend;
};

/* List of special commands present in the configuration file */
#define VIP_IP_PORT			"VIPS-IP-PORT"
#define NBPROC				"global-nbproc"
#define GLOBAL_MAXCONN			"global-maxconn"
#define DEFAULTS_MAXCONN		"defaults-maxconn"
#define DEFAULT_TIMEOUT_CONNECT		"defaults-timeout-connect"
#define DEFAULT_TIMEOUT_CLIENT		"defaults-timeout-client"
#define DEFAULT_TIMEOUT_SERVER		"defaults-timeout-server"

#endif
