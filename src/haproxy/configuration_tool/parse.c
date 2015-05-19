#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "parse.h"

static int forward_frontend_num = 1, forward_backend_num = 1;
static int reverse_frontend_num = 1, reverse_backend_num = 1;

static char vip_ips[MAX_VIPS][MAX_LEN], server_ips[MAX_SERVERS][MAX_LEN];
static int  vip_ports[MAX_VIPS], server_ports[MAX_SERVERS];

static int sport[MAX_SERVERS], eport[MAX_SERVERS];
static char source_ips[MAX_SERVERS][MAX_LEN];

static int server_count = 1;

int min(int a, int b)
{
	return a < b ? a : b;
}

int initialize()
{
	int numcpus = 1;
	int cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (cpus > 0)
		numcpus = min(MAX_NBPROC, cpus);
	return numcpus;
}

void write_cpu_map(FILE *fp, int max_cpus)
{
	int i = 1;

	while (i <= max_cpus) {
		fprintf(fp, "\tcpu-map %d %d\n", i, i-1);
		i++;
	}
}

void write_global_static(FILE *fp, int numcpus)
{
	fprintf(fp, "# Global section\n");
	fprintf(fp, "global\n");
	fprintf(fp, "\tlog 127.0.0.1\tlocal0\n");
	fprintf(fp, "\tlog 127.0.0.1\tlocal1 notice\n");
	fprintf(fp, "\tdaemon\n\tquiet\n\tnbproc %d\n", numcpus);
	fprintf(fp, "\tuser %s\n\tgroup %s\n", USER, GROUP);
	fprintf(fp, "\tstats socket %s mode 600 level admin\n", HAPROXY_SOCK);
	fprintf(fp, "\tstats timeout %s\n", STATS_TIMEOUT);
	write_cpu_map(fp, numcpus);
	fprintf(fp, "\tca-base %s\n", CA_BASE);
	fprintf(fp, "\tcrt-base %s\n", CERT_BASE);
	fprintf(fp, "\tssl-default-bind-ciphers %s\n", CIPHERS);
	fprintf(fp, "\tssl-default-bind-options %s\n\n", CIPHER_OPTIONS);
}

void write_userlist_static(FILE *fp)
{
	fprintf(fp, "# List of users\n");
	fprintf(fp, "userlist stats-auth\n");
	fprintf(fp, "\tgroup admin users admin\n");
	fprintf(fp, "\tuser  admin insecure-password admin\n");
	fprintf(fp, "\tgroup readonly  users user\n");
	fprintf(fp, "\tuser user insecure-password user\n\n");
}

void write_default_static(FILE *fp)
{
	fprintf(fp, "# Default section\n");
	fprintf(fp, "defaults\n");
	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\toption forwardfor\n");
	fprintf(fp, "\tretries 3\n");
	fprintf(fp, "\toption redispatch\n");
	fprintf(fp, "\tmaxconn 60000\n");
	fprintf(fp, "\toption splice-auto\n");
	fprintf(fp, "\toption prefer-last-server\n");
	fprintf(fp, "\ttimeout connect 5000ms\n");
	fprintf(fp, "\ttimeout client 50000ms\n");
	fprintf(fp, "\ttimeout server 50000ms\n");
	fprintf(fp, "\tcompression offload\n");
	fprintf(fp, "\tcompression algo %s\n", COMPRESSION_ALGO);
	fprintf(fp, "\tcompression type %s\n\n", COMPRESSION_TYPE);
}

void save_stats_once(FILE *fp)
{
	static int first_time = 1;

	if (first_time) {
		first_time = 0;

		fprintf(fp, "\tstats uri /stats\n");
		fprintf(fp, "\tstats enable\n");
		fprintf(fp, "\tacl AUTH http_auth(stats-auth)\n");
		fprintf(fp, "\tacl AUTH_ADMIN http_auth(stats-auth) admin\n");
		fprintf(fp, "\tstats http-request auth unless AUTH\n");
	}
}

