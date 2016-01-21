#define _BSD_SOURCE
#define BATT_NOW        "/sys/class/power_supply/BAT0/energy_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/energy_full"
#define BATT_STATUS       "/sys/class/power_supply/BAT0/status"
#define BATT2_NOW        "/sys/class/power_supply/BAT1/energy_now"
#define BATT2_FULL       "/sys/class/power_supply/BAT1/energy_full"
#define BATT2_STATUS       "/sys/class/power_supply/BAT1/status"
#include <unistd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <sys/time.h>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#include <X11/Xlib.h>

char *tzargentina = "America/Buenos_Aires";
char *tzutc = "UTC";
char *tzberlin = "Europe/Berlin";
char *tzbrussels = "Europe/Brussels";

static Display *dpy;
long cpu0_work = 0;
long cpu0_total = 0;
long cpu1_work = 0;
long cpu1_total = 0;
long cpu2_work = 0;
long cpu2_total = 0;
long cpu3_work = 0;
long cpu3_total = 0;

char *
smprintf(char *fmt, ...)
{
	va_list fmtargs;
	char *ret;
	int len;

	va_start(fmtargs, fmt);
	len = vsnprintf(NULL, 0, fmt, fmtargs);
	va_end(fmtargs);

	ret = malloc(++len);
	if (ret == NULL) {
		perror("malloc");
		exit(1);
	}

	va_start(fmtargs, fmt);
	vsnprintf(ret, len, fmt, fmtargs);
	va_end(fmtargs);

	return ret;
}

void
settz(char *tzname)
{
	setenv("TZ", tzname, 1);
}

int
parse_netdev(unsigned long long int *receivedabs, unsigned long long int *sentabs)
{
	char buf[255];
	char *datastart;
	static int bufsize;
	int rval;
	FILE *devfd;
	unsigned long long int receivedacc, sentacc;

	bufsize = 255;
	devfd = fopen("/proc/net/dev", "r");
	rval = 1;

	// Ignore the first two lines of the file
	fgets(buf, bufsize, devfd);
	fgets(buf, bufsize, devfd);

	while (fgets(buf, bufsize, devfd)) {
		if ((datastart = strstr(buf, "lo:")) == NULL) {
			datastart = strstr(buf, ":");

			// With thanks to the conky project at http://conky.sourceforge.net/
			sscanf(datastart + 1, "%llu  %*d     %*d  %*d  %*d  %*d   %*d        %*d       %llu",\
					&receivedacc, &sentacc);
			*receivedabs += receivedacc;
			*sentabs += sentacc;
			rval = 0;
		}
	}

	fclose(devfd);
	return rval;
}

void
calculate_speed(char *speedstr, unsigned long long int newval, unsigned long long int oldval)
{
	double speed;
	speed = (newval - oldval) / 1024.0;
	if (speed > 1024.0) {
		speed /= 1024.0;
		sprintf(speedstr, "%.3f MB/s", speed);
	} else {
		sprintf(speedstr, "%.2f KB/s", speed);
	}
}

char *
get_netusage(unsigned long long int *rec, unsigned long long int *sent)
{
	unsigned long long int newrec, newsent;
	newrec = newsent = 0;
	char downspeedstr[15], upspeedstr[15];
	static char retstr[42];
	int retval;

	retval = parse_netdev(&newrec, &newsent);
	if (retval) {
		fprintf(stdout, "Error when parsing /proc/net/dev file.\n");
		exit(1);
	}

	calculate_speed(downspeedstr, newrec, *rec);
	calculate_speed(upspeedstr, newsent, *sent);

	sprintf(retstr, "down: %s up: %s", downspeedstr, upspeedstr);

	*rec = newrec;
	*sent = newsent;
	return retstr;
}

char *
getbattery()
{
	long lnum1, lnum2 = 0;
	char *status = malloc(sizeof(char)*12);
	char s = '?';
	FILE *fp = NULL;
	if ((fp = fopen(BATT_NOW, "r"))) {
		fscanf(fp, "%ld\n", &lnum1);
		fclose(fp);
		fp = fopen(BATT_FULL, "r");
		fscanf(fp, "%ld\n", &lnum2);
		fclose(fp);
		fp = fopen(BATT_STATUS, "r");
		fscanf(fp, "%s\n", status);
		fclose(fp);
		if (strcmp(status,"Charging") == 0)
			s = '+';
		if (strcmp(status,"Discharging") == 0)
			s = '-';
		if (strcmp(status,"Full") == 0)
			s = '=';
		return smprintf("%c%ld%%", s,(lnum1/(lnum2/100)));
	}
	else return smprintf("");
}

char *
getbattery2()
{
	long lnum1, lnum2 = 0;
	char *status = malloc(sizeof(char)*12);
	char s = '?';
	FILE *fp = NULL;
	if ((fp = fopen(BATT2_NOW, "r"))) {
		fscanf(fp, "%ld\n", &lnum1);
		fclose(fp);
		fp = fopen(BATT2_FULL, "r");
		fscanf(fp, "%ld\n", &lnum2);
		fclose(fp);
		fp = fopen(BATT2_STATUS, "r");
		fscanf(fp, "%s\n", status);
		fclose(fp);
		if (strcmp(status,"Charging") == 0)
			s = '+';
		if (strcmp(status,"Discharging") == 0)
			s = '-';
		if (strcmp(status,"Full") == 0)
			s = '=';
		return smprintf("%c%ld%%", s,(lnum1/(lnum2/100)));
	}
	else return smprintf("");
}

