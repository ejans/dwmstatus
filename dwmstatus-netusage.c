#define _BSD_SOURCE
#define BATT_NOW        "/sys/class/power_supply/BAT0/charge_now"
#define BATT_FULL       "/sys/class/power_supply/BAT0/charge_full"
#define BATT_STATUS       "/sys/class/power_supply/BAT0/status"
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
mktimes(char *fmt, char *tzname)
{
	char buf[129];
	time_t tim;
	struct tm *timtm;

	bzero(buf, sizeof(buf));
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
main(void)
{
	char *status;
	char *avgs;
	//char *tmar;
	//char *tmutc;
	//char *tmbln;
	char *tmbsls;
	char *netstats;
	char *batstats;
	static unsigned long long int rec, sent;

	if (!(dpy = XOpenDisplay(NULL))) {
		fprintf(stderr, "dwmstatus: cannot open display.\n");
		return 1;
	}

	parse_netdev(&rec, &sent);
	//for (;;sleep(1)) {
	for (;;sleep(2)) {
		avgs = loadavg();
		//tmar = mktimes("%H:%M", tzargentina);
		//tmutc = mktimes("%H:%M", tzutc);
		//tmbln = mktimes("KW %W %a %d %b %H:%M %Z %Y", tzberlin);
		tmbsls = mktimes("KW %W %a %d %b %H:%M %Z %Y", tzbrussels);
		netstats = get_netusage(&rec, &sent);
		batstats = getbattery();

		//status = smprintf("L: %s|N: %s|B: %s|A: %s|U: %s|%s",
				//avgs, netstats, batstats, tmar, tmutc, tmbln);
		status = smprintf("L: %s|N: %s|B: %s|%s",
				avgs, netstats, batstats, tmbsls);
		setstatus(status);
		free(avgs);
		//free(tmar);
		//free(tmutc);
		//free(tmbln);
		free(tmbsls);
		free(status);
	}

	XCloseDisplay(dpy);

	return 0;
}
