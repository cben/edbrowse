/* http.c
 * HTTP protocol client implementation
 * This file is part of the edbrowse project, released under GPL.
 */

#include "eb.h"

#ifdef _MSC_VER
#include <fcntl.h>
#else
#include <sys/wait.h>
#endif
#include <time.h>

char *serverData;
int serverDataLen;
CURL *global_http_handle;
CURLSH *global_share_handle;
bool pluginsOn = true;
bool down_bg;			/* download in background */
char showProgress = 'd';	// dots

static CURL *down_h;
static int down_pid;		/* pid of the downloading child process */
static bool down_permitted;
static int down_msg;

struct BG_JOB {
	struct BG_JOB *next, *prev;
	int pid, state;
	size_t fsize;		// file size
	int file2;		// offset into filename
	char file[4];
};
struct listHead down_jobs = {
	&down_jobs, &down_jobs
};

static void background_download(struct eb_curl_callback_data *data);
static void setup_download(struct eb_curl_callback_data *data);
static CURL *http_curl_init(struct eb_curl_callback_data *cbd);

static char *http_headers;
static int http_headers_len;
static char *httpLanguage;	/* outgoing */
long ht_code;			/* example, 404 */
static char ht_error[CURL_ERROR_SIZE + 1];
/* an assortment of variables that are gleaned from the incoming http headers */
/* http content type is used in many places, and isn't arbitrarily long
 * or case sensitive, so keep our own sanitized copy. */
static char ht_content[60];
static char *ht_charset;	/* extra content info such as charset */
int ht_length;			/* http content length */
static char *ht_cdfn;		/* http content disposition file name */
static time_t ht_modtime;	/* http modification time */
static char *ht_etag;		/* the etag in the header */
static bool ht_cacheable;

/*********************************************************************
This function is called for a new web page, by http refresh,
or by http redirection,
or by document.location = new_url, or by new Window().
If delay is 0 or less then the action should happen now.
-1 is from http header location:
The refresh parameter means replace the current page.
This is false only if js creates a new window, which should stack up on top of the old.
*********************************************************************/

char *newlocation;
int newloc_d;			/* possible delay */
bool newloc_r;			/* replace the buffer */
struct ebFrame *newloc_f;	/* frame calling for new web page */
bool js_redirects;
void gotoLocation(char *url, int delay, bool rf)
{
	if (newlocation && delay >= newloc_d) {
		nzFree(url);
		return;
	}
	nzFree(newlocation);
	newlocation = url;
	newloc_d = delay;
	newloc_r = rf;
	newloc_f = cf;
	if (!delay)
		js_redirects = true;
}				/* gotoLocation */

/* string is allocated. Quotes are removed. No other processing is done.
 * You may need to decode %xx bytes or such. */
static char *find_http_header(const char *name)
{
	char *s, *t, *u, *v;
	int namelen = strlen(name);

	if (!http_headers)
		return NULL;

	for (s = http_headers; *s; s = v) {
/* find start of next line */
		v = strchr(s, '\n');
		if (!v)
			break;
		++v;

/* name: value */
		t = strchr(s, ':');
		if (!t || t >= v)
			continue;
		u = t;
		while (u > s && isspace(u[-1]))
			--u;
		if (u - s != namelen)
			continue;
		if (!memEqualCI(s, name, namelen))
			continue;

/* This is a match */
		++t;
		while (t < v && isspace(*t))
			++t;
		u = v;
		while (u > t && isspace(u[-1]))
			--u;
/* remove quotes */
		if (u - t >= 2 && *t == u[-1] && (*t == '"' || *t == '\''))
			++t, --u;
		if (u == t)
			return NULL;
		return pullString(t, u - t);
	}

	return NULL;
}				/* find_http_header */

static void scan_http_headers(bool fromCallback)
{
	char *v;

	if (!ht_content[0] && (v = find_http_header("content-type"))) {
		strncpy(ht_content, v, sizeof(ht_content) - 1);
		caseShift(ht_content, 'l');
		nzFree(v);
		debugPrint(3, "content %s", ht_content);
		ht_charset = strchr(ht_content, ';');
		if (ht_charset)
			*ht_charset++ = 0;
/* The protocol, such as rtsp, could have already set the mime type. */
		if (!cf->mt)
			cf->mt = findMimeByContent(ht_content);
	}

	if (!ht_cdfn && (v = find_http_header("content-disposition"))) {
		char *s = strstrCI(v, "filename=");
		if (s) {
			s += 9;
			if (*s == '"') {
				char *t;
				++s;
				t = strchr(s, '"');
				if (t)
					*t = 0;
			}
			ht_cdfn = cloneString(s);
			debugPrint(3, "disposition filename %s", ht_cdfn);
		}
		nzFree(v);
	}

	if (!ht_length && (v = find_http_header("content-length"))) {
		ht_length = atoi(v);
		nzFree(v);
		if (ht_length)
			debugPrint(3, "content length %d", ht_length);
	}

	if (!ht_etag && (v = find_http_header("etag"))) {
		ht_etag = v;
		debugPrint(3, "etag %s", ht_etag);
	}

	if (ht_cacheable && (v = find_http_header("cache-control"))) {
		caseShift(v, 'l');
		if (strstr(v, "no-cache")) {
			ht_cacheable = false;
			debugPrint(3, "no cache");
		}
		nzFree(v);
	}

	if (ht_cacheable && (v = find_http_header("pragma"))) {
		caseShift(v, 'l');
		if (strstr(v, "no-cache")) {
			ht_cacheable = false;
			debugPrint(3, "no cache");
		}
		nzFree(v);
	}

	if (!ht_modtime && (v = find_http_header("last-modified"))) {
		ht_modtime = parseHeaderDate(v);
		if (ht_modtime)
			debugPrint(3, "mod date %s", v);
		nzFree(v);
	}

	if (fromCallback)
		return;

	if ((newlocation == NULL) && (v = find_http_header("location"))) {
		unpercentURL(v);
		gotoLocation(v, -1, true);
	}

	if (v = find_http_header("refresh")) {
		int delay;
		if (parseRefresh(v, &delay)) {
			unpercentURL(v);
			gotoLocation(v, delay, true);
/* string is passed to somewhere else, set v to null so it is not freed */
			v = NULL;
		}
		nzFree(v);
	}
}				/* scan_http_headers */

/* actually run the curl request, http or ftp or whatever */
static bool is_http;
static CURLcode fetch_internet(CURL * h)
{
	CURLcode curlret;
	down_h = h;
/* this should already be 0 */
	nzFree(newlocation);
	newlocation = NULL;
	nzFree(http_headers);
	http_headers = initString(&http_headers_len);
	ht_content[0] = 0;
	ht_charset = NULL;
	nzFree(ht_cdfn);
	ht_cdfn = NULL;
	nzFree(ht_etag);
	ht_etag = NULL;
	ht_length = 0;
	ht_modtime = 0;
	curlret = curl_easy_perform(h);
	if (is_http)
		scan_http_headers(false);
	return curlret;
}				/* fetch_internet */

static bool ftpConnect(const char *url, const char *user, const char *pass,
		       bool f_encoded);
static bool read_credentials(char *buffer);
static size_t curl_header_callback(char *header_line, size_t size, size_t nmemb,
				   struct eb_curl_callback_data *data);
static const char *message_for_response_code(int code);

/* Callback used by libcurl. Captures data from http, ftp, pop3, etc.
 * download states:
 * -1 user aborted the download
 * 0 standard in-memory download
 * 1 download but stop and ask user if he wants to download to disk
* 2 disk download foreground
* 3 disk download background parent
* 4 disk download background child
* 5 disk download background prefork
 * 6 mime type says this should be a stream */
size_t
eb_curl_callback(char *incoming, size_t size, size_t nitems,
		 struct eb_curl_callback_data *data)
{
	size_t num_bytes = nitems * size;
	int dots1, dots2, rc;

	if (data->down_state == 1 && is_http) {
/* don't do a download unless the code is 200. */
		curl_easy_getinfo(down_h, CURLINFO_RESPONSE_CODE, &ht_code);
		if (ht_code != 200)
			data->down_state = 0;
	}

	if (data->down_state == 1) {
		if (ht_length == 0) {
// http should always set http content length, this is just for ftp.
// And ftp downloading a file always has state = 1 on the first data block.
			double d_size = 0.0;	// download size, if we can get it
			curl_easy_getinfo(down_h,
					  CURLINFO_CONTENT_LENGTH_DOWNLOAD,
					  &d_size);
			ht_length = d_size;
			if (ht_length < 0)
				ht_length = 0;
		}

/* state 1, first data block, ask the user */
		setup_download(data);
		if (data->down_state == 0)
			goto showdots;
		if (data->down_state == -1 || data->down_state == 5)
			return -1;
	}

	if (data->down_state == 2 || data->down_state == 4) {	/* to disk */
		rc = write(data->down_fd, incoming, num_bytes);
		if (rc == num_bytes) {
			if (data->down_state == 4) {
#if 0
// Deliberately delay background download, to get several running in parallel
// for testing purposes.
				if (data->down_length == 0)
					sleep(17);
				data->down_length += rc;
#endif
				return rc;
			}
			goto showdots;
		}
		if (data->down_state == 2) {
			setError(MSG_NoWrite2, data->down_file);
			return -1;
		}
		i_printf(MSG_NoWrite2, data->down_file);
		printf(", ");
		i_puts(MSG_DownAbort);
		exit(1);
	}

showdots:
	dots1 = *(data->length) / CHUNKSIZE;
	if (data->down_state == 0)
		stringAndBytes(data->buffer, data->length, incoming, num_bytes);
	else
		*(data->length) += num_bytes;
	dots2 = *(data->length) / CHUNKSIZE;
	if (showProgress != 'q' && dots1 < dots2) {
		if (showProgress == 'd') {
			for (; dots1 < dots2; ++dots1)
				putchar('.');
			fflush(stdout);
		}
		if (showProgress == 'c' && ht_length)
			printf("%d/%d\n", dots2,
			       (ht_length + CHUNKSIZE - 1) / CHUNKSIZE);
	}
	return num_bytes;
}