char *
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	bzero(buf, sizeof(buf));
	// If we don't set the timezone it will be gotten from /etc/timezone
	settz(tzname);
	tim = time(NULL);
	timtm = localtime(&tim);
	if (timtm == NULL) {
		perror("localtime");
		exit(1);
	}

	if (!strftime(buf, sizeof(buf)-1, fmt, timtm)) {
		fprintf(stderr, "strftime == 0\n");
		exit(1);
	}

	return smprintf("%s", buf);
}

void
setstatus(char *str)
{
	XStoreName(dpy, DefaultRootWindow(dpy), str);
	XSync(dpy, False);
}

char *
loadavg(void)
{
	double avgs[3];

	if (getloadavg(avgs, 3) < 0) {
		perror("getloadavg");
		exit(1);
	}

	return smprintf("%.2f %.2f %.2f", avgs[0], avgs[1], avgs[2]);
}

int
readInt(char *input)
{
	FILE *fd;
	int val;

	fd = fopen(input, "r");
	if (fd==NULL)
		return -1;
	fscanf(fd, "%d", &val);
	fclose(fd);
	return val;
}

char *
getcpu(void)
{
	FILE *fd;
	long jif1, jif2, jif3, jif4, jif5, jif6, jif7;
	long work0, total0, work1, total1, work2, total2, work3, total3;
	int load0, load1, load2, load3, freq;
	char *color0, *color1;

	// ---- LOAD
	fd = fopen("/proc/stat", "r");
	char c;
	while (c != '\n') c = fgetc(fd);
	fscanf(fd, "cpu0 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
	work0 = jif1 + jif2 + jif3 + jif6 + jif7;
	total0 = work0 + jif4 + jif5;

	c = 0;
	while (c != '\n') c = fgetc(fd);
	fscanf(fd, "cpu1 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
	work1 = jif1 + jif2 + jif3 + jif6 + jif7;
	total1 = work1 + jif4 + jif5;

	c = 0;
	while (c != '\n') c = fgetc(fd);
	fscanf(fd, "cpu2 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
	work2 = jif1 + jif2 + jif3 + jif6 + jif7;
	total2 = work2 + jif4 + jif5;

	c = 0;
	while (c != '\n') c = fgetc(fd);
	fscanf(fd, "cpu3 %ld %ld %ld %ld %ld %ld %ld", &jif1, &jif2, &jif3, &jif4, &jif5, &jif6, &jif7);
	work3 = jif1 + jif2 + jif3 + jif6 + jif7;
	total3 = work3 + jif4 + jif5;

	fclose(fd);

	load0 = 100 * (work0 - cpu0_work) / (total0 - cpu0_total);
	load1 = 100 * (work1 - cpu1_work) / (total1 - cpu1_total);
	load2 = 100 * (work2 - cpu2_work) / (total2 - cpu2_total);
	load3 = 100 * (work3 - cpu3_work) / (total3 - cpu3_total);

	cpu0_work = work0;
	cpu0_total = total0;
	cpu1_work = work1;
	cpu1_total = total1;
	cpu2_work = work2;
	cpu2_total = total2;
	cpu3_work = work3;
	cpu3_total = total3;

	return smprintf("%1d%% %1d%% %1d%% %1d%%", load0, load1, load2, load3);
}

char*
getmem(void)
{
	FILE *fd;
	long total, free, available;
	int used, amt;

	fd = fopen("/proc/meminfo", "r");
	fscanf(fd, "MemTotal: %ld kB\n", &total);
	fscanf(fd, "MemFree: %ld kB\n", &free);
	fscanf(fd, "MemAvailable: %ld kB\n", &available);
	fclose(fd);
	used = 100 * (total - available) / total;
	amt = (total - available) / 1024;
	//amt = (total - available);
	//return snprintf(status, size, "\x08""M\x05%d%%", used);
	return smprintf("%d%% (%dMB)", used, amt);
}

int
main(void)
{
	char *status;
	char *avgs;
	char *cpu;
	char *mem;
	char *tmbsls;
	char *netstats;
	char *batstats;
	char *batstats2;
	static unsigned long long int rec, sent;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec, &sent);
	for (;;sleep(1)) {
		//for (;;sleep(2)) {
		avgs = loadavg();
		cpu = getcpu();
		mem = getmem();
		tmbsls = mktimes("CW %V %a %d %b %H:%M %Z %Y", tzbrussels);
		netstats = get_netusage(&rec, &sent);
		batstats = getbattery();
		batstats2 = getbattery2();
		status = smprintf("P: %s | M: %s | N: %s | B1: %s B2: %s | %s",
				cpu, mem, netstats, batstats2, batstats, tmbsls);
		setstatus(status);
		free(avgs);
		free(tmbsls);
		free(cpu);
		free(mem);
		free(batstats);
		free(batstats2);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
