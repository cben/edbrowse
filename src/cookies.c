/* cookies.c
 * Cookies
 * (c) 2002 Mikulas Patocka
 * This file is part of the Links project, released under GPL
 *
 * Modified by Karl Dahlke for integration with edbrowse.
 * Modified by Chris Brannon to allow cooperation with libcurl.
 */

#include "eb.h"

struct cookie {
    struct cookie *next;
    struct cookie *prev;
/* These are allocated */
    char *name, *value;
    char *server, *path, *domain;
    time_t expires;		/* zero means undefined */
    bool secure;
};

static int
count_tabs(const char *the_string)
{
    int str_index = 0, num_tabs = 0;
    for(str_index = 0; the_string[str_index] != '\0'; str_index++)
	if(the_string[str_index] == '\t')
	    num_tabs++;
    return num_tabs;
}				/* count_tabs */

#define FIELDS_PER_COOKIE_LINE 7

static struct cookie *
cookie_from_netscape_line(char *cookie_line)
{
    struct cookie *new_cookie = NULL;
    if(cookie_line && cookie_line[0]) {
/* Only parse the line if it is not a comment and it has the requisite number
 * of tabs.  Comment lines begin with a leading # symbol.
 * Syntax checking is rudimentary, because these lines are
 * machine-generated. */
	if(cookie_line[0] != '#' &&
	   count_tabs(cookie_line) == FIELDS_PER_COOKIE_LINE - 1) {
	    new_cookie = allocZeroMem(sizeof (struct cookie));
	    char *start = cookie_line;
	    char *end = strchr(cookie_line, '\t');
	    new_cookie->domain = pullString1(start, end);
	    start = end + 1;
/* Ignore the second field of the line. */
	    while(*start != '\t')
		start++;
	    start++;		/* skip tab */
	    end = strchr(start, '\t');
	    new_cookie->path = pullString1(start, end);
	    start = end + 1;
	    if(*start == 'T' || *start == 't')
		new_cookie->secure = true;
	    else
		new_cookie->secure = false;
	    start = strchr(start, '\t') + 1;
	    new_cookie->expires = strtol(start, &end, 10);
/* Now end points to the tab following the expiration time. */
	    start = end + 1;
	    end = strchr(start, '\t');
	    new_cookie->name = pullString1(start, end);
	    start = end + 1;
/* strcspn gives count of non-newline characters in string, which is the
 * length of the final field.  Either CR or LF is considered a newline. */
	    new_cookie->value = pullString(start, strcspn(start, "\r\n"));
	}
    }

    return new_cookie;
}				/* cookie_from_netscape_line */

static void
freeCookie(struct cookie *c)
{
    nzFree(c->name);
    nzFree(c->value);
    nzFree(c->server);
    nzFree(c->path);
    nzFree(c->domain);
}				/* freeCookie */

static struct listHead cookies = { &cookies, &cookies };

static bool displacedCookie;
static void
acceptCookie(struct cookie *c)
{
    struct cookie *d;
    displacedCookie = false;
    foreach(d, cookies) {
	if(stringEqualCI(d->name, c->name) &&
	   stringEqualCI(d->domain, c->domain)) {
	    displacedCookie = true;
	    delFromList(d);
	    freeCookie(d);
	    nzFree(d);
	    break;
	}
    }
    addToListBack(&cookies, c);
}				/* acceptCookie */


/* Tell libcurl about a new cookie.  Called when setting cookies from
 * JavaScript.
 * The function is pretty simple.  Construct a line of the form used by
 * the Netscape cookie file format, and pass that to libcurl. */
static CURLcode
cookieForLibcurl(const struct cookie *c)
{
/* Netscape format */
/* I have no clue what the second argument is suppose to be, */
/* I'm always calling it false. */
    char *cookLine =
       allocMem(strlen(c->path) + strlen(c->domain) + strlen(c->name) +
       strlen(c->value) + 48);
    sprintf(cookLine, "%s\tFALSE\t%s\t%s\t%u\t%s\t%s\n", c->domain, c->path,
       c->secure ? "TRUE" : "FALSE", (unsigned)c->expires, c->name, c->value);
    debugPrint(3, "cookie for libcurl");
    curl_easy_setopt(curl_handle, CURLOPT_COOKIELIST, cookLine);
    nzFree(cookLine);
}				/* cookieForLibcurl */

