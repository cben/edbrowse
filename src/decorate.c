/*********************************************************************
decorate.c:
sanitize a tree of nodes produced by html,
and decorate the tree with the corresponding js objects.
A <form> tag has a corresponding Form object in the js world, etc.
This is done for the html that is on the initial web page,
and any html that is produced by javascript via
foo.innerHTML = string or document.write(string).
*********************************************************************/

#include "eb.h"

/* The current (foreground) edbrowse window.
 * This is replaced with a stub when run within the javascript process. */
struct ebWindow *cw;

/* traverse the tree of nodes with a callback function */
nodeFunction traverse_callback;

/* possible callback functions in this file */
static void prerenderNode(struct htmlTag *node, bool opentag);
static void jsNode(struct htmlTag *node, bool opentag);

static void traverseNode(struct htmlTag *node)
{
	struct htmlTag *child;
	(*traverse_callback) (node, true);
	for (child = node->firstchild; child; child = child->sibling)
		traverseNode(child);
	(*traverse_callback) (node, false);
}				/* traverseNode */

void traverseAll(int start)
{
	struct htmlTag *t;
	int i;

	for (i = start; i < cw->numTags; ++i) {
		t = tagList[i];
		if (!t->parent && !t->slash && t->step < 10)
			traverseNode(t);
	}
}				/* traverseAll */

static int nopt;		/* number of options */
/* None of these tags nest, so it is reasonable to talk about
 * the current open tag. */
static struct htmlTag *currentForm, *currentSel, *currentOpt;
static struct htmlTag *currentTitle, *currentScript, *currentTA;
static struct htmlTag *currentA;
static char *radioCheck;
static int radio_l;

const char *attribVal(const struct htmlTag *t, const char *name)
{
	const char *v;
	if (!t->attributes)
		return 0;
	int j = stringInListCI(t->attributes, name);
	if (j < 0)
		return 0;
	v = t->atvals[j];
	if (!v || !*v)
		return 0;
	return v;
}				/* attribVal */

static bool attribPresent(const struct htmlTag *t, const char *name)
{
	int j = stringInListCI(t->attributes, name);
	return (j >= 0);
}				/* attribPresent */

static void linkinTree(struct htmlTag *parent, struct htmlTag *child)
{
	struct htmlTag *c, *d;
	child->parent = parent;

	if (!parent->firstchild) {
		parent->firstchild = child;
		return;
	}

	for (c = parent->firstchild; c; c = c->sibling) {
		d = c;
	}
	d->sibling = child;
}				/* linkinTree */

static void makeButton(void)
{
	struct htmlTag *t = newTag("input");
	t->controller = currentForm;
	t->itype = INP_SUBMIT;
	t->value = emptyString;
	t->created = true;
	linkinTree(currentForm, t);
}				/* makeButton */

struct htmlTag *findOpenTag(struct htmlTag *t, int action)
{
	while (t = t->parent)
		if (t->action == action)
			return t;
	return 0;
}				/* findOpenTag */

struct htmlTag *findOpenList(struct htmlTag *t)
{
	while (t = t->parent)
		if (t->action == TAGACT_OL || t->action == TAGACT_UL)
			return t;
	return 0;
}				/* findOpenList */

void formControl(struct htmlTag *t, bool namecheck)
{
	int itype = t->itype;
	char *myname = (t->name ? t->name : t->id);
	struct htmlTag *cf = currentForm;
	if (!cf) {
/* nodes could be created dynamically, not through html */
		cf = findOpenTag(t, TAGACT_FORM);
	}
	if (cf)
		t->controller = cf;
	else if (itype != INP_BUTTON)
		debugPrint(3, "%s is not part of a fill-out form",
			   t->info->desc);
	if (namecheck && !myname)
		debugPrint(3, "%s does not have a name", t->info->desc);
}				/* formControl */

static const char *const inp_types[] = {
	"reset", "button", "image", "submit",
	"hidden",
	"text", "password", "number", "file",
	"select", "textarea", "radio", "checkbox",
	0
};