void write_frontend_reverse(FILE *fp, char *type, int num,
			    char vips[MAX_VIPS][MAX_LEN], int ports[MAX_VIPS])
{
	int i;

	fprintf(fp, "# Reverse Proxy frontend section %d\n",
		reverse_frontend_num);
	fprintf(fp, "frontend www-http-%s-%d\n", type, reverse_frontend_num);
	fprintf(fp, "\tbind ");

	for (i = 0; i < num; i++) {
		if (i)
			fprintf(fp, ",");
		fprintf(fp, "%s:%d", vips[i], ports[i]);
	}
	fprintf(fp, "\n");

	save_stats_once(fp);

	fprintf(fp, "\tdefault_backend www-backend-%s-%d\n\n",
		type, reverse_frontend_num++);
}

void write_frontend_forward(FILE *fp, char *type, int num,
			    char vips[MAX_VIPS][MAX_LEN], int ports[MAX_VIPS])
{
	int i;

	fprintf(fp, "# Forward Proxy frontend section %d\n",
		forward_frontend_num);
	fprintf(fp, "frontend www-http-%s-%d\n", type, forward_frontend_num);
	fprintf(fp, "\tbind ");

	for (i = 0; i < num; i++) {
		if (i)
			fprintf(fp, ",");
		fprintf(fp, "%s:%d", vips[i], ports[i]);
	}
	fprintf(fp, "\n");

	save_stats_once(fp);

	fprintf(fp, "\n\tdefault_backend www-backend-%s-%d\n\n",
		type, forward_frontend_num++);
}

void write_frontend(FILE *fp, char *type, int num,
		    char vips[MAX_VIPS][MAX_LEN], int ports[MAX_VIPS])
{
	if (!strcmp(type, REVERSE_PROXY))
		write_frontend_reverse(fp, type, num, vips, ports);
	else
		write_frontend_forward(fp, type, num, vips, ports);
}

void set_ip_ports(char ips[MAX_VIPS][MAX_LEN], int ports[MAX_VIPS], int max,
		  int type)
{
	int i;
	static int vport = 80;
	static int count = 1;
	static int sport = 4040;
	int port;
	char *vip = "192.168.122";
	char *sip = "10.0.0";
	char *ip;

	if (type == VIP) {
		port = vport;
		ip = vip;
	} else {
		port = sport;
		ip = sip;
	}

	for (i = 0; i < max; i++) {
		sprintf(ips[i], "%s.%d", ip, count++);
		ports[i] = port++;
	}
}

void write_static(FILE *fp, int numcpus)
{
	write_global_static(fp, numcpus);
	write_userlist_static(fp);
	write_default_static(fp);
}

void write_backend_reverse(FILE *fp, char *type, int num,
			   char ips[MAX_SERVERS][MAX_LEN],
			   int ports[MAX_SERVERS])
{
	int i;

	fprintf(fp, "# Reverse Proxy backend section %d\n",
		reverse_backend_num);
	fprintf(fp, "backend www-backend-%s-%d\n", type, reverse_backend_num++);
	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\tmaxconn 60000\n");
	fprintf(fp, "\tbalance roundrobin\n");
	fprintf(fp, "\toption forwardfor\n");	/* Needed? */
	fprintf(fp, "\tcookie FKSID prefix indirect nocache\n");
	for (i = 0; i < num; i++) {
		fprintf(fp, "\tserver %s%d %s:%d maxconn 10000 check\n",
			SERVER_NAME, server_count++, ips[i], ports[i]);
	}
	fprintf(fp, "\n");
}

void write_backend_forward(FILE *fp, char *type, int num,
			   char ips[MAX_SERVERS][MAX_LEN],
			   int ports[MAX_SERVERS],
			   char sips[MAX_SERVERS][MAX_LEN],
			   int sport[MAX_SERVERS], int eport[MAX_SERVERS])
{
	int i;

	fprintf(fp, "# Forward Proxy backend section %d\n",
		forward_backend_num);
	fprintf(fp, "backend www-backend-%s-%d\n", type, forward_backend_num++);
	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\tmaxconn 60000\n");
	fprintf(fp, "\tbalance roundrobin\n");
	fprintf(fp, "\toption forwardfor\n");	/* Needed? */
	fprintf(fp, "\tcookie FKSID prefix indirect nocache\n");

	for (i = 0; i < num; i++) {
		fprintf(fp, "\tserver %s%d %s:%d maxconn 10000 source %s:%d-%d check\n",
			SERVER_NAME, server_count++, ips[i], ports[i],
			sips[i], sport[i], eport[i]);
	}
}