/* Should this server really specify this domain in a cookie? */
/* Domain must be the trailing substring of server. */
bool
domainSecurityCheck(const char *server, const char *domain)
{
    int i, dl, nd;
    dl = strlen(domain);
/* x.com or x.y.z */
    if(dl < 5)
	return false;
    if(dl > strlen(server))
	return false;
    i = strlen(server) - dl;
    if(!stringEqualCI(server + i, domain))
	return false;
    if(i && server[i - 1] != '.')
	return false;
    nd = 2;			/* number of dots */
    if(dl > 4 && domain[dl - 4] == '.') {
	static const char *const tld[] = {
	    "com", "edu", "net", "org", "gov", "mil", "int", "biz", NULL
	};
	if(stringInListCI(tld, domain + dl - 3) >= 0)
	    nd = 1;
    }
    for(i = 0; domain[i]; i++)
	if(domain[i] == '.')
	    if(!--nd)
		return true;
    return false;
}				/* domainSecurityCheck */

/* Let's jump right into it - parse a cookie, as received from a website. */
bool
receiveCookie(const char *url, const char *str)
{
    struct cookie *c;
    const char *p, *q, *server;
    char *date, *s;

    debugPrint(3, "%s", str);

    server = getHostURL(url);
    if(server == 0 || !*server)
	return false;

/* Cookie starts with name=value.  If we can't get that, go home. */
    for(p = str; *p != ';' && *p; p++) ;
    for(q = str; *q != '='; q++)
	if(!*q || q >= p)
	    return false;
    if(str == q)
	return false;

    c = allocZeroMem(sizeof (struct cookie));
    c->name = pullString1(str, q);
    ++q;
    if(p - q > 0)
	c->value = pullString1(q, p);
    else
	c->value = EMPTYSTRING;

    c->server = cloneString(server);

    if(date = extractHeaderParam(str, "expires")) {
	c->expires = parseHeaderDate(date);
	nzFree(date);
    } else if(date = extractHeaderParam(str, "max-age")) {
	int n = stringIsNum(date);
	if(n >= 0) {
	    time_t now = time(0);
	    c->expires = now + n;
	}
	nzFree(date);
    }

    c->path = extractHeaderParam(str, "path");
    if(!c->path) {
/* The url indicates the path for this cookie, if a path is not explicitly given */
	const char *dir, *dirend;
	getDirURL(url, &dir, &dirend);
	c->path = pullString1(dir, dirend);
    } else {
	if(!c->path[0] || c->path[strlen(c->path) - 1] != '/')
	    c->path = appendString(c->path, "/");
	if(c->path[0] != '/')
	    c->path = prependString(c->path, "/");
    }

    if(!(c->domain = extractHeaderParam(str, "domain")))
	c->domain = cloneString(server);
    if(c->domain[0] == '.')
	strcpy(c->domain, c->domain + 1);
    if(!domainSecurityCheck(server, c->domain)) {
	nzFree(c->domain);
	c->domain = cloneString(server);
    }

    if(s = extractHeaderParam(str, "secure")) {
	c->secure = true;
	nzFree(s);
    }

    cookieForLibcurl(c);
    freeCookie(c);
    nzFree(c);
    return true;
}				/* receiveCookie */

/* I'm assuming I can read the cookie file, process it,
 * and if necessary, write it out again, with the expired cookies deleted,
 * all before another edbrowse process interferes.
 * I've given it some thought, and I think I can ignore the race conditions. */