/* We want to be able to abort transfers when SIGINT is received. 
 * During data transfers, libcurl ignores EINTR.  So there's no obvious way
 * to abort a transfer on SIGINT.
 * However, libcurl does call a function periodically, to indicate the
 * progress of the transfer.  If the progress function returns a non-zero
 * value, then libcurl aborts the transfer.  The nice thing about libcurl
 * is that it uses timeouts when reading and writing.  It won't block
 * forever in some system call.
 * We can be certain that libcurl will, in fact, call the progress function
 * periodically.
 * Note: libcurl doesn't start calling the progress function until after the
 * connection is made.  So it can block indefinitely during connect().
 * All of the progress arguments to the function are unused. */

static int
curl_progress(void *unused, double dl_total, double dl_now,
	      double ul_total, double ul_now)
{
	int ret = 0;
	if (intFlag) {
		intFlag = false;
		ret = 1;
	}
	return ret;
}				/* curl_progress */

uchar base64Bits(char c)
{
	if (isupperByte(c))
		return c - 'A';
	if (islowerByte(c))
		return c - ('a' - 26);
	if (isdigitByte(c))
		return c - ('0' - 52);
	if (c == '+')
		return 62;
	if (c == '/')
		return 63;
	return 64;		/* error */
}				/* base64Bits */

/*********************************************************************
Decode some data in base64.
This function operates on the data in-line.  It does not allocate a fresh
string to hold the decoded data.  Since the data will be smaller than
the base64 encoded representation, this cannot overflow.
If you need to preserve the input, copy it first.
start points to the start of the input
*end initially points to the byte just after the end of the input
Returns: GOOD_BASE64_DECODE on success, BAD_BASE64_DECODE or
EXTRA_CHARS_BASE64_DECODE on error.
When the function returns success, *end points to the end of the decoded
data.  On failure, end points to the just past the end of
what was successfully decoded.
*********************************************************************/

int base64Decode(char *start, char **end)
{
	char *b64_end = *end;
	uchar val, leftover, mod;
	bool equals;
	int ret = GOOD_BASE64_DECODE;
	char c, *q, *r;
/* Since this is a copy, and the unpacked version is always
 * smaller, just unpack it inline. */
	mod = 0;
	equals = false;
	for (q = r = start; q < b64_end; ++q) {
		c = *q;
		if (isspaceByte(c))
			continue;
		if (equals) {
			if (c == '=')
				continue;
			ret = EXTRA_CHARS_BASE64_DECODE;
			break;
		}
		if (c == '=') {
			equals = true;
			continue;
		}
		val = base64Bits(c);
		if (val & 64) {
			ret = BAD_BASE64_DECODE;
			break;
		}
		if (mod == 0) {
			leftover = val << 2;
		} else if (mod == 1) {
			*r++ = (leftover | (val >> 4));
			leftover = val << 4;
		} else if (mod == 2) {
			*r++ = (leftover | (val >> 2));
			leftover = val << 6;
		} else {
			*r++ = (leftover | val);
		}
		++mod;
		mod &= 3;
	}
	*end = r;
	return ret;
}				/* base64Decode */

static void
unpackUploadedFile(const char *post, const char *boundary,
		   char **postb, int *postb_l)
{
	static const char message64[] = "Content-Transfer-Encoding: base64";
	const int boundlen = strlen(boundary);
	const int m64len = strlen(message64);
	char *post2;
	char *b1, *b2, *b3, *b4;	/* boundary points */
	int unpack_ret;

	*postb = 0;
	*postb_l = 0;
	if (!strstr(post, message64))
		return;

	post2 = cloneString(post);
	b2 = strstr(post2, boundary);
	while (true) {
		b1 = b2 + boundlen;
		if (*b1 != '\r')
			break;
		b1 += 2;
		b1 = strstr(b1, "Content-Transfer");
		b2 = strstr(b1, boundary);
		if (memcmp(b1, message64, m64len))
			continue;
		b1 += m64len - 6;
		strcpy(b1, "8bit\r\n\r\n");
		b1 += 8;
		b1[0] = b1[1] = ' ';
		b3 = b2 - 4;

		b4 = b3;
		unpack_ret = base64Decode(b1, &b4);
		if (unpack_ret != GOOD_BASE64_DECODE)
			mail64Error(unpack_ret);
		/* Should we *really* keep going at this point? */
		strmove(b4, b3);
		b2 = b4 + 4;
	}

	b1 += strlen(b1);
	*postb = post2;
	*postb_l = b1 - post2;
}				/* unpackUploadedFile */

/* Pull a keyword: attribute out of an internet header. */
static char *extractHeaderItem(const char *head, const char *end,
			       const char *item, const char **ptr)
{
	int ilen = strlen(item);
	const char *f, *g;
	char *h = 0;
	for (f = head; f < end - ilen - 1; f++) {
		if (*f != '\n')
			continue;
		if (!memEqualCI(f + 1, item, ilen))
			continue;
		f += ilen;
		if (f[1] != ':')
			continue;
		f += 2;
		while (*f == ' ')
			++f;
		for (g = f; g < end && *g >= ' '; g++) ;
		while (g > f && g[-1] == ' ')
			--g;
		h = pullString1(f, g);
		if (ptr)
			*ptr = f;
		break;
	}
	return h;
}				/* extractHeaderItem */

/* This is a global function; it is called from cookies.c */
char *extractHeaderParam(const char *str, const char *item)
{
	int le = strlen(item), lp;
	const char *s = str;
/* ; denotes the next param */
/* Even the first param has to be preceeded by ; */
	while (s = strchr(s, ';')) {
		while (*s && (*s == ';' || (uchar) * s <= ' '))
			s++;
		if (!memEqualCI(s, item, le))
			continue;
		s += le;
		while (*s && ((uchar) * s <= ' ' || *s == '='))
			s++;
		if (!*s)
			return emptyString;
		lp = 0;
		while ((uchar) s[lp] >= ' ' && s[lp] != ';')
			lp++;
		return pullString(s, lp);
	}
	return NULL;
}				/* extractHeaderParam */

/* Date format is:    Mon, 03 Jan 2000 21:29:33 GMT|[+-]nnnn */
			/* Or perhaps:     Sun Nov  6 08:49:37 1994 */
time_t parseHeaderDate(const char *date)
{
	static const char *const months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	time_t t = 0;
	int zone = 0;
	time_t now = 0;
	time_t utcnow = 0;
	int y;			/* remember the type of format */
	struct tm *temptm = NULL;
	struct tm tm;
	memset(&tm, 0, sizeof(struct tm));
	tm.tm_isdst = -1;

	now = time(NULL);
	temptm = gmtime(&now);
	if (temptm == NULL)
		goto fail;
	utcnow = mktime(temptm);
	if (utcnow == -1 && errno)
		goto fail;

/* skip past day of the week */
	date = strchr(date, ' ');
	if (!date)
		goto fail;
	date++;

	if (isdigitByte(*date)) {	/* first format */
		y = 0;
		if (isdigitByte(date[1])) {
			tm.tm_mday = (date[0] - '0') * 10 + date[1] - '0';
			date += 2;
		} else {
			tm.tm_mday = *date - '0';
			++date;
		}
		if (*date != ' ' && *date != '-')
			goto fail;
		++date;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (memEqualCI(date, months[tm.tm_mon], 3))
				goto f1;
		goto fail;
f1:
		date += 3;
		if (*date == ' ') {
			date++;
			if (!isdigitByte(date[0]))
				goto fail;
			if (!isdigitByte(date[1]))
				goto fail;
			if (!isdigitByte(date[2]))
				goto fail;
			if (!isdigitByte(date[3]))
				goto fail;
			tm.tm_year =
			    (date[0] - '0') * 1000 + (date[1] - '0') * 100 +
			    (date[2] - '0') * 10 + date[3] - '0' - 1900;
			date += 4;
		} else if (*date == '-') {
			/* Sunday, 06-Nov-94 08:49:37 GMT */
			date++;
			if (!isdigitByte(date[0]))
				goto fail;
			if (!isdigitByte(date[1]))
				goto fail;
			if (!isdigitByte(date[2])) {
				tm.tm_year =
				    (date[0] >=
				     '7' ? 1900 : 2000) + (date[0] - '0') * 10 +
				    date[1] - '0' - 1900;
				date += 2;
			} else {
				tm.tm_year = atoi(date) - 1900;
				date += 4;
			}
		} else
			goto fail;
		if (*date != ' ')
			goto fail;
		date++;
	} else {
/* second format */
		y = 1;
		for (tm.tm_mon = 0; tm.tm_mon < 12; tm.tm_mon++)
			if (memEqualCI(date, months[tm.tm_mon], 3))
				goto f2;
		goto fail;
f2:
		date += 3;
		while (*date == ' ')
			date++;
		if (!isdigitByte(date[0]))
			goto fail;
		tm.tm_mday = date[0] - '0';
		date++;
		if (*date != ' ') {
			if (!isdigitByte(date[0]))
				goto fail;
			tm.tm_mday = tm.tm_mday * 10 + date[0] - '0';
			date++;
		}
		if (*date != ' ')
			goto fail;
		date++;
	}

/* ready to crack time */
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_hour = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_min = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;
	if (*date != ':')
		goto fail;
	date++;
	if (!isdigitByte(date[0]))
		goto fail;
	if (!isdigitByte(date[1]))
		goto fail;
	tm.tm_sec = (date[0] - '0') * 10 + date[1] - '0';
	date += 2;

	if (y) {
/* year is at the end */
		if (*date != ' ')
			goto fail;
		date++;
		if (!isdigitByte(date[0]))
			goto fail;
		if (!isdigitByte(date[1]))
			goto fail;
		if (!isdigitByte(date[2]))
			goto fail;
		if (!isdigitByte(date[3]))
			goto fail;
		tm.tm_year =
		    (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] -
								      '0') *
		    10 + date[3] - '0' - 1900;
		date += 4;
	}

	if (*date != ' ' && *date)
		goto fail;

	while (*date == ' ')
		++date;
	if ((*date == '+' || *date == '-') &&
	    isdigit(date[1]) && isdigit(date[2]) &&
	    isdigit(date[3]) && isdigit(date[4])) {
		zone = 10 * (date[1] - '0') + date[2] - '0';
		zone *= 60;
		zone += 10 * (date[3] - '0') + date[4] - '0';
		zone *= 60;
/* adjust to gmt */
		if (*date == '+')
			zone = -zone;
	}

	t = mktime(&tm);
	if (t != (time_t) - 1)
		return t + zone + (now - utcnow);