/* helper function for input tag */
void htmlInputHelper(struct htmlTag *t)
{
	int n = INP_TEXT;
	int len;
	char *myname = (t->name ? t->name : t->id);
	const char *s = attribVal(t, "type");
	if (s) {
		n = stringInListCI(inp_types, s);
		if (n < 0) {
			debugPrint(3, "unrecognized input type %s", s);
			n = INP_TEXT;
		}
	} else if (stringEqual(t->info->name, "BUTTON")) {
		n = INP_BUTTON;
	}
	t->itype = n;

	s = attribVal(t, "maxlength");
	len = 0;
	if (s)
		len = stringIsNum(s);
	if (len > 0)
		t->lic = len;

/* In this case an empty value should be "", not null */
	if (t->value == 0)
		t->value = emptyString;
	if (t->rvalue == 0)
		t->rvalue = cloneString(t->value);

	if (n == INP_RADIO && t->checked && radioCheck && myname) {
		char namebuf[200];
		if (strlen(myname) < sizeof(namebuf) - 3) {
			if (!*radioCheck)
				stringAndChar(&radioCheck, &radio_l, '|');
			sprintf(namebuf, "|%s|", t->name);
			if (strstr(radioCheck, namebuf)) {
				debugPrint(3,
					   "multiple radio buttons have been selected");
				return;
			}
			stringAndString(&radioCheck, &radio_l, namebuf + 1);
		}
	}

	/* radio name */
	/* Even the submit fields can have a name, but they don't have to */
	formControl(t, (n > INP_SUBMIT));
}				/* htmlInputHelper */

/* return an allocated string containing the text entries for the checked options */
char *displayOptions(const struct htmlTag *sel)
{
	const struct htmlTag *t;
	char *opt;
	int opt_l;
	int i;

	opt = initString(&opt_l);
	for (i = 0; i < cw->numTags; ++i) {
		t = tagList[i];
		if (t->controller != sel)
			continue;
		if (!t->checked)
			continue;
		if (*opt)
			stringAndChar(&opt, &opt_l, ',');
		stringAndString(&opt, &opt_l, t->textval);
	}

	return opt;
}				/* displayOptions */

