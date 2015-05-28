#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include "parse.h"

/* TODO: Add vip-process (which process handles) for each section */

static struct proxy_frontend frontends[MAX_FRONTENDS];
static struct xml_data xml_input[MAX_LINES];
static int frontend_count = 0;
static int max_input_lines;
static int nbproc, set_ssl_params;

int min(int a, int b)
{
	return a < b ? a : b;
}

int initialize_system()
{
	int numcpus = 1;
	int cpus = sysconf(_SC_NPROCESSORS_CONF);

	if (cpus > 0)
		numcpus = min(MAX_NBPROC, cpus);
	return numcpus;
}

char *get_string(char *ptr, int num_to_skip)
{
	char *head;

	do {
		while (*ptr != '"') {
			ptr++;
			if (!*ptr)
				return NULL;
		}
		ptr++;
	} while (--num_to_skip > 0);

	head = ptr;
	if (!*head)
		return NULL;

	while (*ptr != '"') {
		ptr++;
		if (!*ptr)
			return NULL;
	}

	return strndup(head, ptr - head);
}

char *get_key(char *ptr)
{
	return get_string(ptr, SKIP_ONE_QUOTE);
}

char *get_value(char *ptr)
{
	return get_string(ptr, SKIP_THREE_QUOTES);
}

int look_for_param(char *string)
{
	int sindex = 0;

	while (sindex < max_input_lines) {
		if (!strcmp(xml_input[sindex].key, string))
			return sindex;
		sindex++;
	}

	return -1;
}

int get_port_no(char *start, char *end)
{
	char s[5];

	strncpy(s, start, min(sizeof(s), end - start));
	return atoi(s);
}

/*
 * Validate that user passed same ports for all ip:port's. Return -1
 * on error, else the common port#.
 */
int validate_ip_port(char *ip)
{
	int port = -1;

	while (ip) {
		char *start = strchr(ip, ':');
		char *end;
		int tmp_port;

		if (!start)
			break;
		start++;

		end = strchr(start, ',');
		if (!end)
			tmp_port = atoi(start);
		else
			tmp_port = get_port_no(start, end);

		if (port == -1) {
			port = tmp_port;
		} else if (port != tmp_port) {
			port = -1;
			break;
		}
		ip = end;
	}

	return port;
}

void print_table()
{
	int sindex = 0;

	while (sindex < max_input_lines) {
		printf("Key: %s Val: %s\n", xml_input[sindex].key,
			xml_input[sindex].value);
		sindex++;
	}
}

void write_cpu_map(FILE *fp, int val)
{
	int i;

	/* Need to see how many processors, and print either 0,1,2 or 0,2,4 */
	for (i = 0; i < val; i++)
		fprintf(fp, "\tcpu-map %d %d\n", i+1, i);
}