fail:
	return 0;
}				/* parseHeaderDate */

bool parseRefresh(char *ref, int *delay_p)
{
	int delay = 0;
	char *u = ref;
	if (isdigitByte(*u))
		delay = atoi(u);
	while (isdigitByte(*u) || *u == '.')
		++u;
	if (*u == ';')
		++u;
	while (*u == ' ')
		++u;
	if (memEqualCI(u, "url=", 4)) {
		char qc;
		u += 4;
		qc = *u;
		if (qc == '"' || qc == '\'')
			++u;
		else
			qc = 0;
		strmove(ref, u);
		u = ref + strlen(ref);
		if (u > ref && u[-1] == qc)
			u[-1] = 0;
		debugPrint(3, "delay %d %s", delay, ref);
/* avoid the obvious infinite loop */
		if (sameURL(ref, cf->fileName)) {
			*delay_p = 0;
			return false;
		}
		*delay_p = delay;
		return true;
	}
	i_printf(MSG_GarbledRefresh, ref);
	*delay_p = 0;
	return false;
}				/* parseRefresh */

/* Return true if we waited for the duration, false if interrupted.
 * I don't know how to do this in Windows. */
bool shortRefreshDelay(void)
{
/* the value 10 seconds is somewhat arbitrary */
	if (newloc_d < 10)
		return true;
	i_printf(MSG_RedirectDelayed, newlocation, newloc_d);
	return false;
}				/* shortRefreshDelay */

static char *urlcopy;
static int urlcopy_l;

// encode the url, if it was supplied by the user.
// Otherwise just make a copy.
// Either way there is room for one more char at the end.
static void urlSanitize(const char *url, const char *post, bool f_encoded)
{
	const char *portloc;

	if (f_encoded && !looksPercented(url, post)) {
		debugPrint(2, "Warning, url %s doesn't look encoded", url);
		f_encoded = false;
	}

	if (!f_encoded) {
		urlcopy = percentURL(url, post);
		urlcopy_l = strlen(urlcopy);
	} else {
		if (post)
			urlcopy_l = post - url;
		else
			urlcopy_l = strlen(url);
		urlcopy = allocMem(urlcopy_l + 2);
		strncpy(urlcopy, url, urlcopy_l);
		urlcopy[urlcopy_l] = 0;
	}

// get rid of : in http://this.that.com:/path, curl can't handle it.
	getPortLocURL(urlcopy, &portloc, 0);
	if (portloc && !isdigit(portloc[1])) {
		const char *s = portloc + strcspn(portloc, "/?#\1");
		strmove((char *)portloc, s);
		urlcopy_l = strlen(urlcopy);
	}
}				/* urlSanitize */

// Last three are result parameters, for http headers and body strings.
// Set to 0 if you don't want these passed back in this way.
bool httpConnect(const char *url, bool down_ok, bool webpage,
		 bool f_encoded, char **headers_p, char **body_p, int *bodlen_p)
{
	char *cacheData = NULL;
	int cacheDataLen = 0;
	CURL *h;		// the curl http handle
	struct eb_curl_callback_data cbd;
	char *referrer = NULL;
	CURLcode curlret = CURLE_OK;
	struct curl_slist *custom_headers = NULL;
	struct curl_slist *tmp_headers = NULL;
	const struct MIMETYPE *mt;
	char user[MAXUSERPASS], pass[MAXUSERPASS];
	char creds_buf[MAXUSERPASS * 2 + 1];	/* creds abr. for credentials */
	int creds_len = 0;
	bool still_fetching = true;
	const char *host;
	const char *prot;
	char *cmd;
	const char *post, *s;
	char *postb = NULL;
	int postb_l = 0;
	bool transfer_status = false;
	bool proceed_unauthenticated = false;
	int redirect_count = 0;
	bool name_changed = false;
	bool post_request = false;
	bool head_request = false;

	if (headers_p)
		*headers_p = 0;
	if (body_p)
		*body_p = 0;
	if (bodlen_p)
		*bodlen_p = 0;

	urlcopy = NULL;
	serverData = NULL;
	serverDataLen = 0;
	strcpy(creds_buf, ":");	/* Flush stale username and password. */
	cf->mt = NULL;		/* should already be null */

	host = getHostURL(url);
	if (!host) {
		setError(MSG_DomainEmpty);
		return false;
	}

/* Pull user password out of the url */
	user[0] = pass[0] = 0;
	s = getUserURL(url);
	if (s) {
		if (strlen(s) >= sizeof(user) - 2) {
			setError(MSG_UserNameLong, sizeof(user));
			return false;
		}
		strcpy(user, s);
	}
	s = getPassURL(url);
	if (s) {
		if (strlen(s) >= sizeof(pass) - 2) {
			setError(MSG_PasswordLong, sizeof(pass));
			return false;
		}
		strcpy(pass, s);
	}

	prot = getProtURL(url);
	if (!prot) {
		setError(MSG_WebProtBad, "(?)");
		return false;
	}

	if (stringEqualCI(prot, "http") || stringEqualCI(prot, "https")) {
		;		/* ok for now */
	} else if (stringEqualCI(prot, "ftp") ||
		   stringEqualCI(prot, "ftps") ||
		   stringEqualCI(prot, "scp") ||
		   stringEqualCI(prot, "tftp") || stringEqualCI(prot, "sftp")) {
		return ftpConnect(url, user, pass, f_encoded);
	} else if ((cf->mt = findMimeByProtocol(prot)) && pluginsOn
		   && cf->mt->stream) {
mimestream:
/* set this to null so we don't push a new buffer */
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
/* could jump back here after a redirect, so use urlcopy if it is there */
		cmd = pluginCommand(cf->mt, (urlcopy ? urlcopy : url),
				    NULL, NULL);
		nzFree(urlcopy);
		urlcopy = NULL;
		if (!cmd)
			return false;
		eb_system(cmd, true);
		nzFree(cmd);
		return true;
	} else {
		setError(MSG_WebProtBad, prot);
		return false;
	}

/* Ok, it's http, but the suffix could force a plugin */
	if ((cf->mt = findMimeByURL(url)) && pluginsOn && cf->mt->stream)
		goto mimestream;

/* if invoked from a playlist */
	if (currentReferrer && pluginsOn
	    && (mt = findMimeByURL(currentReferrer)) && mt->stream) {
		cf->mt = mt;
		goto mimestream;
	}

	cbd.buffer = &serverData;
	cbd.length = &serverDataLen;
	cbd.down_state = 0;
	cbd.down_file = cbd.down_file2 = NULL;
	cbd.down_length = 0;
	down_permitted = down_ok;
	h = http_curl_init(&cbd);
	if (!h)
		return false;

/* "Expect:" header causes some servers to lose.  Disable it. */
	tmp_headers = curl_slist_append(custom_headers, "Expect:");
	if (tmp_headers == NULL)
		i_printfExit(MSG_NoMem);
	custom_headers = tmp_headers;
	if (httpLanguage) {
		custom_headers =
		    curl_slist_append(custom_headers, httpLanguage);
		if (custom_headers == NULL)
			i_printfExit(MSG_NoMem);
	}

	post = strchr(url, '\1');
	postb = 0;
	urlSanitize(url, post, f_encoded);

	if (post) {
		post_request = true;
		post++;

		if (strncmp(post, "`mfd~", 5)) ;	/* No need to do anything, not multipart. */

		else {
			int multipart_header_len = 0;
			char *multipart_header =
			    initString(&multipart_header_len);
			char thisbound[24];
			post += 5;

			stringAndString(&multipart_header,
					&multipart_header_len,
					"Content-Type: multipart/form-data; boundary=");
			s = strchr(post, '\r');
			stringAndBytes(&multipart_header, &multipart_header_len,
				       post, s - post);
			tmp_headers =
			    curl_slist_append(custom_headers, multipart_header);
			if (tmp_headers == NULL)
				i_printfExit(MSG_NoMem);
			custom_headers = tmp_headers;
			/* curl_slist_append made a copy of multipart_header. */
			nzFree(multipart_header);

			memcpy(thisbound, post, s - post);
			thisbound[s - post] = 0;
			post = s + 2;
			unpackUploadedFile(post, thisbound, &postb, &postb_l);
		}
		curlret = curl_easy_setopt(h, CURLOPT_POSTFIELDS,
					   (postb_l ? postb : post));
		if (curlret != CURLE_OK)
			goto curl_fail;
		curlret =
		    curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE,
				     postb_l ? postb_l : strlen(post));
		if (curlret != CURLE_OK)
			goto curl_fail;
	} else {
		curlret = curl_easy_setopt(h, CURLOPT_HTTPGET, 1);
		if (curlret != CURLE_OK)
			goto curl_fail;
	}

	if (sendReferrer && currentReferrer) {
		const char *post2 = strchr(currentReferrer, '\1');
		if (!post2)
			post2 = currentReferrer + strlen(currentReferrer);
		if (post2 - currentReferrer >= 7
		    && !memcmp(post2 - 7, ".browse", 7))
			post2 -= 7;
		nzFree(cw->referrer);
		cw->referrer = cloneString(currentReferrer);
		cw->referrer[post2 - currentReferrer] = 0;
		referrer = cw->referrer;
	}

	curlret = curl_easy_setopt(h, CURLOPT_REFERER, referrer);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = curl_easy_setopt(h, CURLOPT_HTTPHEADER, custom_headers);
	if (curlret != CURLE_OK)
		goto curl_fail;
	curlret = setCurlURL(h, urlcopy);
	if (curlret != CURLE_OK)
		goto curl_fail;

	/* If we have a username and password, then tell libcurl about it.
	 * libcurl won't send it to the server unless server gave a 401 response.
	 * Libcurl selects the most secure form of auth provided by server. */

	if (user[0] && pass[0]) {
		strcpy(creds_buf, user);
		creds_len = strlen(creds_buf);
		creds_buf[creds_len] = ':';
		strcpy(creds_buf + creds_len + 1, pass);
	} else
		getUserPass(urlcopy, creds_buf, false);