static void prerenderNode(struct htmlTag *t, bool opentag)
{
	int itype;		/* input type */
	int j;
	int action = t->action;
	const char *a;		/* usually an attribute */

#if 0
	printf("prend %c%s\n", (opentag ? ' ' : '/'), t->info->name);
#endif

	if (t->step >= 1)
		return;
	if (!opentag)
		t->step = 1;

	switch (action) {
	case TAGACT_TEXT:
		if (!opentag || !t->textval)
			break;

		if (currentTitle) {
			if (!cw->ft) {
				cw->ft = cloneString(t->textval);
				spaceCrunch(cw->ft, true, false);
			}
			t->deleted = true;
			break;
		}

		if (currentOpt) {
			currentOpt->textval = cloneString(t->textval);
			spaceCrunch(currentOpt->textval, true, false);
			t->deleted = true;
			break;
		}

		if (currentScript) {
			currentScript->textval = cloneString(t->textval);
			t->deleted = true;
			break;
		}

		if (currentTA) {
			currentTA->value = cloneString(t->textval);
/* Sometimes tidy lops off the last newline character; it depends on
 * the tag following. And even if it didn't end in nl in the original html,
 * <textarea>foobar</textarea>, it probably should,
 * as it goes into a new buffer. */
			j = strlen(currentTA->value);
			if (j && currentTA->value[j - 1] != '\n') {
				currentTA->value =
				    reallocMem(currentTA->value, j + 2);
				currentTA->value[j] = '\n';
				currentTA->value[j + 1] = 0;
			}
			currentTA->rvalue = cloneString(currentTA->value);
			t->deleted = true;
			break;
		}

/* text is on the page */
		if (currentA) {
			char *s;
			for (s = t->textval; *s; ++s)
				if (isalnumByte(*s)) {
					currentA->textin = true;
					break;
				}
		}
		break;

	case TAGACT_TITLE:
		currentTitle = (opentag ? t : 0);
		break;

	case TAGACT_SCRIPT:
		currentScript = (opentag ? t : 0);
		break;

	case TAGACT_A:
		currentA = (opentag ? t : 0);
		break;

	case TAGACT_FORM:
		if (opentag) {
			currentForm = t;
			a = attribVal(t, "method");
			if (a) {
				if (stringEqualCI(a, "post"))
					t->post = true;
				else if (!stringEqualCI(a, "get"))
					debugPrint(3,
						   "form method should be get or post");
			}
			a = attribVal(t, "enctype");
			if (a) {
				if (stringEqualCI(a, "multipart/form-data"))
					t->mime = true;
				else if (!stringEqualCI
					 (a,
					  "application/x-www-form-urlencoded"))
					debugPrint(3,
						   "unrecognized enctype, plese use multipart/form-data or application/x-www-form-urlencoded");
			}
			if (a = t->href) {
				const char *prot = getProtURL(a);
				if (prot) {
					if (stringEqualCI(prot, "mailto"))
						t->bymail = true;
					else if (stringEqualCI
						 (prot, "javascript"))
						t->javapost = true;
					else if (stringEqualCI(prot, "https"))
						t->secure = true;
					else if (!stringEqualCI(prot, "http"))
						debugPrint(3,
							   "form cannot submit using protocol %s",
							   prot);
				}
			}

			nzFree(radioCheck);
			radioCheck = initString(&radio_l);
		}
		if (!opentag && currentForm) {
			if (t->href && !t->submitted) {
				makeButton();
				t->submitted = true;
			}
			currentForm = 0;
		}
		break;

	case TAGACT_INPUT:
		if (!opentag)
			break;
		htmlInputHelper(t);
		itype = t->itype;
		if (itype == INP_HIDDEN)
			break;
		if (currentForm) {
			++currentForm->ninp;
			if (itype == INP_SUBMIT || itype == INP_IMAGE)
				currentForm->submitted = true;
			if (itype == INP_BUTTON && t->onclick)
				currentForm->submitted = true;
			if (itype > INP_HIDDEN && itype <= INP_SELECT
			    && t->onchange)
				currentForm->submitted = true;
		}
		break;

	case TAGACT_OPTION:
		if (!opentag) {
			currentOpt = 0;
			break;
		}
		if (!currentSel) {
			debugPrint(3,
				   "option appears outside a select statement");
			break;
		}
		currentOpt = t;
		t->controller = currentSel;
		t->lic = nopt++;
		if (attribPresent(t, "selected")) {
			if (currentSel->lic && !currentSel->multiple)
				debugPrint(3, "multiple options are selected");
			else {
				t->checked = t->rchecked = true;
				++currentSel->lic;
			}
		}
		if (!t->value)
			t->value = emptyString;
		t->textval = emptyString;
		break;

	case TAGACT_SELECT:
		if (opentag) {
			currentSel = t;
			nopt = 0;
			t->itype = INP_SELECT;
			formControl(t, true);
		} else {
			currentSel = 0;
			t->action = TAGACT_INPUT;
			t->value = displayOptions(t);
		}
		break;

	case TAGACT_TA:
		if (opentag) {
			currentTA = t;
			t->itype = INP_TA;
			formControl(t, true);
		} else {
			t->action = TAGACT_INPUT;
			if (!t->value) {
/* This can only happen it no text inside, <textarea></textarea> */
/* like the other value fields, it can't be null */
				t->rvalue = t->value = emptyString;
			}
			j = sideBuffer(0, t->value, -1, 0);
			t->lic = j;
			currentTA = 0;
		}
		break;

	case TAGACT_META:
		if (opentag) {
/* This function doesn't do anything inside the js process.
 * It only works when scanning the original web page.
 * Thus I assume meta tags that set cookies, or keywords, or description,
 * or a refresh directive, are there from the get-go.
 * If js was going to generate a cookie it would just set document.cookie,
 * it wouldn't build a meta tag to set the cookie and then
 * appendChild it to head, right? */
			htmlMetaHelper(t);
		}
		break;

	case TAGACT_TR:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TABLE);
		break;

	case TAGACT_TD:
		if (opentag)
			t->controller = findOpenTag(t, TAGACT_TR);
		break;

	case TAGACT_SPAN:
		if (!opentag)
			break;
		if (!(a = t->classname))
			break;
		if (stringEqualCI(a, "sup"))
			action = TAGACT_SUP;
		if (stringEqualCI(a, "sub"))
			action = TAGACT_SUB;
		if (stringEqualCI(a, "ovb"))
			action = TAGACT_OVB;
		t->action = action;
		break;

	case TAGACT_OL:
/* look for start parameter for numbered list */
		if (opentag) {
			a = attribVal(t, "start");
			if (a && (j = stringIsNum(a)) >= 0)
				t->slic = j - 1;
		}
		break;

	}			/* switch */
}				/* prerenderNode */