void write_global_static(FILE *fp, int numcpus)
{
	int index;

	fprintf(fp, "# Global section\n");
	fprintf(fp, "global\n");
	fprintf(fp, "\tlog 127.0.0.1\tlocal0\n");
	fprintf(fp, "\tlog 127.0.0.1\tlocal1 notice\n");
	fprintf(fp, "\tdaemon\n\tquiet\n");
	fprintf(fp, "\tuser %s\n\tgroup %s\n", USER, GROUP);
	fprintf(fp, "\tstats socket %s mode 600 level admin\n", HAPROXY_SOCK);
	fprintf(fp, "\tstats timeout %s\n", STATS_TIMEOUT);

	if ((index = look_for_param(GLOBAL_MAXCONN)) != -1)
		fprintf(fp, "\tmaxconn %s\n", xml_input[index].value);
	else
		fprintf(fp, "\tmaxconn %d\n", DEFAULT_MAXCONN);

	if ((index = look_for_param(NBPROC)) != -1)
		nbproc = atoi(xml_input[index].value);
	else
		nbproc = 1;

	fprintf(fp, "\tnbproc %d\n", nbproc);

	write_cpu_map(fp, atoi(xml_input[index].value));

	if (set_ssl_params) {
		fprintf(fp, "\ttune.ssl.default-dh-param %d\n", DH_PARAM);
		fprintf(fp, "\tca-base %s\n", CA_BASE);
		fprintf(fp, "\tcrt-base %s\n", CERT_BASE);
		fprintf(fp, "\tssl-default-bind-ciphers %s\n", CIPHERS);
		fprintf(fp, "\tssl-default-bind-options %s\n\n",
			CIPHER_OPTIONS);
	} else
		fprintf(fp, "\n");
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
	int index;

	fprintf(fp, "# Default section\n");
	fprintf(fp, "defaults\n");
	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\toption forwardfor\n");
	fprintf(fp, "\tretries 3\n");
	fprintf(fp, "\toption redispatch\n");

	if ((index = look_for_param(DEFAULTS_MAXCONN)) != -1)
		fprintf(fp, "\tmaxconn %s\n", xml_input[index].value);

	fprintf(fp, "\toption splice-auto\n");
	fprintf(fp, "\toption prefer-last-server\n");

	if ((index = look_for_param(DEFAULT_TIMEOUT_CONNECT)) != -1)
		fprintf(fp, "\ttimeout connect %s\n", xml_input[index].value);
	else
		fprintf(fp, "\ttimeout connect %s\n", DEFAULT_TIMEOUT_CONNECT);

	if ((index = look_for_param(DEFAULT_TIMEOUT_CLIENT)) != -1)
		fprintf(fp, "\ttimeout client %s\n", xml_input[index].value);
	else
		fprintf(fp, "\ttimeout client %s\n", DEFAULT_TIMEOUT_CLIENT);

	if ((index = look_for_param(DEFAULT_TIMEOUT_SERVER)) != -1)
		fprintf(fp, "\ttimeout server %s\n", xml_input[index].value);
	else
		fprintf(fp, "\ttimeout server %s\n", DEFAULT_TIMEOUT_SERVER);

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

// ssl crt /etc/ssl/private/haproxy.pem

void write_bind_lines(FILE *fp, int index)
{
	int i;

	for (i = 0; i < nbproc; i++) {
		if (frontends[index].vip_port == SSL_PORT) {
			fprintf(fp, "\tbind %s process %d ssl crt %s\n",
				frontends[index].vip_ips, i + 1, SSL_PEM_FILE);
		} else {
			fprintf(fp, "\tbind %s process %d\n",
				frontends[index].vip_ips, i + 1);
		}
	}
}

void write_frontend_reverse(FILE *fp, char *type, int index)
{
	int i;

	fprintf(fp, "# Reverse proxy frontend \n");
	fprintf(fp, "frontend %s\n", frontends[index].vip_frontend_name);

	write_bind_lines(fp, index);

	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\tmaxconn %d\n", frontends[index].vip_maxconn);

	save_stats_once(fp);

	fprintf(fp, "\tdefault_backend %s\n\n",
		frontends[index].vip_backend.vip_backend_names[0]);
}

void write_frontend_forward(FILE *fp, char *type, int index)
{
	int i;

	fprintf(fp, "# Forward proxy frontend \n");
	fprintf(fp, "frontend %s\n", frontends[index].vip_frontend_name);

	write_bind_lines(fp, index);

	fprintf(fp, "\tmode tcp\n");
	fprintf(fp, "\tmaxconn %d\n", frontends[index].vip_maxconn);

	save_stats_once(fp);

	fprintf(fp, "\tdefault_backend %s\n\n",
		frontends[index].vip_backend.vip_backend_names[0]);
}

void write_backend_reverse(FILE *fp, char *type, int index)
{
	int i;

	fprintf(fp, "# Reverse proxy backend \n");
	fprintf(fp, "backend %s\n",
		frontends[index].vip_backend.vip_backend_names[0]);

	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\tbalance roundrobin\n");
	fprintf(fp, "\toption forwardfor\n");
	fprintf(fp, "\tcookie FKSID prefix indirect nocache\n");
	for (i = 0; i < frontends[index].vip_backend.num_servers; i++) {
		fprintf(fp, "\tserver %s %s maxconn %d check\n",
		frontends[index].vip_backend.vip_server_name[i],
		frontends[index].vip_backend.vip_server_ips[i],
		frontends[index].vip_backend.vip_server_maxconn[i]);
	}
	fprintf(fp, "\n");
}

void write_backend_forward(FILE *fp, char *type, int index)
{
	int i;

	fprintf(fp, "# Forward proxy backend \n");
	fprintf(fp, "backend %s\n",
		frontends[index].vip_backend.vip_backend_names[0]);

	fprintf(fp, "\tmode http\n");
	fprintf(fp, "\tbalance roundrobin\n");
	fprintf(fp, "\toption forwardfor\n");
	fprintf(fp, "\tcookie FKSID prefix indirect nocache\n");
	for (i = 0; i < frontends[index].vip_backend.num_servers; i++) {
		fprintf(fp, "\tserver %s %s source %s maxconn %d check\n",
		frontends[index].vip_backend.vip_server_name[i],
		frontends[index].vip_backend.vip_server_ips[i],
		frontends[index].vip_backend.vip_forward_ips[i],
		frontends[index].vip_backend.vip_server_maxconn[i]);
	}
	fprintf(fp, "\n");
}

void write_global_default_userlist(FILE *fp, int numcpus)
{
	write_global_static(fp, numcpus);
	write_userlist_static(fp);
	write_default_static(fp);
}

int test_config(char *file)
{
	char cmd[1024];

	sprintf(cmd, "haproxy -c -f %s", file);
	return system(cmd);
}

int test_and_activate_config(char *file)
{
	int ret = 1;

	if (test_config(file)) {
		printf("Config file generated is bad\n");
	} else {
		char cmd[1024];

		/* mv file, reload haproxy */
		printf("Successfully created configuration file\n");

		printf("Remove following line on a server\n");
		return 0;

		sprintf(cmd, "mv /etc/haproxy/haproxy.cfg /etc/haproxy/haproxy.cfg.org 2> /dev/null");
		system(cmd);
		sprintf(cmd, "mv %s /etc/haproxy/haproxy.cfg 2> /dev/null",
			file);
		if (system(cmd)) {
			/* Unable to mv new file, undo earlier mv */
			sprintf(cmd, "mv /etc/haproxy/haproxy.cfg.org /etc/haproxy/haproxy.cfg 2> /dev/null");
			system(cmd);
		}
		printf("Successfully moved configuration file\n");

		sprintf(cmd, "service /etc/init.d/haproxy reload > /dev/null 2>&1");
		ret = system(cmd);
	}

	return ret;
}

void parse_line(char *line, int index)
{
	char *val;

	strcpy(xml_input[index].key, get_key(line));
	strcpy(xml_input[index].value, get_value(line));
}

/* Return true if the first non-space character is '#' */
int is_comment(char *line)
{
	while (*line) {
		if (!isspace(*line))
			return *line == '#';
		line++;
	}

	return 0;
}

int bad_line(char *line)
{
	if (line[strlen(line) - 1] == '\n')
		line[strlen(line) - 1] = 0;
	return !*line || is_comment(line) || !strcmp(line, "{")
		|| !strcmp(line, "}");
}

void read_input(int max)
{
	int i = 0;
	char line[MAX_VALUE];

	while (i < max && fgets(line, MAX_VALUE, stdin) != NULL) {
		if (bad_line(line))
			continue;
		parse_line(line, i);
		i++;
	}

	max_input_lines = i;
}

void initialize_entry(int xml_index)
{
	int i, j;

	for (i = 0; i < MAX_FRONTENDS; i++) {
		frontends[frontend_count].vip_ips[0] = 0;
		frontends[frontend_count].vip_maxconn = 1000;
		frontends[frontend_count].vip_backend.num_servers = 0;
		for (j = 0; j < MAX_SERVERS; j++) {
			frontends[frontend_count].vip_backend.\
				vip_server_ips[j][0] = 0;
			frontends[frontend_count].vip_backend.\
				vip_server_name[j][0] = 0;
			frontends[frontend_count].vip_backend.\
				vip_server_maxconn[j] = 1000;
			frontends[frontend_count].vip_backend.\
				vip_forward_ips[j][0] = 0;
		}
	}
}

int save_one_entry_info(int xml_index)
{
	int next_entry = -1;
	int i = frontend_count;
	int serv_num = -1;
	int type = -1;
	int is_forward = 0;
	static int fcounter = 1, bcounter = 1;
char name[MAX_LEN];	/* Not required, use directly */

	initialize_entry(xml_index);

	strcpy(frontends[i].vip_ips, xml_input[xml_index].value);
	frontends[i].vip_port = validate_ip_port(frontends[i].vip_ips);
	if (frontends[i].vip_port == -1) {
		fprintf(stderr, "%s: Bad ip/port combination\n",
			frontends[i].vip_ips);
		exit(1);
	}

	if (frontends[i].vip_port == SSL_PORT)
		set_ssl_params = 1;

	while (++xml_index < max_input_lines) {
		if (!strcmp(xml_input[xml_index].key, "VIPS-IP-PORT")) {
			next_entry = xml_index;
			break;
		} else if (!strcmp(xml_input[xml_index].key, "VIPS-maxconn")) {
			frontends[i].vip_maxconn =
				atoi(xml_input[xml_index].value);
		} else if (!strcmp(xml_input[xml_index].key, "VIPS-server-ip-port")) {
			strcpy(frontends[frontend_count].\
				vip_backend.vip_server_ips[++serv_num],
				xml_input[xml_index].value);
		} else if (!strcmp(xml_input[xml_index].key, "VIPS-server-name")) {
			strcpy(frontends[frontend_count].vip_backend.\
				vip_server_name[serv_num],
				xml_input[xml_index].value);
		} else if (!strcmp(xml_input[xml_index].key, "VIPS-server-maxconn")) {
			frontends[frontend_count].vip_backend.\
				vip_server_maxconn[serv_num] = \
				atoi(xml_input[xml_index].value);
		} else if (!strcmp(xml_input[xml_index].key, "VIPS-server-src-ip-port")) {
			/* Currently order of forward-src-ip is important, it
			 * should come after server-ip.
			 */
			frontends[frontend_count].vip_backend.is_forward = 1;
			is_forward = 1;
			strcpy(frontends[frontend_count].\
				vip_backend.vip_forward_ips[serv_num],
				xml_input[xml_index].value);
		}
	}

	if (is_forward) {
		sprintf(frontends[frontend_count].vip_frontend_name,
			"frontend-%s-%d", "forward", fcounter);
		sprintf(frontends[frontend_count].vip_backend.\
			vip_backend_names[0], "backend-%s-%d",
			"forward", fcounter++);
	} else {
		sprintf(frontends[frontend_count].vip_frontend_name,
			"frontend-%s-%d", "reverse", bcounter);
		sprintf(frontends[frontend_count].vip_backend.\
			vip_backend_names[0], "backend-%s-%d",
			"reverse", bcounter++);
	}

	frontends[frontend_count].vip_backend.num_servers = serv_num + 1;
	frontend_count++;

	/* Whether more VIP's are present or not */
	return next_entry;
}

void save_fe_be(void)
{
	int index;

	index = look_for_param(VIP_IP_PORT);
	while (index != -1)
		index = save_one_entry_info(index);
}

int is_forward_proxy(int index)
{
	return frontends[index].vip_backend.is_forward;
}

void configure_fe_be(FILE *fp)
{
	int i;

	/* Write frontends */
	for (i = 0; i < frontend_count; i++) {
		if (is_forward_proxy(i))
			write_frontend_forward(fp, "forward", i);
		else
			write_frontend_reverse(fp, "reverse", i);
	}

	/* Write backends */
	for (i = 0; i < frontend_count; i++) {
		if (is_forward_proxy(i))
			write_backend_forward(fp, "forward", i);
		else
			write_backend_reverse(fp, "reverse", i);
	}
}

int main(void)
{
	FILE *fp = fopen(CONFIG_FILE, "w");
	int numcpus;

	if (!fp) {
		perror(CONFIG_FILE);
		return 1;
	}

	numcpus = initialize_system();

	read_input(MAX_LINES);

	/* Retrieve the params into parseable arrays */
	save_fe_be();

	write_global_default_userlist(fp, numcpus);

	configure_fe_be(fp);
	fclose(fp);

	return test_and_activate_config(CONFIG_FILE);
}