/*
 * If the URL didn't have user and password, and getUserPass failed,
 * then creds_buf == "".
 */
	curlret = curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto curl_fail;

/* We are ready to make a transfer.  Here is where it gets complicated.
 * At the top of the loop, we perform the HTTP request.  It may fail entirely
 * (I.E., libcurl returns an indicator other than CURLE_OK).
 * We may be redirected.  Edbrowse needs finer control over the redirection
 * process than libcurl gives us.
 * Decide whether to accept the redirection, using the following criteria.
 * Does user permit redirects?  Will we exceed maximum allowable redirects?
 * Is the destination in the fetch history?
 * We may be asked for authentication.  In that case, grab username and
 * password from the user.  If the server accepts the username and password,
 * then add it to the list of authentication records.  */

	still_fetching = true;
	serverData = initString(&serverDataLen);

	if (!post_request && presentInCache(urlcopy)) {
		head_request = true;
		curl_easy_setopt(h, CURLOPT_NOBODY, 1l);
	}

	while (still_fetching == true) {
		char *redir = NULL;
		cbd.length = &serverDataLen;
// An earlier 302 redirection could set content type = application/binary,
// which in turn sets state = 1, which is ignored since 302 takes precedence.
// So state might still be 1, set it back to 0.
		cbd.down_state = 0;

// recheck suffix after a redirect
		if (redirect_count &&
		    (cf->mt = findMimeByURL(urlcopy)) && pluginsOn
		    && cf->mt->stream) {
			curl_easy_cleanup(h);
			goto mimestream;
		}

perform:
		is_http = true;
		ht_cacheable = true;
		curlret = fetch_internet(h);

		if (cbd.down_state == 6) {
			curl_easy_cleanup(h);
			goto mimestream;
		}

/*********************************************************************
If the desired file is in cache for some reason, and we issued the head request,
and it is application, or some such that triggers a download, then state = 1,
but no data is forthcoming, and the user was never asked if he wants
to download, so state is still 1.
So ask, and then look at state.
If state is nonzero, sorry, I'm not taking the file from cache,
not yet, just because it's a whole bunch of new code.
We don't generally store our downloaded files in cache anyways,
they go where they go, so this doesn't come up very often.
*********************************************************************/

		if (head_request) {
			if (cbd.down_state == 1) {
				setup_download(&cbd);
/* now we have our answer */
			}

			if (cbd.down_state != 0) {
				curl_easy_setopt(h, CURLOPT_NOBODY, 0l);
				head_request = false;
				debugPrint(3, "switch to get for download %d",
					   cbd.down_state);
			}

			if (cbd.down_state == 2) {
				curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE,
						  &ht_code);
				if (ht_code == 200)
					goto perform;
			}
		}

		if (cbd.down_state == 5) {
/* user has directed a download of this file in the background. */
			background_download(&cbd);
			if (cbd.down_state == 4)
				goto perform;
		}

		if (cbd.down_state == 3 || cbd.down_state == -1) {
/* set this to null so we don't push a new buffer */
			serverData = NULL;
			cnzFree(cbd.down_file);
			curl_easy_cleanup(h);
			return false;
		}

		if (cbd.down_state == 4) {
			if (curlret != CURLE_OK) {
				ebcurl_setError(curlret, urlcopy);
				showError();
				exit(2);
			}
			i_printf(MSG_DownSuccess);
			printf(": %s\n", cbd.down_file2);
			exit(0);
		}

		if (*(cbd.length) >= CHUNKSIZE && showProgress == 'd')
			nl();	/* We printed dots, so terminate them with newline */

		if (cbd.down_state == 2) {
			close(cbd.down_fd);
			cnzFree(cbd.down_file);
			setError(MSG_DownSuccess);
			serverData = NULL;
			curl_easy_cleanup(h);
			return false;
		}

		if (curlret != CURLE_OK)
			goto curl_fail;
		curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &ht_code);
		if (curlret != CURLE_OK)
			goto curl_fail;

		debugPrint(3, "http code %ld", ht_code);

/* refresh header is an alternate form of redirection */
		if (newlocation && newloc_d >= 0) {
			if (shortRefreshDelay()) {
				ht_code = 302;
			} else {
				nzFree(newlocation);
				newlocation = 0;
			}
		}

		if (allowRedirection &&
		    (ht_code >= 301 && ht_code <= 303 ||
		     ht_code >= 307 && ht_code <= 308)) {
			redir = newlocation;
			if (redir)
				redir = resolveURL(urlcopy, redir);
			still_fetching = false;
			if (redir == NULL) {
				/* Redirected, but we don't know where to go. */
				i_printf(MSG_RedirectNoURL, ht_code);
				transfer_status = true;
			} else if (redirect_count >= 10) {
				i_puts(MSG_RedirectMany);
				transfer_status = true;
				nzFree(redir);
			} else {	/* redirection looks good. */
				strcpy(creds_buf, ":");	/* Flush stale data. */
				nzFree(urlcopy);
				urlcopy = redir;
				unpercentURL(urlcopy);

/* Convert POST request to GET request after redirection. */
/* This should only be done for 301 through 303 */
				if (ht_code < 307) {
					curl_easy_setopt(h, CURLOPT_HTTPGET, 1);
					post_request = false;
				}
/* I think there is more work to do for 307 308,
 * pasting the prior post string onto the new URL. Not sure about this. */

				getUserPass(urlcopy, creds_buf, false);

				curlret =
				    curl_easy_setopt(h, CURLOPT_USERPWD,
						     creds_buf);
				if (curlret != CURLE_OK)
					goto curl_fail;

				curlret = setCurlURL(h, urlcopy);
				if (curlret != CURLE_OK)
					goto curl_fail;

				if (!post_request && presentInCache(urlcopy)) {
					head_request = true;
					curl_easy_setopt(h, CURLOPT_NOBODY, 1l);
				}

				nzFree(serverData);
				serverData = emptyString;
				serverDataLen = 0;
				redirect_count += 1;
				still_fetching = true;
				name_changed = true;
				debugPrint(2, "redirect %s", urlcopy);
			}
		}

		else if (ht_code == 401 && !proceed_unauthenticated) {
			bool got_creds;
			i_printf(MSG_AuthRequired, urlcopy);
			nl();
			got_creds = read_credentials(creds_buf);
			if (got_creds) {
				addWebAuthorization(urlcopy, creds_buf, false);
				curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
				nzFree(serverData);
				serverData = emptyString;
				serverDataLen = 0;
			} else {
/* User aborted the login process, try and at least get something. */
				proceed_unauthenticated = true;
			}
		} else {	/* not redirect, not 401 */
			if (head_request) {
				if (fetchCache
				    (urlcopy, ht_etag, ht_modtime, &cacheData,
				     &cacheDataLen)) {
					nzFree(serverData);
					serverData = cacheData;
					serverDataLen = cacheDataLen;
					still_fetching = false;
					transfer_status = true;
				} else {
/* Back through the loop,
 * now doing GET rather than HEAD. */
					curl_easy_setopt(h, CURLOPT_NOBODY, 0l);
					head_request = false;
					--redirect_count;
				}
			} else {
				if (ht_code == 200 && ht_cacheable &&
				    (ht_modtime || ht_etag) &&
				    cbd.down_state == 0)
					storeCache(urlcopy, ht_etag, ht_modtime,
						   serverData, serverDataLen);
				still_fetching = false;
				transfer_status = true;
			}
		}
	}

curl_fail:
	if (custom_headers)
		curl_slist_free_all(custom_headers);
// Don't need the handle any more.
	curl_easy_cleanup(h);

	if (curlret != CURLE_OK)
		ebcurl_setError(curlret, urlcopy);

	nzFree(newlocation);
	newlocation = 0;

	if (transfer_status == false) {
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
		nzFree(urlcopy);	/* Free it on transfer failure. */
	} else {
		if (ht_code != 200 && ht_code != 201 &&
		    (webpage || debugLevel >= 2) ||
		    ht_code == 201 && debugLevel >= 3)
			i_printf(MSG_HTTPError,
				 ht_code, message_for_response_code(ht_code));
		if (name_changed)
			changeFileName = urlcopy;
		else
			nzFree(urlcopy);	/* Don't need it anymore. */
	}

	nzFree(postb);

/* see if http header has set the filename */
	if (ht_cdfn) {
		nzFree(changeFileName);
		changeFileName = ht_cdfn;
		ht_cdfn = NULL;
	}

/* Check for plugin to run here */
	if (transfer_status && ht_code == 200 && cf->mt && pluginsOn &&
	    !cf->mt->stream && !cf->mt->outtype && cf->mt->program) {
		bool rc = playServerData();
		nzFree(serverData);
		serverData = NULL;
		serverDataLen = 0;
		return rc;
	}

	if (transfer_status) {
		if (headers_p) {
			*headers_p = http_headers;
// The string is your responsibility now.
			http_headers = 0;
		}
		if (body_p) {
			*body_p = serverData;
			*bodlen_p = serverDataLen;
// The string is your responsibility now.
			serverData = 0;
			serverDataLen = 0;
		}
	}

	return transfer_status;
}				/* httpConnect */

