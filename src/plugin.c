/*********************************************************************
plugin.c: mime types and plugins.
Run audio players, pdf converters, etc, based on suffix or content-type.
*********************************************************************/

#include "eb.h"
#include <sys/stat.h>

#ifdef DOSLIKE
#include <process.h>		// for _getpid(),...
#define getpid _getpid
#endif

/* create an input or an output file for edbrowse under /tmp.
 * Since an external program may act upon this file, a certain suffix
 * may be required.
 * Fails if /tmp/.edbrowse does not exist or cannot be created. */
static char *tempin, *tempout;
static char *suffixin;		/* suffix of input file */
static char *suffixout;		/* suffix of output file */

static bool makeTempFilename(const char *suffix, int idx, bool output)
{
	char *filename;

// if no temp directory then we can't proceed
	if (!ebUserDir) {
		setError(MSG_TempNone);
		return false;
	}

	if (asprintf(&filename, "%s/pf%d-%d.%s",
		     ebUserDir, getpid(), idx, suffix) < 0)
		i_printfExit(MSG_MemAllocError, strlen(ebUserDir) + 24);

	if (output) {
// free the last one, don't need it any more.
		nzFree(tempout);
		tempout = filename;
	} else {
		nzFree(tempin);
		tempin = filename;
		suffixin = tempin + strlen(tempin) - strlen(suffix);
	}

	return true;
}				/* makeTempFilename */

static int tempIndex;

