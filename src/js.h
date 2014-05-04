/* js.h, a layer between edbrowse and the javascript engine.
 * This is a form of encapsulation, and it has proven to be vital,
 * since Mozilla js changes quite often, and those changes
 * can be comfined to jsdom.cpp, jsloc.cpp, and html.cpp.
 * No other file need know about the javascript API,
 * and no other file should include jsapi.h. */

#ifndef JS_H
#define JS_H

#include <limits>
/* work around a bug where the standard UINT32_MAX isn't defined, I really hope this is correct */
#ifndef UINT32_MAX
#define UINT32_MAX std::numeric_limits<uint32_t>::max()
#endif
/* now we can include our jsapi */
#include <jsapi.h>
#include <jsfriendapi.h>

typedef JS::Heap < JSObject * >HeapRootedObject;

#define PROP_FIXED (JSPROP_ENUMERATE|JSPROP_READONLY|JSPROP_PERMANENT)

#include <string>
#include <vector>

using namespace std;

/* The structure for an html tag.
 * These tags are at times compared with js objects, or even created by js objects,
 * so the structure should be visible to the js machinery. */
struct htmlTag {
	HeapRootedObject jv;	/* corresponding java variable */
	int seqno;
	int ln;			/* line number */
	int lic;		/* list item count, highly overloaded */
	int action;
	const struct tagInfo *info;
/* the form that owns this input tag, etc */
	struct htmlTag *controller;
	eb_bool slash:1;	/* as in </A> */
	eb_bool balanced:1;	/* <foo> and </foo> */
	eb_bool retain:1;
	eb_bool multiple:1;
	eb_bool rdonly:1;
	eb_bool clickable:1;	/* but not an input field */
	eb_bool secure:1;
	eb_bool checked:1;
	eb_bool rchecked:1;	/* for reset */
	eb_bool post:1;		/* post, rather than get */
	eb_bool javapost:1;	/* post by calling javascript */
	eb_bool mime:1;		/* encode as mime, rather than url encode */
	eb_bool bymail:1;	/* send by mail, rather than http */
	eb_bool submitted:1;
	eb_bool onclick:1;
	eb_bool onsubmit:1;
	eb_bool onreset:1;
	eb_bool onchange:1;
	char subsup;		/* span turned into sup or sub */
	uchar itype;		/* input type = */
	short ninp;		/* number of nonhidden inputs */
	char *attrib;
	char *name, *id, *value, *href;
/* class=foo becomes className = "foo" when you carry from html to javascript,
 * don't ask me why. */
	char *classname;
	const char *inner;	/* for inner html */
};

/* htmlTag.action */
enum {
	TAGACT_ZERO, TAGACT_A, TAGACT_INPUT, TAGACT_TITLE, TAGACT_TA,
	TAGACT_BUTTON, TAGACT_SELECT, TAGACT_OPTION,
	TAGACT_NOP, TAGACT_JS, TAGACT_H, TAGACT_SUB, TAGACT_SUP,
	TAGACT_DW, TAGACT_BODY, TAGACT_HEAD,
	TAGACT_MUSIC, TAGACT_IMAGE, TAGACT_BR, TAGACT_IBR, TAGACT_P,
	TAGACT_BASE, TAGACT_META, TAGACT_PRE,
	TAGACT_DT, TAGACT_LI, TAGACT_HR, TAGACT_TABLE, TAGACT_TR, TAGACT_TD,
	TAGACT_DIV, TAGACT_SPAN, TAGACT_HTML,
	TAGACT_FORM, TAGACT_FRAME,
	TAGACT_MAP, TAGACT_AREA, TAGACT_SCRIPT, TAGACT_EMBED, TAGACT_OBJ,
};

/* htmlTag.itype */
enum {
	INP_RESET, INP_BUTTON, INP_IMAGE, INP_SUBMIT,
	INP_HIDDEN,
	INP_TEXT, INP_PW, INP_NUMBER, INP_FILE,
	INP_SELECT, INP_TA, INP_RADIO, INP_CHECKBOX,
};

/* The list of html tags for the current window. */
#define tagList (*((vector<struct htmlTag *> *)(cw->tags)))

/* Last tag in the above list. */
extern struct htmlTag *topTag;

struct ebWindowJSState {
	JSContext *jcx;		/* javascript context */
	HeapRootedObject jwin;	/* Window (AKA the global object) */
	HeapRootedObject jdoc;	/* document object */
	HeapRootedObject jwloc;	/* javascript window.location */
	HeapRootedObject jdloc;	/* javascript document.location */
};

/*********************************************************************
Place this macro at the top of any function that is invoked
from outside the javascript world.
It is a gateway that returns if javascript has died for this session,
or sets the compartment if javascript is still alive.
It returns retval, which is empty if the function is void.
Thus you can't put retval in parentheses,
even though it is usually good practice to do so.
*********************************************************************/

#define SWITCH_COMPARTMENT(retval) if (!isJSAlive) return retval; \
JSAutoRequest autoreq(cw->jss->jcx); \
JSAutoCompartment ac(cw->jss->jcx, cw->jss->jwin)

/* Prototypes of functions that are only used by the javascript layer. */
/* These can refer to javascript types in the javascript api. */
#include "ebjs.p"

#endif