void prerender(int start)
{
	currentForm = currentSel = currentOpt = NULL;
	currentTitle = currentScript = currentTA = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
	traverse_callback = prerenderNode;
	traverseAll(start);
	currentForm = NULL;
	nzFree(radioCheck);
	radioCheck = 0;
}				/* prerender */

/* create a new url with constructor */
jsobjtype instantiate_url(jsobjtype parent, const char *name, const char *url)
{
	jsobjtype uo;		/* url object */
	uo = instantiate(parent, name, "URL");
	if (uo)
		set_property_string(uo, "href", url);
	return uo;
}				/* instantiate_url */

static void handlerSet(jsobjtype ev, const char *name, const char *code)
{
	enum ej_proptype hasform = has_property(ev, "form");
	char *newcode = allocMem(strlen(code) + 60);
	strcpy(newcode, "with(document) { ");
	if (hasform)
		strcat(newcode, "with(this.form) { ");
	strcat(newcode, code);
	if (hasform)
		strcat(newcode, " }");
	strcat(newcode, " }");
	set_property_function(ev, name, newcode);
	nzFree(newcode);
}				/* handlerSet */

static void set_onhandler(const struct htmlTag *t, const char *name)
{
	const char *s;
	if ((s = attribVal(t, name)) && t->jv)
		handlerSet(t->jv, name, s);
}				/* set_onhandler */

static void set_onhandlers(const struct htmlTag *t)
{
/* I don't do anything with onkeypress, onfocus, etc,
 * these are just the most common handlers */
	if (t->onclick)
		set_onhandler(t, "onclick");
	if (t->onchange)
		set_onhandler(t, "onchange");
	if (t->onsubmit)
		set_onhandler(t, "onsubmit");
	if (t->onreset)
		set_onhandler(t, "onreset");
	if (t->onload)
		set_onhandler(t, "onload");
	if (t->onunload)
		set_onhandler(t, "onunload");
}				/* set_onhandlers */

static const char *fakePropName(void)
{
	static char fakebuf[24];
	static int idx = 0;
	++idx;
	sprintf(fakebuf, "gc$$%d", idx);
	return fakebuf;
}				/*fakePropName */

static jsobjtype establish_js_option(jsobjtype obj, int idx)
{
	jsobjtype oa;		/* option array */
	jsobjtype oo;		/* option object */
	jsobjtype fo;		/* form object */

	if ((oa = get_property_object(obj, "options")) == NULL)
		return NULL;
	if ((oo = instantiate_array_element(oa, idx, "Option")) == NULL)
		return NULL;

/* option.form = select.form */
	fo = get_property_object(obj, "form");
	if (fo)
		set_property_object(oo, "form", fo);

	return oo;
}				/* establish_js_option */

static void establish_inner(jsobjtype obj, const char *start, const char *end,
			    bool isText)
{
	const char *s = emptyString;
	const char *name = (isText ? "innerText" : "innerHTML");
	if (start) {
		s = start;
		if (end)
			s = pullString(start, end - start);
	}
	set_property_string(obj, name, s);
	if (start && end)
		nzFree((char *)s);
/* Anything with an innerHTML might also have a style. */
	instantiate(obj, "style", 0);
}				/* establish_inner */