const struct MIMETYPE *findMimeBySuffix(const char *suffix)
{
	int i;
	int len = strlen(suffix);
	const struct MIMETYPE *m = mimetypes;

	if (!len)
		return NULL;

	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->suffix, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, suffix, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeBySuffix */

/* This looks for a match on suffix or on the url string */
const struct MIMETYPE *findMimeByURL(const char *url)
{
	char suffix[12];
	const char *post, *s, *t;
	const struct MIMETYPE *mt;
	const struct MIMETYPE *m;
	int i, j, l, url_length;

	post = url + strcspn(url, "?\1");
	for (s = post - 1; s >= url && *s != '.' && *s != '/'; --s) ;
	if (*s == '.') {
		++s;
		if (post < s + sizeof(suffix)) {
			strncpy(suffix, s, post - s);
			suffix[post - s] = 0;
			mt = findMimeBySuffix(suffix);
			if (mt)
				return mt;
		}
	}

/* not by suffix, let's look for a url match */
	url_length = strlen(url);
	m = mimetypes;
	for (i = 0; i < maxMime; ++i, ++m) {
		s = m->urlmatch;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, '|');
			if (!t)
				t = s + strlen(s);
			l = t - s;
			if (l && l <= url_length) {
				for (j = 0; j + l <= url_length; ++j) {
					if (memEqualCI(s, url + j, l))
						return m;
				}
			}
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeByURL */

const struct MIMETYPE *findMimeByFile(const char *filename)
{
	char suffix[12];
	const char *post, *s;
	post = filename + strlen(filename);
	for (s = post - 1; s >= filename && *s != '.' && *s != '/'; --s) ;
	if (*s != '.')
		return NULL;
	++s;
	if (post >= s + sizeof(suffix))
		return NULL;
	strncpy(suffix, s, post - s);
	suffix[post - s] = 0;
	return findMimeBySuffix(suffix);
}				/* findMimeByFile */

const struct MIMETYPE *findMimeByContent(const char *content)
{
	int i;
	int len = strlen(content);
	const struct MIMETYPE *m = mimetypes;

	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->content, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, content, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeByContent */

const struct MIMETYPE *findMimeByProtocol(const char *prot)
{
	int i;
	int len = strlen(prot);
	const struct MIMETYPE *m = mimetypes;

	for (i = 0; i < maxMime; ++i, ++m) {
		const char *s = m->prot, *t;
		if (!s)
			continue;
		while (*s) {
			t = strchr(s, ',');
			if (!t)
				t = s + strlen(s);
			if (t - s == len && memEqualCI(s, prot, len))
				return m;
			if (*t)
				++t;
			s = t;
		}
	}

	return NULL;
}				/* findMimeByProtocol */

/* The result is allocated */
char *pluginCommand(const struct MIMETYPE *m,
		    const char *infile, const char *outfile, const char *suffix)
{
	int len, inlen, outlen;
	const char *s;
	char *cmd, *t;

	if (!suffix)
		suffix = emptyString;
	if (!infile)
		infile = emptyString;
	if (!outfile)
		outfile = emptyString;

	len = 0;
	for (s = m->program; *s; ++s) {
		if (*s == '%' && s[1] == 'i') {
			inlen = shellProtectLength(infile);
			len += inlen;
			++s;
			continue;
		}
		if (*s == '%' && s[1] == 'o') {
			outlen = shellProtectLength(outfile);
			len += outlen;
			++s;
			continue;
		}
		++len;
	}
	++len;

	cmd = allocMem(len);
	t = cmd;
	for (s = m->program; *s; ++s) {
		if (*s == '%' && s[1] == 'i') {
			shellProtect(t, infile);
			t += inlen;
			++s;
			continue;
		}
		if (*s == '%' && s[1] == 'o') {
			shellProtect(t, outfile);
			t += outlen;
			++s;
			continue;
		}
		*t++ = *s;
	}
	*t = 0;

	debugPrint(3, "plugin %s", cmd);
	return cmd;
}				/* pluginCommand */

/* play the contents of the current buffer, or otherwise
 * act upon it based on the program corresponding to its mine type.
 * This is called from twoLetter() in buffers.c, and should return:
* 0 error, 1 success, 2 not a play buffer command */
int playBuffer(const char *line, const char *playfile)
{
	const struct MIMETYPE *mt = cf->mt;
	static char sufbuf[12];
	char *cmd;
	const char *suffix = NULL;
	char *buf;
	int buflen;
	char *infile;
	char c = line[2];

	if (c && c != '.')
		return 2;

	if (!cw->dol) {
		setError(cw->dirMode ? MSG_EmptyBuffer : MSG_AudioEmpty);
		return 0;
	}
	if (cw->browseMode) {
		setError(MSG_AudioBrowse);
		return 0;
	}
	if (cw->sqlMode) {
		setError(MSG_AudioDB);
		return 0;
	}
	if (cw->dirMode && !playfile) {
		setError(MSG_AudioDir);
		return 0;
	}

	if (playfile) {
/* play the file passed in */
		suffix = strrchr(playfile, '.') + 1;
		strcpy(sufbuf, suffix);
		suffix = sufbuf;
		mt = findMimeBySuffix(suffix);
		if (!mt || mt->outtype) {
			setError(MSG_SuffixBad, suffix);
			return 0;
		}
		cmd = pluginCommand(mt, playfile, 0, suffix);
		if (!cmd)
			return 0;
		goto play_command;
	}

	if (!mt) {
/* need to determine the mime type */
		if (c == '.') {
			suffix = line + 3;
		} else {
			if (cf->fileName) {
				const char *endslash;
				suffix = strrchr(cf->fileName, '.');
				endslash = strrchr(cf->fileName, '/');
				if (suffix && endslash && endslash > suffix)
					suffix = NULL;
			}
			if (!suffix) {
				setError(MSG_NoSuffix);
				return 0;
			}
			++suffix;
		}
		if (strlen(suffix) > 5) {
			setError(MSG_SuffixLong);
			return 0;
		}
		mt = findMimeBySuffix(suffix);
		if (!mt) {
			setError(MSG_SuffixBad, suffix);
			return 0;
		}
		cf->mt = mt;
	}

	if (!suffix) {
		suffix = mt->suffix;
		if (!suffix)
			suffix = "x";
		else {
			int i;
			for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
				if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
					break;
				sufbuf[i] = mt->suffix[i];
			}
			sufbuf[i] = 0;
			suffix = sufbuf;
		}
	}

	if (mt->outtype) {
		setError(MSG_SuffixBad, suffix);
		return 0;
	}

	++tempIndex;
	if (!makeTempFilename(suffix, tempIndex, false))
		return 0;
	infile = tempin;
	if (!isURL(cf->fileName) && !access(cf->fileName, 4) && !cw->changeMode)
		infile = cf->fileName;
	cmd = pluginCommand(mt, infile, 0, suffix);
	if (!cmd)
		return 0;
	if (infile == tempin) {
		if (!unfoldBuffer(context, false, &buf, &buflen)) {
			nzFree(cmd);
			return 0;
		}
		if (!memoryOutToFile(tempin, buf, buflen,
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
			unlink(tempin);
			nzFree(cmd);
			nzFree(buf);
			return 0;
		}
		nzFree(buf);
	}

play_command:
	eb_system(cmd, true);

	if (!cw->dirMode && infile == tempin)
		unlink(tempin);
	nzFree(cmd);

	return 1;
}				/* playBuffer */

bool playServerData(void)
{
	const struct MIMETYPE *mt = cf->mt;
	char *cmd;
	const char *suffix = mt->suffix;

	if (!suffix)
		suffix = "x";
	else {
		static char sufbuf[12];
		int i;
		for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
			if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
				break;
			sufbuf[i] = mt->suffix[i];
		}
		sufbuf[i] = 0;
		suffix = sufbuf;
	}

	++tempIndex;
	if (!makeTempFilename(suffix, tempIndex, false))
		return false;
	cmd = pluginCommand(cf->mt, tempin, 0, suffix);
	if (!cmd)
		return false;
	if (!memoryOutToFile(tempin, serverData, serverDataLen,
			     MSG_TempNoCreate2, MSG_NoWrite2)) {
		unlink(tempin);
		nzFree(cmd);
		return false;
	}
	eb_system(cmd, true);

	unlink(tempin);
	nzFree(cmd);

	return true;
}				/* playServerData */

/* return the name of the output file, or 0 upon failure */
/* Return "|" if output is in memory and not in a temp file. */
char *runPluginConverter(const char *buf, int buflen)
{
	const struct MIMETYPE *mt = cf->mt;
	char *cmd;
	const char *suffix = mt->suffix;
	bool ispipe = !strstr(mt->program, "%o");
	bool rc;
	char *infile;
	int system_ret = 0;

	if (!suffix)
		suffix = "x";
	else {
		static char sufbuf[12];
		int i;
		for (i = 0; i < sizeof(sufbuf) - 1; ++i) {
			if (mt->suffix[i] == ',' || mt->suffix[i] == 0)
				break;
			sufbuf[i] = mt->suffix[i];
		}
		sufbuf[i] = 0;
		suffix = sufbuf;
	}

	++tempIndex;
	if (!makeTempFilename(suffix, tempIndex, false))
		return 0;
	suffixout = (cf->mt->outtype == 'h' ? "html" : "txt");
	++tempIndex;
	if (!makeTempFilename(suffixout, tempIndex, true))
		return 0;
	infile = tempin;
	if (!isURL(cf->fileName) && !access(cf->fileName, 4) && !cw->changeMode)
		infile = cf->fileName;
	cmd = pluginCommand(cf->mt, infile, tempout, suffix);
	if (!cmd)
		return NULL;
	if (infile == tempin) {
		if (!memoryOutToFile(tempin, buf, buflen,
				     MSG_TempNoCreate2, MSG_NoWrite2)) {
			unlink(tempin);
			nzFree(cmd);
			return NULL;
		}
	}
#ifndef DOSLIKE
/* no popen call in windows I guess */
	if (ispipe) {
		FILE *p = popen(cmd, "r");
		if (!p) {
			setError(MSG_NoSpawn, cmd, errno);
			if (infile == tempin)
				unlink(tempin);
			nzFree(cmd);
			return NULL;
		}
/* borrow a global data array */
		rc = fdIntoMemory(fileno(p), &serverData, &serverDataLen);
		fclose(p);
		if (infile == tempin)
			unlink(tempin);
		nzFree(cmd);
		if (rc)
			return "|";
		nzFree(serverData);
		serverData = NULL;
		return NULL;
	}
#endif // #ifndef DOSLIKE

	system_ret = eb_system(cmd, false);

	if (infile == tempin)
		unlink(tempin);
	nzFree(cmd);
	if (!system_ret)
		return tempout;
	else
		return NULL;
}				/* runPluginConverter */