/* Format a line from an ftp ls. */
static void ftpls(char *line)
{
	int l = strlen(line);
	int j;
	if (l && line[l - 1] == '\r')
		line[--l] = 0;

/* blank line becomes paragraph break */
	if (!l || memEqualCI(line, "total ", 6) && stringIsNum(line + 6)) {
		stringAndString(&serverData, &serverDataLen, "<P>\n");
		return;
	}
	stringAndString(&serverData, &serverDataLen, "<br>");

	for (j = 0; line[j]; ++j)
		if (!strchr("-rwxdls", line[j]))
			break;

	if (j == 10 && line[j] == ' ') {	/* long list */
		int fsize, nlinks;
		char user[42], group[42];
		char month[8];
		int day;
		char *q, *t;
		sscanf(line + j, " %d %40s %40s %d %3s %d",
		       &nlinks, user, group, &fsize, month + 1, &day);
		q = strchr(line, ':');
		if (q) {
			for (++q; isdigitByte(*q) || *q == ':'; ++q) ;
			while (*q == ' ')
				++q;
		} else {
/* old files won't have the time, but instead, they have the year. */
/* bad news for us; no good/easy way to glom onto this one. */
			month[0] = month[4] = ' ';
			month[5] = 0;
			q = strstr(line, month);
			if (q) {
				q += 8;
				while (*q == ' ')
					++q;
				while (isdigitByte(*q))
					++q;
				while (*q == ' ')
					++q;
			}
		}

		if (q && *q) {
			char qc = '"';
			if (strchr(q, qc))
				qc = '\'';
			stringAndString(&serverData, &serverDataLen,
					"<A HREF=x");
			serverData[serverDataLen - 1] = qc;
			t = strstr(q, " -> ");
			if (t)
				stringAndBytes(&serverData, &serverDataLen, q,
					       t - q);
			else
				stringAndString(&serverData, &serverDataLen, q);
			stringAndChar(&serverData, &serverDataLen, qc);
			stringAndChar(&serverData, &serverDataLen, '>');
			stringAndString(&serverData, &serverDataLen, q);
			stringAndString(&serverData, &serverDataLen, "</A>");
			if (line[0] == 'd')
				stringAndChar(&serverData, &serverDataLen, '/');
			stringAndString(&serverData, &serverDataLen, ": ");
			stringAndNum(&serverData, &serverDataLen, fsize);
			stringAndChar(&serverData, &serverDataLen, '\n');
			return;
		}
	}

	if (!strpbrk(line, "<>&")) {
		stringAndString(&serverData, &serverDataLen, line);
	} else {
		char c, *q;
		for (q = line; c = *q; ++q) {
			char *meta = 0;
			if (c == '<')
				meta = "&lt;";
			if (c == '>')
				meta = "&gt;";
			if (c == '&')
				meta = "&amp;";
			if (meta)
				stringAndString(&serverData, &serverDataLen,
						meta);
			else
				stringAndChar(&serverData, &serverDataLen, c);
		}
	}

	stringAndChar(&serverData, &serverDataLen, '\n');
}				/* ftpls */

/* parse_directory_listing: convert an FTP-style listing to html. */
/* Repeatedly calls ftpls to parse each line of the data. */
static void parse_directory_listing(void)
{
	char *s, *t;
	char *incomingData = serverData;
	int incomingLen = serverDataLen;
	serverData = initString(&serverDataLen);
	stringAndString(&serverData, &serverDataLen, "<html>\n<body>\n");

	if (!incomingLen) {
		i_stringAndMessage(&serverData, &serverDataLen,
				   MSG_FTPEmptyDir);
	} else {

		s = incomingData;
		while (s < incomingData + incomingLen) {
			t = strchr(s, '\n');
			if (!t || t >= incomingData + incomingLen)
				break;	/* should never happen */
			*t = 0;
			ftpls(s);
			s = t + 1;
		}
	}

	stringAndString(&serverData, &serverDataLen, "</body></html>\n");
	nzFree(incomingData);
}				/* parse_directory_listing */

void ebcurl_setError(CURLcode curlret, const char *url)
{
	const char *host = NULL, *protocol = NULL;
	protocol = getProtURL(url);
	host = getHostURL(url);

/* this should never happen */
	if (!host)
		host = emptyString;

	switch (curlret) {

	case CURLE_UNSUPPORTED_PROTOCOL:
		setError(MSG_WebProtBad, protocol);
		break;
	case CURLE_URL_MALFORMAT:
		setError(MSG_BadURL, url);
		break;
	case CURLE_COULDNT_RESOLVE_HOST:
		setError(MSG_IdentifyHost, host);
		break;
	case CURLE_REMOTE_ACCESS_DENIED:
		setError(MSG_RemoteAccessDenied);
		break;
	case CURLE_TOO_MANY_REDIRECTS:
		setError(MSG_RedirectMany);
		break;

	case CURLE_OPERATION_TIMEDOUT:
		setError(MSG_Timeout);
		break;
	case CURLE_PEER_FAILED_VERIFICATION:
	case CURLE_SSL_CACERT:
		setError(MSG_NoCertify, host);
		break;

	case CURLE_GOT_NOTHING:
	case CURLE_RECV_ERROR:
		setError(MSG_WebRead);
		break;
	case CURLE_SEND_ERROR:
		setError(MSG_CurlSendData);
		break;
	case CURLE_COULDNT_CONNECT:
		setError(MSG_WebConnect, host);
		break;
	case CURLE_FTP_CANT_GET_HOST:
		setError(MSG_FTPConnect);
		break;

	case CURLE_ABORTED_BY_CALLBACK:
		setError(MSG_Interrupted);
		break;
/* These all look like session initiation failures. */
	case CURLE_FTP_WEIRD_SERVER_REPLY:
	case CURLE_FTP_WEIRD_PASS_REPLY:
	case CURLE_FTP_WEIRD_PASV_REPLY:
	case CURLE_FTP_WEIRD_227_FORMAT:
	case CURLE_FTP_COULDNT_SET_ASCII:
	case CURLE_FTP_COULDNT_SET_BINARY:
	case CURLE_FTP_PORT_FAILED:
		setError(MSG_FTPSession);
		break;

	case CURLE_FTP_USER_PASSWORD_INCORRECT:
		setError(MSG_LogPass);
		break;

	case CURLE_FTP_COULDNT_RETR_FILE:
		setError(MSG_FTPTransfer);
		break;

	case CURLE_SSL_CONNECT_ERROR:
		setError(MSG_SSLConnectError, ht_error);
		break;

	case CURLE_LOGIN_DENIED:
		setError(MSG_LogPass);
		break;

	default:
		setError(MSG_CurlCatchAll, curl_easy_strerror(curlret));
		break;
	}
}				/* ebcurl_setError */

/* Like httpConnect, but for ftp */
static bool ftpConnect(const char *url, const char *user, const char *pass,
		       bool f_encoded)
{
	CURL *h;		// the curl handle for ftp
	struct eb_curl_callback_data cbd;
	int protLength;		/* length of "ftp://" */
	bool transfer_success = false;
	bool has_slash, is_scp;
	CURLcode curlret = CURLE_OK;
	char creds_buf[MAXUSERPASS * 2 + 1];
	size_t creds_len = 0;

	is_http = false;
	protLength = strchr(url, ':') - url + 3;
/* scp is somewhat unique among the protocols handled here */
	is_scp = memEqualCI(url, "scp", 3);

	if (user[0] && pass[0]) {
		strcpy(creds_buf, user);
		creds_len = strlen(creds_buf);
		creds_buf[creds_len] = ':';
		strcpy(creds_buf + creds_len + 1, pass);
	} else if (memEqualCI(url, "ftp", 3)) {
		strcpy(creds_buf, "anonymous:ftp@example.com");
	}

	cbd.buffer = &serverData;
	cbd.length = &serverDataLen;
	cbd.down_state = 0;
	cbd.down_file = cbd.down_file2 = NULL;
	cbd.down_length = 0;
	h = http_curl_init(&cbd);
	if (!h)
		goto ftp_transfer_fail;
	curlret = curl_easy_setopt(h, CURLOPT_USERPWD, creds_buf);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	serverData = initString(&serverDataLen);
	urlSanitize(url, 0, f_encoded);

/* libcurl appends an implicit slash to URLs like "ftp://foo.com".
* Be explicit, so that edbrowse knows that we have a directory. */

	if (!strchr(urlcopy + protLength, '/'))
		strcpy(urlcopy + urlcopy_l, "/");

	curlret = setCurlURL(h, urlcopy);
	if (curlret != CURLE_OK)
		goto ftp_transfer_fail;

	has_slash = urlcopy[urlcopy_l] == '/';
/* don't download a directory listing, we want to see that */
/* Fetching a directory will fail in the special case of scp. */
	cbd.down_state = (has_slash ? 0 : 1);
	down_msg = MSG_FTPDownload;
	if (is_scp)
		down_msg = MSG_SCPDownload;
	cbd.length = &serverDataLen;

perform:
	curlret = fetch_internet(h);

	if (cbd.down_state == 5) {
/* user has directed a download of this file in the background. */
		background_download(&cbd);
		if (cbd.down_state == 4)
			goto perform;
	}

	if (cbd.down_state == 3 || cbd.down_state == -1) {
/* set this to null so we don't push a new buffer */
		serverData = NULL;
		cnzFree(cbd.down_file);
		curl_easy_cleanup(h);
		return false;
	}

	if (cbd.down_state == 4) {
		if (curlret != CURLE_OK) {
			ebcurl_setError(curlret, urlcopy);
			showError();
			exit(2);
		}
		i_printf(MSG_DownSuccess);
		printf(": %s\n", cbd.down_file2);
		exit(0);
	}

	if (*(cbd.length) >= CHUNKSIZE && showProgress == 'd')
		nl();		/* We printed dots, so terminate them with newline */

	if (cbd.down_state == 2) {
		close(cbd.down_fd);
		setError(MSG_DownSuccess);
		serverData = NULL;
		cnzFree(cbd.down_file);
		curl_easy_cleanup(h);
		return false;
	}

/* Should we run this code on any error condition? */
/* The SSH error pops up under sftp. */
	if (curlret == CURLE_FTP_COULDNT_RETR_FILE ||
	    curlret == CURLE_REMOTE_FILE_NOT_FOUND || curlret == CURLE_SSH) {
		if (has_slash | is_scp)
			transfer_success = false;
		else {		/* try appending a slash. */
			strcpy(urlcopy + urlcopy_l, "/");
			cbd.down_state = 0;
			cnzFree(cbd.down_file);
			cbd.down_file = 0;
			curlret = setCurlURL(h, urlcopy);
			if (curlret != CURLE_OK)
				goto ftp_transfer_fail;

			curlret = fetch_internet(h);
			if (curlret != CURLE_OK)
				transfer_success = false;
			else {
				parse_directory_listing();
				transfer_success = true;
			}
		}
	} else if (curlret == CURLE_OK) {
		if (has_slash == true)
			parse_directory_listing();
		transfer_success = true;
	} else
		transfer_success = false;

ftp_transfer_fail:
// Don't need the handle any more
	if (h)
		curl_easy_cleanup(h);
	if (transfer_success == false) {
		if (curlret != CURLE_OK)
			ebcurl_setError(curlret, urlcopy);
		nzFree(serverData);
		serverData = 0;
		serverDataLen = 0;
	}
	if (transfer_success == true && !stringEqual(url, urlcopy))
		changeFileName = urlcopy;
	else
		nzFree(urlcopy);

	return transfer_success;
}				/* ftpConnect */