static void domLink(struct htmlTag *t, const char *classname,	/* instantiate this class */
		    const char *href, const char *list,	/* next member of this array */
		    jsobjtype owner, int radiosel)
{
	jsobjtype master;
	jsobjtype alist = 0;
	jsobjtype io = 0;	/* input object */
	unsigned length;
	bool dupname = false;
/* some strings from the html tag */
	const char *symname = t->name;
	const char *idname = t->id;
	const char *membername = 0;	/* usually symname */
	const char *href_url = t->href;
	const char *htmlclass = t->classname;

	debugPrint(5, "domLink %s.%d name %s",
		   classname, radiosel, (symname ? symname : emptyString));

	if (symname && has_property(owner, symname)) {
/*********************************************************************
This could be a duplicate name.
Yes, that really happens.
Link to the first tag having this name,
and link the second tag under a fake name, so gc won't throw it away.
Or - it could be a duplicate name because multiple radio buttons
all share the same name.
The first time, we create the array,
and thereafter we just link under that array.
Or - and this really does happen -
an input tag could have the name action, colliding with form.action.
I have no idea what to do here.
I will assume the tag displaces the action.
That means javascript cannot change the action of the form,
which it rarely does anyways.
When it refers to form.action, that will be the input tag.
I'll check for that one first.
Yeah, it makes my head spin too.
*********************************************************************/

		if (stringEqual(symname, "action")) {
			jsobjtype ao;	/* action object */
			ao = get_property_object(owner, symname);
			if (ao == NULL)
				return;
/* actioncrash tells me if we've already had this collision */
			if (!has_property(ao, "actioncrash")) {
				delete_property(owner, symname);
/* advance, as though this were not found */
				goto afterfound;
			}
		}

/* radiosel is 1 for radio buttons and 2 for select */
		if (radiosel == 1) {
/* name present and radio buttons, name should be the array of buttons */
			io = get_property_object(owner, symname);
			if (io == NULL)
				return;
		} else {
/* don't know why the duplicate name */
			dupname = true;
		}
	}

afterfound:
/* The input object is nonzero if&only if the input is a radio button,
 * and not the first button in the set, thus it isce the array containing
 * these buttons. */

	if (io == NULL) {
/*********************************************************************
Ok, the above condition does not hold.
We'll be creating a new object under owner, but through what name?
The name= tag, unless it's a duplicate,
or id= if there is no name=, or a fake name just to protect it from gc.
*********************************************************************/

		if (!symname && idname) {
/* id= must not displace submit, reset, or action.
 * Example www.startpage.com, where id=submit */
			if (!stringEqual(idname, "submit") &&
			    !stringEqual(idname, "reset") &&
			    !stringEqual(idname, "action"))
				membername = idname;
		} else if (symname && !dupname) {
			membername = symname;
		}
		if (!membername)
			membername = fakePropName();

		if (radiosel) {
/* The first radio button, or input type=select */
/* Either way the form element is suppose to be an array. */
			io = instantiate_array(owner, membername);
			if (io == NULL)
				return;
			if (radiosel == 1) {
				set_property_string(io, "type", "radio");
				set_property_string(io, "nodeName", "radio");
			} else {
/* I've read some docs that say select is itself an array,
 * and then references itself as an array of options.
 * Self referencing? Really? Well it seems to work. */
				set_property_object(io, "options", io);
				set_property_number(io, "selectedIndex", -1);
			}
		} else {
/* A standard input element, just create it. */
			io = instantiate(owner, membername, classname);
			if (io == NULL)
				return;

/* not an array; needs the childNodes array beneath it for the children */
			instantiate_array(io, "childNodes");
/* and in the special case of form, also need an array of elements */
			if (stringEqual(classname, "Form"))
				instantiate_array(io, "elements");
		}

		if (membername == symname) {
/* link to document.all */
			master = get_property_object(cw->docobj, "all");
			if (master == NULL)
				return;
			set_property_object(master, symname, io);

			if (stringEqual(symname, "action"))
				set_property_bool(io, "actioncrash", true);
		}

		if (list)
			alist = get_property_object(owner, list);
		if (alist) {
			length = get_arraylength(alist);
			if (length < 0)
				return;
			set_array_element_object(alist, length, io);
			if (symname && !dupname)
				set_property_object(alist, symname, io);
			if (idname && membername != idname)
				set_property_object(alist, idname, io);
		}		/* list indicated */
	}

	if (radiosel == 1) {
/* drop down to the element within the radio array, and return that element */
/* w becomes the object associated with this radio button */
/* io is, by assumption, an array */
		jsobjtype w;
		length = get_arraylength(io);
		if (length < 0)
			return;
		w = instantiate_array_element(io, length, "Element");
		if (w == NULL)
			return;
		io = w;
	}

	if (symname)
		set_property_string(io, "name", symname);

	if (idname) {
/* io.id becomes idname, and idMaster.idname becomes io
 * In case of forms, v.id should remain undefined.  So we can have
 * a form field named "id". */
		if (!stringEqual(classname, "Form"))
			set_property_string(io, "id", idname);
		master = get_property_object(cw->docobj, "idMaster");
		set_property_object(master, idname, io);
	}

	if (href && href_url)
		instantiate_url(io, href, href_url);

	if (stringEqual(classname, "Element")) {
/* link back to the form that owns the element */
		set_property_object(io, "form", owner);
	}

	if (htmlclass) {
		set_property_string(io, "className", htmlclass);
	}

	t->jv = io;

	set_property_string(io, "nodeName", t->info->name);

	if (stringEqual(classname, "Html")) {
		set_property_object(cw->docobj, "documentElement", io);
	}

	if (stringEqual(classname, "Body")) {
/* here are a few attributes that come in with the body */
		set_property_object(cw->docobj, "body", io);
		set_property_number(io, "clientHeight", 768);
		set_property_number(io, "clientWidth", 1024);
		set_property_number(io, "offsetHeight", 768);
		set_property_number(io, "offsetWidth", 1024);
		set_property_number(io, "scrollHeight", 768);
		set_property_number(io, "scrollWidth", 1024);
		set_property_number(io, "scrollTop", 0);
		set_property_number(io, "scrollLeft", 0);
	}

	if (stringEqual(classname, "Head")) {
		set_property_object(cw->docobj, "head", io);
	}

}				/* domLink */