void
cookiesFromJar(void)
{
    char *cbuf, *s, *t;
    FILE *f;
    int n, cnt, expired, displaced;
    char *cbuf_end;
    time_t now;
    struct cookie *c;

    if(!cookieFile)
	return;
    if(!fileIntoMemory(cookieFile, &cbuf, &n))
	showErrorAbort();
    cbuf[n] = 0;
    cbuf_end = cbuf + n;
    time(&now);

    cnt = expired = displaced = 0;
    s = cbuf;

    while(s < cbuf_end) {
	t = s + strcspn(s, "\r\n");
/* t points to the first newline past s.  If there is no newline in s,
 * then it points to the NUL byte at end of s. */
	*t = '\0';
	c = cookie_from_netscape_line(s);

	if(c) {			/* Got a valid cookie line. */
	    cnt++;
	    if(c->expires < now) {
		freeCookie(c);
		nzFree(c);
		++expired;
	    } else {
		acceptCookie(c);
		displaced += displacedCookie;
	    }
	}

	s = t + 1;		/* Get ready to read more lines. */
/* Skip over blank lines, if necessary.  */
	while(s < cbuf_end && (*s == '\r' || *s == '\n'))
	    s++;
    }

    debugPrint(3, "%d persistent cookies, %d expired, %d displaced",
       cnt, expired, displaced);
    nzFree(cbuf);
    if(!(expired + displaced))
	return;

/* Pour the cookies back into the jar */
    f = fopen(cookieFile, "w");
    if(!f)
	i_printfExit(MSG_NoRebCookie, cookieFile);
    foreach(c, cookies)
       fprintf(f, "%s\tFALSE\t%s\t%s\t%u\t%s\t%s\n",
       c->domain, c->path,
       c->secure ? "TRUE" : "FALSE", (unsigned)c->expires, c->name, c->value);
    fclose(f);
}				/* cookiesFromJar */

static bool
isInDomain(const char *d, const char *s)
{
    int dl = strlen(d);
    int sl = strlen(s);
    int j = sl - dl;
    if(j < 0)
	return false;
    if(!memEqualCI(d, s + j, dl))
	return false;
    if(j && s[j - 1] != '.')
	return false;
    return true;
}				/* isInDomain */

static bool
isPathPrefix(const char *d, const char *s)
{
    int dl = strlen(d);
    int sl = strlen(s);
    if(dl > sl)
	return false;
    return !memcmp(d, s, dl);
}				/* isPathPrefix */



void
sendCookies(char **s, int *l, const char *url, bool issecure)
{
    const char *server = getHostURL(url);
    const char *data = getDataURL(url);
    int nc = 0;			/* new cookie */
    struct cookie *c = NULL;
    time_t now;
    struct curl_slist *known_cookies = NULL;
    struct curl_slist *cursor = NULL;
    curl_easy_getinfo(curl_handle, CURLINFO_COOKIELIST, &known_cookies);

    if(!url || !server || !data)
	return;

    if(data > url && data[-1] == '/')
	data--;
    if(!*data)
	data = "/";
    time(&now);

/* Can't use foreach here, since known_cookies is just a pointer. */
    cursor = known_cookies;

/* The code at the top of the loop guards against a memory leak.
 * Otherwise, structs could become inaccessible after continue statements. */
    while(cursor != NULL) {
	if(c != NULL) {		/* discard un-freed cookie structs */
	    freeCookie(c);
	    nzFree(c);
	}
	c = cookie_from_netscape_line(cursor->data);
	cursor = cursor->next;
	if(c == NULL)		/* didn't read a cookie line. */
	    continue;
	if(!isInDomain(c->domain, server))
	    continue;
	if(!isPathPrefix(c->path, data))
	    continue;
	if(c->expires && c->expires < now)
	    continue;
	if(c->secure && !issecure)
	    continue;
/* We're good to go. */
	if(!nc)
	    stringAndString(s, l, "Cookie: "), nc = 1;
	else
	    stringAndString(s, l, "; ");
	stringAndString(s, l, c->name);
	stringAndChar(s, l, '=');
	stringAndString(s, l, c->value);
	debugPrint(3, "send cookie %s=%s", c->name, c->value);
    }

    if(c != NULL) {
	freeCookie(c);
	nzFree(c);
    }

    if(known_cookies != NULL)
	curl_slist_free_all(known_cookies);
    if(nc)
	stringAndString(s, l, eol);
}				/* sendCookies */