/* If the user has asked for locale-specific responses, then build an
 * appropriate Accept-Language: header. */
void setHTTPLanguage(const char *lang)
{
	int httpLanguage_l;

	nzFree(httpLanguage);
	httpLanguage = NULL;
	if (!lang)
		return;

	httpLanguage = initString(&httpLanguage_l);
	stringAndString(&httpLanguage, &httpLanguage_l, "Accept-Language: ");
	stringAndString(&httpLanguage, &httpLanguage_l, lang);
}				/* setHTTPLanguage */

/* Set the FD_CLOEXEC flag on a socket newly-created by libcurl.
 * Let's not leak libcurl's sockets to child processes created by the
 * ! (escape-to-shell) command.
 * This is a callback.  It returns 0 on success, 1 on failure, per the
 * libcurl docs.
 */
static int
my_curl_safeSocket(void *clientp, curl_socket_t socketfd, curlsocktype purpose)
{
#ifdef _MSC_VER
	return 0;
#else // !_MSC_VER for success = fcntl(socketfd, F_SETFD, FD_CLOEXEC);
	int success = fcntl(socketfd, F_SETFD, FD_CLOEXEC);
	if (success == -1)
		success = 1;
	else
		success = 0;
	return success;
#endif // _MSC_VER y/n
}

static CURL *http_curl_init(struct eb_curl_callback_data *cbd)
{
	CURLcode curl_init_status = CURLE_OK;
	CURL *h = curl_easy_init();
	if (h == NULL)
		goto libcurl_init_fail;
	curl_init_status =
	    curl_easy_setopt(h, CURLOPT_SHARE, global_share_handle);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
/* Lots of these setopt calls shouldn't fail.  They just diddle a struct. */
	curl_easy_setopt(h, CURLOPT_SOCKOPTFUNCTION, my_curl_safeSocket);
	curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, eb_curl_callback);
	curl_easy_setopt(h, CURLOPT_WRITEDATA, cbd);
	curl_easy_setopt(h, CURLOPT_HEADERFUNCTION, curl_header_callback);
	curl_easy_setopt(h, CURLOPT_HEADERDATA, cbd);
	if (debugLevel >= 4)
		curl_easy_setopt(h, CURLOPT_VERBOSE, 1);
	curl_easy_setopt(h, CURLOPT_DEBUGFUNCTION, ebcurl_debug_handler);
	curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0);
	curl_easy_setopt(h, CURLOPT_PROGRESSFUNCTION, curl_progress);
	curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, webTimeout);
	curl_easy_setopt(h, CURLOPT_USERAGENT, currentAgent);
	curl_easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_DEFAULT);
	curl_easy_setopt(h, CURLOPT_USERAGENT, currentAgent);
/* We're doing this manually for now.
	curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, allowRedirection);
*/
	curl_easy_setopt(h, CURLOPT_AUTOREFERER, sendReferrer);
	if (ftpActive)
		curl_easy_setopt(h, CURLOPT_FTPPORT, "-");
	else
		curl_easy_setopt(h, CURLOPT_FTPPORT, NULL);
/* See "man curl_easy_setopt.3" for info on CURLOPT_FTPPORT.  Supplying
* "-" makes libcurl select the best IP address for active ftp. */

/*
* tell libcurl to pick the strongest method from basic, digest and ntlm authentication
* don't use any auth method as it will prefer Negotiate to NTLM,
* and it looks like in most cases microsoft IIS says it supports both and libcurl
* doesn't fall back to NTLM when it discovers that Negotiate isn't set up on a system
*/
	curl_easy_setopt(h, CURLOPT_HTTPAUTH,
			 CURLAUTH_BASIC | CURLAUTH_DIGEST | CURLAUTH_NTLM);

/* The next few setopt calls could allocate or perform file I/O. */
	ht_error[0] = '\0';
	curl_init_status = curl_easy_setopt(h, CURLOPT_ERRORBUFFER, ht_error);
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	curl_init_status = curl_easy_setopt(h, CURLOPT_ENCODING, "");
	if (curl_init_status != CURLE_OK)
		goto libcurl_init_fail;
	return h;

libcurl_init_fail:
	i_printf(MSG_LibcurlNoInit);
	if (h)
		curl_easy_cleanup(h);
	return 0;
}				/* http_curl_init */

/*
 * There's no easy way to get at the server's response message from libcurl.
 * So here are some tables and a function for translating response codes to
 * messages.
*/

static const char *response_codes_1xx[] = {
	"Continue",
	"Switching Protocols"
};

static const char *response_codes_2xx[] = {
	"OK",
	"Created" "Accepted",
	"Non-Authoritative Information",
	"No Content",
	"Reset Content",
	"Partial Content"
};

static const char *response_codes_3xx[] = {
	"Multiple Choices",
	"Moved Permanently",
	"Found",
	"See Other",
	"Not Modified",
	"Use Proxy",
	"(Unused)",
	"Temporary Redirect"
};

static const char *response_codes_4xx[] = {
	"Bad Request",
	"Unauthorized",
	"Payment Required",
	"Forbidden",
	"Not Found",
	"Method Not Allowed",
	"Not Acceptable",
	"Proxy Authentication Required",
	"Request Timeout",
	"Conflict",
	"Gone",
	"Length Required",
	"Precondition Failed",
	"Request Entity Too Large",
	"Request-URI Too Long",
	"Unsupported Media Type",
	"Requested Range Not Satisfiable",
	"Expectation Failed"
};

static const char *response_codes_5xx[] = {
	"Internal Server Error",
	"Not Implemented",
	"Bad Gateway",
	"Service Unavailable",
	"Gateway Timeout",
	"HTTP Version Not Supported"
};

static const char *unknown_http_response =
    "Unknown response when accessing webpage.";

static int max_codes[] = {
	0,
	sizeof(response_codes_1xx) / sizeof(char *),
	sizeof(response_codes_2xx) / sizeof(char *),
	sizeof(response_codes_3xx) / sizeof(char *),
	sizeof(response_codes_4xx) / sizeof(char *),
	sizeof(response_codes_5xx) / sizeof(char *)
};

static const char **responses[] = {
	NULL, response_codes_1xx, response_codes_2xx, response_codes_3xx,
	response_codes_4xx, response_codes_5xx
};

static const char *message_for_response_code(int code)
{
	const char *message = NULL;
	if (code < 100 || code > 599)
		message = unknown_http_response;
	else {
		int primary = code / 100;	/* Yields int in interval [1,6] */
		int subcode = code % 100;
		if (subcode >= max_codes[primary])
			message = unknown_http_response;
		else
			message = responses[primary][subcode];
	}
	return message;
}				/* message_for_response_code */

/*
 * Function: prompt_and_read
 * Arguments:
  ** prompt: prompt that user should see.
  ** buffer: buffer into which the data should be stored.
  ** max_length: maximum allowable length of input.
 ** error_msg: message to display if input exceeds maximum length.
 * Note: prompt and error_message should be message constants from messages.h.
 * Return value: none.  buffer contains input on return. */

/* We need to read two things from the user while authenticating: a username
 * and a password.  Here, the task of prompting and reading is encapsulated
 * in a function, and we call that function twice.
 * After the call, the buffer contains the user's input, without a newline.
 * The return value is the length of the string in buffer. */
static int
prompt_and_read(int prompt, char *buffer, int buffer_length, int error_message)
{
	bool reading = true;
	int n = 0;
	while (reading) {
		i_printf(prompt);
		fflush(stdout);
		if (!fgets(buffer, buffer_length, stdin))
			ebClose(0);
		n = strlen(buffer);
		if (n && buffer[n - 1] == '\n')
			buffer[--n] = '\0';	/* replace newline with NUL */
		if (n >= (MAXUSERPASS - 1)) {
			i_printf(error_message, MAXUSERPASS - 2);
			nl();
		} else
			reading = false;
	}
	return n;
}				/* prompt_and_read */

/*
 * Function: read_credentials
 * Arguments:
 ** buffer: buffer in which to place username and password.
 * Return value: true if credentials were read, false otherwise.

* Behavior: read a username and password from the user.  Store them in
 * the buffer, separated by a colon.
 * This function returns false in two situations.
 * 1. The program is not being run interactively.  The error message is
 * set to indicate this.
 * 2. The user aborted the login process by typing x"x".
 * Again, the error message reflects this condition.
*/