static const char defvl[] = "defaultValue";
static const char defck[] = "defaultChecked";
static const char defsel[] = "defaultSelected";

static void formControlJS(struct htmlTag *t)
{
	const char *typedesc;
	int itype = t->itype;
	int isradio = itype == INP_RADIO;
	int isselect = (itype == INP_SELECT) * 2;
	char *myname = (t->name ? t->name : t->id);
	const struct htmlTag *form = t->controller;

	if (form && form->jv)
		domLink(t, "Element", 0, "elements", form->jv,
			isradio | isselect);
	else
		domLink(t, "Element", 0, 0, cw->docobj, isradio | isselect);
	if (!t->jv)
		return;

	set_onhandlers(t);

	if (itype <= INP_RADIO) {
		set_property_string(t->jv, "value", t->value);
		if (itype != INP_FILE) {
/* No default value on file, for security reasons */
			set_property_string(t->jv, defvl, t->value);
		}		/* not file */
	}

	if (isselect)
		typedesc = t->multiple ? "select-multiple" : "select-one";
	else
		typedesc = inp_types[itype];
	set_property_string(t->jv, "type", typedesc);

	if (itype >= INP_RADIO) {
		set_property_bool(t->jv, "checked", t->checked);
		set_property_bool(t->jv, defck, t->checked);
	}
}				/* formControlJS */

static void optionJS(struct htmlTag *t)
{
	struct htmlTag *sel = t->controller;
	const char *tx = t->textval;

	if (!sel)
		return;

	if (!tx) {
		debugPrint(3, "empty option");
	} else {
		if (!t->value)
			t->value = cloneString(tx);
	}

/* no point if the controlling select doesn't have a js object */
	if (!sel->jv)
		return;

	t->jv = establish_js_option(sel->jv, t->lic);
	set_property_string(t->jv, "text", t->textval);
	set_property_string(t->jv, "value", t->value);
	set_property_string(t->jv, "nodeName", "option");
	set_property_bool(t->jv, "selected", t->checked);
	set_property_bool(t->jv, defsel, t->checked);

	if (t->checked && !sel->multiple) {
		set_property_number(sel->jv, "selectedIndex", t->lic);
		set_property_string(sel->jv, "value", t->value);
	}
}				/* optionJS */