void write_backend(FILE *fp, char *type, int num,
		   char ips[MAX_SERVERS][MAX_LEN], int ports[MAX_SERVERS],
		   char sips[MAX_SERVERS][MAX_LEN], int sport[MAX_SERVERS],
		   int eport[MAX_SERVERS])
{
	if (!strcmp(type, REVERSE_PROXY))
		write_backend_reverse(fp, type, num, ips, ports);
	else
		write_backend_forward(fp, type, num, ips, ports, sips, sport,
				      eport);
}

void set_source_ips(char sip[MAX_SERVERS][MAX_LEN], int *sport, int *dport,
		    int max)
{
	int i;
	int s = 10000;
	int d = 20000;

	for (i = 0; i < max; i++) {
		sport[i] = s;
		dport[i] = d;
		s += 10000;
		d += 10000;
		sprintf(sip[i], "16.10.20.%d", rand() % 255);
	}
}

int test_config(char *file)
{
	char cmd[1024];

	sprintf(cmd, "haproxy -c -f %s", file);
	return system(cmd);
}

int mv_config_file(char *cfile, char *path)
{
	return 0;

	char cmd[1024];

	sprintf(cmd, "mv %s/%s %s/%s.prev 2>/dev/null",
		path, cfile, path, cfile);
	if (system(cmd))
		return 1;

	sprintf(cmd, "mv %s %s 2>/dev/null", cfile, path);
	if (system(cmd)) {
		/* Restore previous configuration */
		sprintf(cmd, "mv %s/%s %s/%s.prev 2>/dev/null",
			path, cfile, path, cfile);
		system(cmd);
		return 1;
	}

	sprintf(cmd, "%s/%s", path, cfile);
	return test_config(cmd);
}

int reload_haproxy(void)
{
	return 0;

	char cmd[1024];

	sprintf(cmd, "/etc/init.d/haproxy reload > /dev/null 2>&1");
	return system(cmd);
}

int config_file_done(char *cfile)
{
	int ret = 0;

	if (test_config(CONFIG_FILE)) {
		fprintf(stderr, "Config file generated is bad\n");
		ret = 1;
	} else {
		if (mv_config_file(CONFIG_FILE, CONFIG_PATH)) {
			fprintf(stderr, "Error moving configuration file\n");
			ret = 1;
		} else if (reload_haproxy()) {
			fprintf(stderr, "Error reloading haproxy\n");
			ret = 1;
		}
	}

	return ret;
}

int main(void)
{
	int numcpus, ret;
	FILE *fp = fopen(CONFIG_FILE, "w");

	if (!fp) {
		perror(CONFIG_FILE);
		exit(1);
	}

	numcpus = initialize();

	/*
	 * Actual structure of program is:
	 * while read input
	 *	parse() one section
	 *	save() one section
	 */
	write_static(fp, numcpus);

	set_ip_ports(vip_ips, vip_ports, 5, VIP);
	write_frontend(fp, REVERSE_PROXY, 5, vip_ips, vip_ports);

	set_ip_ports(vip_ips, vip_ports, 3, VIP);
	write_frontend(fp, REVERSE_PROXY, 3, vip_ips, vip_ports);

	set_ip_ports(vip_ips, vip_ports, 1, VIP);
	write_frontend(fp, REVERSE_PROXY, 1, vip_ips, vip_ports);

	set_ip_ports(vip_ips, vip_ports, 1, VIP);
	write_frontend(fp, FORWARD_PROXY, 1, vip_ips, vip_ports);

	set_ip_ports(server_ips, server_ports, 8, SIP);
	write_backend(fp, REVERSE_PROXY, 8, server_ips, server_ports,
		      NULL, NULL, NULL);

	set_ip_ports(server_ips, server_ports, 3, SIP);
	write_backend(fp, REVERSE_PROXY, 3, server_ips, server_ports,
		      NULL, NULL, NULL);

	set_ip_ports(server_ips, server_ports, 8, SIP);
	write_backend(fp, REVERSE_PROXY, 8, server_ips, server_ports,
		      NULL, NULL, NULL);

	set_ip_ports(server_ips, server_ports, 2, SIP);
	set_source_ips(source_ips, sport, eport, 2);
	write_backend(fp, FORWARD_PROXY, 2, server_ips, server_ports,
		      source_ips, sport, eport);

	fclose(fp);

	return config_file_done(CONFIG_FILE);
}