static bool read_credentials(char *buffer)
{
	int input_length = 0;
	bool got_creds = false;

	if (!isInteractive)
		setError(MSG_Authorize2);
	else {
		i_puts(MSG_WebAuthorize);
		input_length =
		    prompt_and_read(MSG_UserName, buffer, MAXUSERPASS,
				    MSG_UserNameLong);
		if (!stringEqual(buffer, "x")) {
			char *password_ptr = buffer + input_length + 1;
			prompt_and_read(MSG_Password, password_ptr, MAXUSERPASS,
					MSG_PasswordLong);
			if (!stringEqual(password_ptr, "x")) {
				got_creds = true;
				*(password_ptr - 1) = ':';	/* separate user and password with colon. */
			}
		}

		if (!got_creds)
			setError(MSG_LoginAbort);
	}

	return got_creds;
}				/* read_credentials */

/* Callback used by libcurl.
 * Gather all the http headers into one long string. */
static size_t
curl_header_callback(char *header_line, size_t size, size_t nmemb,
		     struct eb_curl_callback_data *data)
{
	size_t bytes_in_line = size * nmemb;
	stringAndBytes(&http_headers, &http_headers_len,
		       header_line, bytes_in_line);

	scan_http_headers(true);
	if (down_permitted && data->down_state == 0) {
		if (cf->mt && cf->mt->stream && pluginsOn) {
/* I don't think this ever happens, since streams are indicated by the protocol,
 * and we wouldn't even get here, but just in case -
 * stop the download and set the flag so we can pass this url
 * to the program that handles this kind of stream. */
			data->down_state = 6;
			return -1;
		}
		if (ht_content[0] && !memEqualCI(ht_content, "text/", 5) &&
		    !memEqualCI(ht_content, "application/xhtml+xml", 21) &&
		    (!pluginsOn || !cf->mt || cf->mt->download)) {
			data->down_state = 1;
			down_msg = MSG_Down;
			debugPrint(3, "potential download based on type %s",
				   ht_content);
		}
	}

	return bytes_in_line;
}				/* curl_header_callback */

/* Print text, discarding the unnecessary carriage return character. */
static void
prettify_network_text(const char *text, size_t size, FILE * destination)
{
	size_t i;
	for (i = 0; i < size; i++) {
		if (text[i] != '\r')
			fputc(text[i], destination);
	}
}				/* prettify_network_text */

/* Print incoming and outgoing headers.
 * Incoming headers are prefixed with curl<, and outgoing headers are
 * prefixed with curl> 
 * We may support more of the curl_infotype values soon. */

int
ebcurl_debug_handler(CURL * handle, curl_infotype info_desc, char *data,
		     size_t size, void *unused)
{
	static bool last_curlin = false;
	FILE *f = debugFile ? debugFile : stdout;

	if (info_desc == CURLINFO_HEADER_OUT) {
		fprintf(f, "curl>\n");
		prettify_network_text(data, size, f);
	} else if (info_desc == CURLINFO_HEADER_IN) {
		if (!last_curlin)
			fprintf(f, "curl<\n");
		prettify_network_text(data, size, f);
	} else;			/* Do nothing.  We don't care about this piece of data. */

	if (info_desc == CURLINFO_HEADER_IN)
		last_curlin = true;
	else if (info_desc)
		last_curlin = false;

	return 0;
}				/* ebcurl_debug_handler */

/* At this point, down_state = 1 */
static void setup_download(struct eb_curl_callback_data *data)
{
	const char *filepart;
	const char *answer;
	const struct MIMETYPE *mt;

/* if not run from a terminal then just return. */
	if (!isInteractive) {
		data->down_state = 0;
		return;
	}

/* If file is changed to a playlist or some such, just return */
	if (ht_cdfn && (mt = findMimeByURL(ht_cdfn)) && mt->stream) {
		debugPrint(3, "download aborted due to stream plugin");
		data->down_state = 0;
		return;
	}

	filepart = getFileURL(urlcopy, true);
top:
	answer = getFileName(down_msg, filepart, false, true);
/* space for a filename means read into memory */
	if (stringEqual(answer, " ")) {
		data->down_state = 0;	/* in memory download */
		return;
	}

	if (stringEqual(answer, "x") || stringEqual(answer, "X")) {
		data->down_state = -1;
		setError(MSG_DownAbort);
		return;
	}

	if (!envFileDown(answer, &answer)) {
		showError();
		goto top;
	}

	data->down_fd = creat(answer, 0666);
	if (data->down_fd < 0) {
		i_printf(MSG_NoCreate2, answer);
		nl();
		goto top;
	}
// free down_file, but not down_file2
	data->down_file = data->down_file2 = cloneString(answer);
	if (downDir) {
		int l = strlen(downDir);
		if (!strncmp(data->down_file2, downDir, l)) {
			data->down_file2 += l;
			if (data->down_file2[0] == '/')
				++data->down_file2;
		}
	}
	data->down_state = (down_bg ? 5 : 2);
	data->length = &data->down_length;
}				/* setup_download */

#ifdef _MSC_VER			// need fork()
/* At this point, down_state = 5 */
static void background_download(struct eb_curl_callback_data *data)
{
	data->down_state = -1;
/* perhaps a better error message here */
	setError(MSG_DownAbort);
	return;
}

int bg_jobs(bool iponly)
{
	return 0;
}

#else // !_MSC_VER

/* At this point, down_state = 5 */
static void background_download(struct eb_curl_callback_data *data)
{
	down_pid = fork();
	if (down_pid < 0) {	/* should never happen */
		data->down_state = -1;
/* perhaps a better error message here */
		setError(MSG_DownAbort);
		return;
	}

	if (down_pid) {		/* parent */
		struct BG_JOB *job;
		close(data->down_fd);
/* the error message here isn't really an error, but a progress message */
		setError(MSG_DownProgress);
		data->down_state = 3;

/* push job onto the list for tracking and display */
		job = allocMem(sizeof(struct BG_JOB) + strlen(data->down_file));
		job->pid = down_pid;
		job->state = 4;
		strcpy(job->file, data->down_file);
		job->file2 = data->down_file2 - data->down_file;
// round file size up to the nearest chunk.
// This will come out 0 only if the true size is 0.
		job->fsize = ((ht_length + (CHUNKSIZE - 1)) / CHUNKSIZE);
		addToListBack(&down_jobs, job);

		return;
	}

/* child doesn't need javascript */
	js_disconnect();
/* ignore interrupt, not sure about quit and hangup */
	signal(SIGINT, SIG_IGN);
	data->down_state = 4;
}				/* background_download */

/* show background jobs and return the number of jobs pending */
/* if iponly is true then just show in progress */
int bg_jobs(bool iponly)
{
	bool present = false, part;
	int numback = 0;
	struct BG_JOB *j;
	int pid, status;

/* gather exit status of background jobs */
	foreach(j, down_jobs) {
		if (j->state != 4)
			continue;
		pid = waitpid(j->pid, &status, WNOHANG);
		if (!pid)
			continue;
		j->state = -1;
		if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
			j->state = 0;
	}

/* three passes */
/* in progress */
	part = false;
	foreach(j, down_jobs) {
		size_t now_size;
		if (j->state != 4)
			continue;
		++numback;
		if (!part) {
			i_printf(MSG_InProgress);
			puts(" {");
			part = present = true;
		}
		printf("%s", j->file + j->file2);
		if (j->fsize)
			printf(" %d/%lu",
			       (fileSizeByName(j->file) / CHUNKSIZE), j->fsize);
		nl();
	}
	if (part)
		puts("}");

	if (iponly)
		return numback;

/* complete */
	part = false;
	foreach(j, down_jobs) {
		if (j->state != 0)
			continue;
		if (!part) {
			i_printf(MSG_Complete);
			puts(" {");
			part = present = true;
		}
		puts(j->file + j->file2);
	}
	if (part)
		puts("}");

/* failed */
	part = false;
	foreach(j, down_jobs) {
		if (j->state != -1)
			continue;
		if (!part) {
			i_printf(MSG_Failed);
			puts(" {");
			part = present = true;
		}
		puts(j->file + j->file2);
	}
	if (part)
		puts("}");

	if (!present)
		i_puts(MSG_Empty);

	return numback;
}				/* bg_jobs */
#endif // #ifndef _MSC_VER // need fork()

static char **novs_hosts;
size_t novs_hosts_avail;
size_t novs_hosts_max;

void addNovsHost(char *host)
{
	if (novs_hosts_max == 0) {
		novs_hosts_max = 32;
		novs_hosts = allocZeroMem(novs_hosts_max * sizeof(char *));
	} else if (novs_hosts_avail >= novs_hosts_max) {
		novs_hosts_max *= 2;
		novs_hosts =
		    reallocMem(novs_hosts, novs_hosts_max * sizeof(char *));
	}
	novs_hosts[novs_hosts_avail++] = host;
}				/* addNovsHost */

/* Return true if the cert for this host should be verified. */
static bool mustVerifyHost(const char *host)
{
	size_t this_host_len = strlen(host);
	size_t i;

	if (!verifyCertificates)
		return false;

	for (i = 0; i < novs_hosts_avail; i++) {
		size_t l1 = strlen(novs_hosts[i]);
		size_t l2 = this_host_len;
		if (l1 > l2)
			continue;
		l2 -= l1;
		if (!stringEqualCI(novs_hosts[i], host + l2))
			continue;
		if (l2 && host[l2 - 1] != '.')
			continue;
		return false;
	}
	return true;
}				/* mustVerifyHost */

void deleteNovsHosts(void)
{
	nzFree(novs_hosts);
	novs_hosts = NULL;
	novs_hosts_avail = novs_hosts_max = 0;
}				/* deleteNovsHosts */