static void jsNode(struct htmlTag *t, bool opentag)
{
	int itype;		/* input type */
	const struct tagInfo *ti = t->info;
	int action = t->action;
	const struct htmlTag *above;
	const char *a;

/* all the js variables are on the open tag */
	if (!opentag)
		return;
	if (t->step >= 2)
		return;
	t->step = 2;

#if 0
	printf("decorate %s\n", t->info->name);
#endif

	switch (action) {
	case TAGACT_TEXT:
		t->jv = instantiate(cw->docobj, fakePropName(), "TextNode");
		if (t->jv) {
			const char *w = t->textval;
			if (!w)
				w = emptyString;
			set_property_string(t->jv, "data", w);
			set_property_string(t->jv, "nodeName", "text");
		}
		break;

	case TAGACT_SCRIPT:
		domLink(t, "Script", "src", "scripts", cw->docobj, 0);
		a = attribVal(t, "type");
		if (a)
			set_property_string(t->jv, "type", a);
		a = attribVal(t, "language");
		if (a)
			set_property_string(t->jv, "language", a);
		break;

	case TAGACT_FORM:
		domLink(t, "Form", "action", "forms", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_INPUT:
		formControlJS(t);
		if (t->itype == INP_TA)
			establish_inner(t->jv, t->value, 0, true);
		break;

	case TAGACT_OPTION:
		optionJS(t);
		break;

	case TAGACT_A:
		domLink(t, "Anchor", "href", "anchors", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_HEAD:
		domLink(t, "Head", 0, "heads", cw->docobj, 0);
		break;

	case TAGACT_BODY:
		domLink(t, "Body", 0, "bodies", cw->docobj, 0);
		set_onhandlers(t);
		break;

	case TAGACT_TABLE:
		domLink(t, "Table", 0, "tables", cw->docobj, 0);
/* create the array of rows under the table */
		instantiate_array(t->jv, "rows");
		break;

	case TAGACT_TR:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Trow", 0, "rows", above->jv, 0);
			instantiate_array(t->jv, "cells");
		}
		break;

	case TAGACT_TD:
		if ((above = t->controller) && above->jv) {
			domLink(t, "Cell", 0, "cells", above->jv, 0);
			establish_inner(t->jv, t->innerHTML, 0, false);
		}
		break;

	case TAGACT_DIV:
		domLink(t, "Div", 0, "divs", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	case TAGACT_SPAN:
	case TAGACT_SUB:
	case TAGACT_SUP:
	case TAGACT_OVB:
		domLink(t, "Span", 0, "spans", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	case TAGACT_AREA:
		domLink(t, "Area", "href", "areas", cw->docobj, 0);
		break;

	case TAGACT_FRAME:
		domLink(t, "Frame", "src", "frames", cw->winobj, 0);
		break;

	case TAGACT_IMAGE:
		domLink(t, "Image", "src", "images", cw->docobj, 0);
		break;

	case TAGACT_P:
		domLink(t, "P", 0, "paragraphs", cw->docobj, 0);
		establish_inner(t->jv, t->innerHTML, 0, false);
		break;

	case TAGACT_TITLE:
		if (cw->ft)
			set_property_string(cw->docobj, "title", cw->ft);
		break;

	}			/* switch */

/* js tree mirrors the dom tree. */
	if (t->jv && t->parent && t->parent->jv)
		run_function_onearg(t->parent->jv, "apch$", t->jv);

/* head and body link to document */
	if (t->jv && !t->parent &&
	    (action == TAGACT_HEAD || action == TAGACT_BODY))
		run_function_onearg(cw->docobj, "apch$", t->jv);

}				/* jsNode */

/* decorate the tree of nodes with js objects */
void decorate(int start)
{
	traverse_callback = jsNode;
	traverseAll(start);
}				/* decorate */