CURLcode setCurlURL(CURL * h, const char *url)
{
	const char *host;
	unsigned long verify;
	const char *proxy = findProxyForURL(url);
	if (!proxy)
		proxy = "";
	else
		debugPrint(3, "proxy %s", proxy);
	host = getHostURL(url);
	if (!host)		// should never happen
		return CURLE_URL_MALFORMAT;
	verify = mustVerifyHost(host);
	curl_easy_setopt(h, CURLOPT_PROXY, proxy);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, verify);
	curl_easy_setopt(h, CURLOPT_SSL_VERIFYHOST, (verify ? 2 : 0));
	return curl_easy_setopt(h, CURLOPT_URL, url);
}				/* setCurlURL */

/*********************************************************************
Given a protocol and a domain, find the proxy server
to mediate your request.
This is the C version, using entries in .ebrc.
There is a javascript version of the same name, that we will support later.
This is a beginning, and it can be used even when javascript is disabled.
A return of null means DIRECT, and this is the default
if we don't match any of the proxy entries.
*********************************************************************/

static const char *findProxyInternal(const char *prot, const char *domain)
{
	struct PXENT *px = proxyEntries;
	int i;

/* first match wins */
	for (i = 0; i < maxproxy; ++i, ++px) {

		if (px->prot) {
			char *s = px->prot;
			char *t;
			int rc;
			while (*s) {
				t = strchr(s, '|');
				if (t)
					*t = 0;
				rc = stringEqualCI(s, prot);
				if (t)
					*t = '|';
				if (rc)
					goto domain;
				if (!t)
					break;
				s = t + 1;
			}
			continue;
		}

domain:
		if (px->domain) {
			int l1 = strlen(px->domain);
			int l2 = strlen(domain);
			if (l1 > l2)
				continue;
			l2 -= l1;
			if (!stringEqualCI(px->domain, domain + l2))
				continue;
			if (l2 && domain[l2 - 1] != '.')
				continue;
		}

		return px->proxy;
	}

	return 0;
}				/* findProxyInternal */

const char *findProxyForURL(const char *url)
{
	return findProxyInternal(getProtURL(url), getHostURL(url));
}				/* findProxyForURL */

/* expand a frame inline.
 * Pass a range of lines; you can expand all the frames in one go.
 * Return false if there is a problem fetching a web page,
 * or if none of the lines are frames. */
static int frameExpandLine(int lineNumber);
static int frameContractLine(int lineNumber);
static const char *stringInBufLine(const char *s, const char *t);
bool frameExpand(bool expand, int ln1, int ln2)
{
	int ln;			/* line number */
	int problem = 0, p;
	bool something_worked = false;

	for (ln = ln1; ln <= ln2; ++ln) {
		if (expand)
			p = frameExpandLine(ln);
		else
			p = frameContractLine(ln);
		if (p > problem)
			problem = p;
		if (p == 0)
			something_worked = true;
	}

	if (something_worked && problem < 3)
		problem = 0;
	if (problem == 1)
		setError(expand ? MSG_NoFrame1 : MSG_NoFrame2);
	if (problem == 2)
		setError(MSG_FrameNoURL);
	return (problem == 0);
}				/* frameExpand */

/* Problems: 0, frame expanded successfully.
 1 line is not a frame.
 2 frame doesn't have a valid url.
 3 Problem fetching the rul or rendering the page.  */
static int frameExpandLine(int ln)
{
	pst line;
	int tagno, start;
	const char *s;
	struct htmlTag *t;
	struct ebFrame *save_cf, *last_f;

	line = fetchLine(ln, -1);
	s = stringInBufLine(line, "Frame ");
	if (!s)
		return 1;
	if ((s = strchr(s, InternalCodeChar)) == NULL)
		return 2;
	tagno = strtol(s + 1, (char **)&s, 10);
	if (tagno < 0 || tagno >= cw->numTags || *s != '{')
		return 2;
	t = tagList[tagno];
	if (t->action != TAGACT_FRAME)
		return 1;

/* the easy case is if it's already been expanded before, we just unhide it. */
	if (t->f1) {
		t->contracted = false;
		return 0;
	}

	s = t->href;
	if (!s)
		return 2;

	save_cf = cf;
/* have to push a new frame before we read the web page */
	for (last_f = &(cw->f0); last_f->next; last_f = last_f->next) ;
	last_f->next = cf = allocZeroMem(sizeof(struct ebFrame));
	cf->owner = cw;
	cf->frametag = t;
	debugPrint(2, "fetch frame %s", s);
	if (!readFileArgv(s)) {
/* serverData was never set, or was freed do to some other error. */
/* We just need to pop the frame and return. */
		fileSize = -1;	/* don't print 0 */
		nzFree(cf->fileName);
		free(cf);
		last_f->next = 0;
		cf = save_cf;
		return 3;
	}

/*********************************************************************
readFile could return success and yet serverData is null.
This happens if httpConnect did something other than fetching data,
like playing a stream. Does that happen, even in a frame?
It can, if the frame is a youtube video, which is not unusual at all.
So check for serverData null here. Once again we pop the frame.
*********************************************************************/

	if (serverData == NULL) {
		nzFree(cf->fileName);
		free(cf);
		last_f->next = 0;
		cf = save_cf;
		fileSize = -1;
		return 0;
	}

	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->f_encoded = true;
		changeFileName = 0;
	} else {
		cf->fileName = cloneString(s);
	}

/* don't print the size of what we just fetched */
	fileSize = -1;

/* If we got some data it has to be html.
 * I should check for that, something like htmlTest in html.c,
 * but I'm too lazy to do that right now, so I'll just assume it's good. */

	cf->hbase = cloneString(cf->fileName);
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJavaContext();
	nzFree(newlocation);	/* should already be 0 */
	newlocation = 0;

	start = cw->numTags;
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	htmlGenerated = false;
	htmlNodesIntoTree(start, t);
	prerender(0);
	if (isJSAlive) {
		decorate(0);
		set_basehref(cf->hbase);
		runScriptsPending();
		runOnload();
		runScriptsPending();
		set_property_string(cf->docobj, "readyState", "complete");
	}

	if (cf->fileName) {
		int j = strlen(cf->fileName);
		cf->fileName = reallocMem(cf->fileName, j + 8);
		strcat(cf->fileName, ".browse");
	}

	t->f1 = cf;
	cf = save_cf;
	return 0;
}				/* frameExpandLine */

static int frameContractLine(int ln)
{
	struct htmlTag *t = line2frame(ln);
	if (!t)
		return 1;
	t->contracted = true;
	return 0;
}				/* frameContractLine */

struct htmlTag *line2frame(int ln)
{
	const char *line;
	int n, opentag = 0, ln1 = ln;
	const char *s;

	for (; ln; --ln) {
		line = (char *)fetchLine(ln, -1);
		if (!opentag && ln < ln1
		    && (s = stringInBufLine(line, "*--`\n"))) {
			for (--s; s > line && *s != InternalCodeChar; --s) ;
			if (*s == InternalCodeChar)
				opentag = atoi(s + 1);
			continue;
		}
		s = stringInBufLine(line, "*`--\n");
		if (!s)
			continue;
		for (--s; s > line && *s != InternalCodeChar; --s) ;
		if (*s != InternalCodeChar)
			continue;
		n = atoi(s + 1);
		if (!opentag)
			return tagList[n];
		if (n == opentag)
			opentag = 0;
	}

	return 0;
}				/* line2frame */

/* a text line in the buffer isn't a string; you can't use strstr */
static const char *stringInBufLine(const char *s, const char *t)
{
	int n = strlen(t);
	for (; *s != '\n'; ++s) {
		if (!strncmp(s, t, n))
			return s;
	}
	return 0;
}				/* stringInBufLine */

bool reexpandFrame(void)
{
	int j, start;
	struct htmlTag *t, *frametag;

/* cut the children off from the frame tag */
	cf = newloc_f;
	frametag = cf->frametag;
	for (t = frametag->firstchild; t; t = t->sibling) {
		t->deleted = true;
		t->step = 100;
		t->parent = 0;
	}
	frametag->firstchild = 0;

	delTimers(cf);
	delInputChanges(cf);
	freeJavaContext(cf);
	nzFree(cf->dw);
	cf->dw = 0;
	nzFree(cf->hbase);
	cf->hbase = 0;
	nzFree(cf->fileName);
	cf->fileName = newlocation;
	newlocation = 0;
	cf->f_encoded = false;
	nzFree(cf->firstURL);
	cf->firstURL = 0;

	if (!readFileArgv(cf->fileName)) {
/* serverData was never set, or was freed do to some other error. */
		fileSize = -1;	/* don't print 0 */
		return false;
	}

	if (serverData == NULL) {
/* frame replaced itself with a playable stream, what to do? */
		fileSize = -1;
		return true;
	}

	if (changeFileName) {
		nzFree(cf->fileName);
		cf->fileName = changeFileName;
		cf->f_encoded = true;
		changeFileName = 0;
	}

/* don't print the size of what we just fetched */
	fileSize = -1;

	cf->hbase = cloneString(cf->fileName);
	browseLocal = !isURL(cf->fileName);
	prepareForBrowse(serverData, serverDataLen);
	if (javaOK(cf->fileName))
		createJavaContext();
	start = cw->numTags;
/* call the tidy parser to build the html nodes */
	html2nodes(serverData, true);
	nzFree(serverData);	/* don't need it any more */
	htmlGenerated = false;
	htmlNodesIntoTree(start, frametag);
	prerender(0);
	if (isJSAlive) {
		decorate(0);
		set_basehref(cf->hbase);
		runScriptsPending();
		runOnload();
		runScriptsPending();
		set_property_string(cf->docobj, "readyState", "complete");
	}

	j = strlen(cf->fileName);
	cf->fileName = reallocMem(cf->fileName, j + 8);
	strcat(cf->fileName, ".browse");

	return true;
}				/* reexpandFrame */
